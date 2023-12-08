#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <poll.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#define CHECK(op)               \
    do                          \
    {                           \
        if ((op) == -1)         \
        {                       \
            perror(#op);        \
            exit(EXIT_FAILURE); \
        }                       \
    } while (0)

#define PORT(p) htons(p)

void checkPortNumber(int portNumber)
{
    if (portNumber < 10000 || portNumber > 65000)
    {
        fprintf(stderr, "Invalid port number.\n");
        exit(EXIT_FAILURE);
    }
}

void usage(char *programName)
{
    fprintf(stderr, "usage: %s server_name\n", programName);
    exit(EXIT_FAILURE);
}

#ifdef BIN

// Structure representing the binary message
struct BinaryMessage
{
    uint8_t messageType;
};

// Function to send a binary message for /HELO or /QUIT
void sendBinaryMessage(int sockfd, uint8_t messageType,
                       struct sockaddr *destAddr, socklen_t destAddrLen)
{
    struct BinaryMessage binaryMsgToSend;
    binaryMsgToSend.messageType = messageType;

    // Sending the encoded binary message over UDP using sendto
    CHECK(sendto(sockfd, &binaryMsgToSend, sizeof(struct BinaryMessage), 0,
                 destAddr, destAddrLen));
}

// Function to receive a binary message
int receiveBinaryMessage(int sockfd, struct BinaryMessage *binaryMsg,
                         struct sockaddr *clientAddr, socklen_t *clientLen)
{
    ssize_t bytesRecv;
    CHECK(bytesRecv = recvfrom(sockfd, binaryMsg, sizeof(struct BinaryMessage), 0,
                               clientAddr, clientLen));

    return bytesRecv;
}

#endif

#define MAX_MSG_LEN 1024

int main(int argc, char *argv[])
{
    int sockfd;
    int status;
    ssize_t bytesRecv;
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    // int bindResult;

    /* test arg number */
    if (argc != 2)
    {
        usage(argv[0]);
    }

    /* convert and check port number */
    int portNumber = atoi(argv[1]);
    checkPortNumber(portNumber);

    /* create socket */
    CHECK(sockfd = socket(AF_INET6, SOCK_DGRAM, 0));

    /* set dual stack socket */
    // Désactivation de l'option IPV6_V6ONLY pour permettre IPv4 et IPv6
    int value = 0;
    CHECK(setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &value, sizeof value));

    /* set local addr */
    struct sockaddr_in6 serverAddr = {0};
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_port = PORT(portNumber);
    serverAddr.sin6_addr = in6addr_any; // On utilise toutes les addresses disponibles.

    struct sockaddr_storage clientStorage;
    socklen_t clientLen = sizeof(clientStorage);

    char buffer[MAX_MSG_LEN];
    (void)buffer;
    /* check if a client is already present */
    /* We bind the socket to the port number passed*/
    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof serverAddr) < 0)
    {
        if (errno == EADDRINUSE)
        {
            // printf("L'adresse IP et le port sont déjà utilisés par un autre processus.\n");
            // Gérer l'erreur ici...
            // Action : Send /HELO.
            // Sending /HELO to the existing user occupying the port
            printf("I'm a client sending /HELO to the server to initiate a connection\n");
#ifdef BIN
            sendBinaryMessage(sockfd, 0x01, (struct sockaddr *)&serverAddr, sizeof serverAddr);
            printf("Send binary msg\n");
#else
            printf("here without bin.\n");
            struct sockaddr_in6 existingUserAddr = serverAddr; // Store existing user address otherwise it won't work.
            CHECK(sendto(sockfd, "/HELO", 5, 0,
                         (struct sockaddr *)&existingUserAddr,
                         sizeof(existingUserAddr)));
#endif
        }
        else
        {
            perror("bind");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        printf("I'm the program server, waiting for connection...\n");
        /*
        Event: recv / HELO
        Action : print remote addr and port
        */
        int waiting = 1;
        // printf("Waiting...\n");
        while (waiting)
        {
#ifdef BIN
            struct BinaryMessage receivedBinaryMsg;
            bytesRecv = receiveBinaryMessage(sockfd, &receivedBinaryMsg,
                                             (struct sockaddr *)&clientStorage,
                                             &clientLen);
            if (bytesRecv > 0)
            {
                if (receivedBinaryMsg.messageType == 0x01)
                {
                    if ((status = getnameinfo((struct sockaddr *)&clientStorage,
                                              clientLen, host, NI_MAXHOST, service,
                                              NI_MAXSERV,
                                              NI_NUMERICHOST | NI_NUMERICSERV)) != 0)
                    {
                        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(status));
                    }
                    else
                    {
                        printf("%s %s", host, service);
                        waiting = 0;
                    }
                }
            }
            printf("Received binary msg.\n");
#else
            CHECK(bytesRecv = recvfrom(sockfd, buffer, MAX_MSG_LEN - 1, 0,
                                       (struct sockaddr *)&clientStorage,
                                       &clientLen));

            buffer[bytesRecv] = '\0'; // Null-terminate the received message
            // printf("%s\n", buffer);
            // Check if the received message is "/HELO"
            if (strncmp(buffer, "/HELO", 5) == 0)
            {
                if ((status = getnameinfo((struct sockaddr *)&clientStorage,
                                          clientLen, host, NI_MAXHOST, service,
                                          NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV)) != 0)
                {
                    fprintf(stderr, "getnameinfo: %s\n", gai_strerror(status));
                }
                else
                {
                    printf("%s %s", host, service);
                    waiting = 0;
                }
            }
#endif
            fflush(stdout);
        }
    }

    /* prepare struct pollfd with stdin and socket for incoming data */
    struct pollfd fds[2];
    memset(fds, 0, sizeof(fds));

    // Pour écouter dans stdin
    fds[0].fd = STDIN_FILENO; // stdin
    fds[0].events = POLLIN;

    // Pour écouter dans le socket.
    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    char message[MAX_MSG_LEN] = {0};

    // printf("Connected..\n");
    /* main loop */
    int running = 1;
    while (running)
    {
        int activity;
        CHECK(activity = poll(fds, 2, -1)); // -1 une attente indéfini.

        if (fds[0].revents & POLLIN)
        {
            // Envoyer la commande /QUIT au serveur ou à une adresse
            // spécifique

            fgets(message, MAX_MSG_LEN, stdin);
            // printf("Sending: %s", message);

            // Implement sending logic here using sendto()
            if (strncmp(message, "/QUIT", 5) == 0)
            {

                CHECK(sendto(sockfd, message, strlen(message), 0,
                             (struct sockaddr *)&clientStorage,
                             clientLen));

                running = 0; // Quit the loop upon /QUIT command
            }
            else
            {
                // Envoi de message au client déjà connecté en utilisant
                // l'adresse du server existant.
                // printf("Sending from here... ");
                CHECK(sendto(sockfd, message, strlen(message), 0,
                             (struct sockaddr *)&clientStorage,
                             clientLen));
            }
        }

        if (fds[1].revents & POLLIN)
        {
            CHECK(bytesRecv = recvfrom(sockfd, message, MAX_MSG_LEN, 0,
                                       (struct sockaddr *)&clientStorage,
                                       &clientLen));

            if (bytesRecv > 0)
            {
                message[bytesRecv] = '\0';

                // Implement message processing logic here
                if (strncmp(message, "/QUIT", 5) == 0)
                {
                    running = 0;
                }
                else
                {
                    // Action print DATA
                    // printf("REC from the socket\n");
                    // printf("Received from sock...  : ");
                    printf("%s", message);
                }
            }
        }
    }

    /* close socket */
    CHECK(close(sockfd));

    /* free memory */
    return 0;
}
