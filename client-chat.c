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

#define HELO ((uint8_t)0x01)
#define QUIT ((uint8_t)0x02)

#endif

void displayClientInfo(struct sockaddr *clientAddr, socklen_t *clientLen,
                       char *hostNameIP, char *portNumber)
{
    int status;

    if ((status = getnameinfo(clientAddr, *clientLen, hostNameIP, NI_MAXHOST,
                              portNumber, NI_MAXSERV,
                              NI_NUMERICHOST | NI_NUMERICSERV)) != 0)
    {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    printf("%s %s\n", hostNameIP, portNumber);
}

#define MAX_MSG_LEN 1024

int main(int argc, char *argv[])
{
    int sockfd;
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
    serverAddr.sin6_addr = in6addr_any; // use my IPv6 address.

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
            struct sockaddr_in6 existingUserAddr = serverAddr; // Store existing user address otherwise it won't work.
#ifdef BIN
            //* Sending /HELO : 0x01 message.
            u_int8_t binBuff[1] = {HELO};

            CHECK(sendto(sockfd, binBuff, 1,
                         0, (struct sockaddr *)&existingUserAddr,
                         sizeof existingUserAddr));
            printf("Send binary msg\n");
#else
            printf("here without bin.\n");
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
            // To store the binary message.
            uint8_t binBuff[1];

            CHECK(bytesRecv = recvfrom(sockfd, binBuff,
                                       1, 0, (struct sockaddr *)&clientStorage,
                                       &clientLen));

            // The message is well received, if it's the /HELO == 0x01
            if (binBuff[0] == HELO)
            {
                // Then we display the client information.
                displayClientInfo((struct sockaddr *)&clientStorage, &clientLen,
                                  host, service);
                waiting = 0; // We stop waiting.
            }

            printf("Received binary msg.\n");
#else
            // Receiving plaint text message.
            CHECK(bytesRecv = recvfrom(sockfd, buffer, MAX_MSG_LEN - 1, 0,
                                       (struct sockaddr *)&clientStorage,
                                       &clientLen));

            // We received at least 1 byte.
            if (bytesRecv > 0)
            {
                buffer[bytesRecv] = '\0'; // Null-terminate the received message
                // printf("%s\n", buffer);
                // Check if the received message is "/HELO"
                if (strncmp(buffer, "/HELO", 5) == 0)
                {
                    // Then we display the client information.
                    displayClientInfo((struct sockaddr *)&clientStorage, &clientLen,
                                      host, service);
                    waiting = 0; // we stop waiting.
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

        // Waiting an event from the keyboard.
        if (fds[0].revents & POLLIN)
        {
            // Envoyer la commande /QUIT au serveur ou à une adresse
            // spécifique

            fgets(message, MAX_MSG_LEN, stdin);
            // printf("Sending: %s", message);

            // Implement sending logic here using sendto()
            // Envoi de message au client déjà connecté en utilisant
            // l'adresse du server existant.
            // printf("Sending from here... ");
            // void *messageOrBinary = NULL;
            // size_t len = 0;

            // We load the data to transmit.
#ifdef BIN
            // if it's a "/QUIT" command then we used binary form.
            if (strncmp(message, "/QUIT", 5) == 0)
            {
                // We only send QUIT command in binary form which is one byte.
                message[0] = 0x02;
                message[1] = '\0'; // nul terminate byte.
            }
#endif

            // And Then we transmit the data.
            CHECK(sendto(sockfd, message, strlen(message), 0,
                         (struct sockaddr *)&clientStorage,
                         clientLen));
#ifdef BIN
            running = !(message[0] == 0x02);
#else
            running = !(strncmp(message, "/QUIT", 5) == 0);
#endif
        }

        // Wainting an event from the socket.
        if (fds[1].revents & POLLIN)
        {
            CHECK(bytesRecv = recvfrom(sockfd, message, MAX_MSG_LEN, 0,
                                       (struct sockaddr *)&clientStorage,
                                       &clientLen));

            // A message is well received.
            message[bytesRecv] = '\0';

#ifdef BIN
            if (message[0] == 0x02)
            {
                running = 0;
            }
            else
            {
                // Action : print DATA.
                printf("%s", message);
            }
#else

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

#endif
        }
    }

    /* close socket */
    CHECK(close(sockfd));

    /* free memory */
    return 0;
}
