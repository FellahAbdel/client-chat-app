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

#define MAX_MSG_LEN 1024

void sendMessage(int socket, struct addrinfo *destAddr, char *message)
{
    size_t len = strlen(message);
    ssize_t bytesSent = sendto(socket, message, len, 0, destAddr->ai_addr, destAddr->ai_addrlen);
    CHECK(bytesSent);
}

int compareString(char *msg1, char *msg2)
{
    size_t len = strlen(msg1);
    if (len > 0 && msg1[len - 1] == '\n')
    {
        msg1[len - 1] = '\0';
    }

    return strcmp(msg1, msg2) == 0;
}

int main(int argc, char *argv[])
{
    int sockfd;
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
    struct sockaddr_in6 server_addr = {0};
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = PORT(portNumber);
    server_addr.sin6_addr = in6addr_any; // On utilise toutes les addresses disponibles.

    struct sockaddr_storage clientStorage;
    socklen_t clientLen = sizeof(clientStorage);

    char buffer[MAX_MSG_LEN];
    /* check if a client is already present */
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0)
    {
        if (errno == EADDRINUSE)
        {
            // printf("L'adresse IP et le port sont déjà utilisés par un autre processus.\n");
            // Gérer l'erreur ici...
            // Action : Send /HELO.
            CHECK(sendto(sockfd, "/HELO\n", 6, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)));
        }
        else
        {
            perror("bind");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        //* Il y a aucun client sur le port.
        /*
        Event: recv / HELO
        Action : print remote addr and port
        */
        int waiting = 1;
        // printf("Waiting...\n");
        while (waiting)
        {
            ssize_t bytesRecv;
            CHECK(bytesRecv = recvfrom(sockfd, buffer, MAX_MSG_LEN, 0, (struct sockaddr *)&clientStorage, &clientLen));

            buffer[bytesRecv] = '\0'; // Null-terminate the received message
            // printf("%s", buffer);
            // Check if the received message is "/HELO"
            if (strcmp(buffer, "/HELO\n") == 0)
            {
                char host[NI_MAXHOST];
                char service[NI_MAXSERV];

                int result = getnameinfo((struct sockaddr *)&clientStorage, clientLen,
                                         host, NI_MAXHOST, service, NI_MAXSERV,
                                         NI_NUMERICHOST | NI_NUMERICSERV);
                if (result != 0)
                {
                    fprintf(stderr, "getnameinfo: %s\n", gai_strerror(result));
                }
                else
                {
                    printf("%s %s", host, service);
                    waiting = 0;
                }
            }
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

    char message[MAX_MSG_LEN];

    // printf("Connected..\n");
    /* main loop */
    int running = 1;
    while (running)
    {
        int activity;
        CHECK(activity = poll(fds, 2, -1)); // -1 une attente indéfini.

        if (fds[0].revents & POLLIN)
        {
            memset(message, 0, MAX_MSG_LEN);
            fgets(message, MAX_MSG_LEN, stdin);
            // printf("Sending: %s", message);

            // Implement sending logic here using sendto()
            if (strcmp(message, "/QUIT") == 0)
            {
                // Envoyer la commande /QUIT au serveur ou à une adresse spécifique
                struct sockaddr_in6 server_quit_addr;
                memset(&server_quit_addr, 0, sizeof(server_quit_addr));
                server_quit_addr.sin6_family = AF_INET6;
                server_quit_addr.sin6_port = PORT(portNumber); // Remplacez SERVER_QUIT_PORT par le port du serveur

                CHECK(sendto(sockfd, message, strlen(message), 0,
                             (struct sockaddr *)&server_quit_addr, sizeof(server_quit_addr)));
                running = 0; // Quit the loop upon /QUIT command
            }
            else
            {
                // Sending data to the connected client
                struct addrinfo *dstAddrLst;
                struct addrinfo hints = {0};
                hints.ai_family = AF_INET6;
                hints.ai_socktype = SOCK_DGRAM;
                hints.ai_protocol = IPPROTO_UDP;

                int res = getaddrinfo(NULL, argv[1], &hints, &dstAddrLst);
                if (res != 0)
                {
                    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
                    exit(EXIT_FAILURE);
                }

                CHECK(sendto(sockfd, message, strlen(message), 0,
                             dstAddrLst->ai_addr, dstAddrLst->ai_addrlen));
            }
        }

        if (fds[1].revents & POLLIN)
        {
            memset(message, 0, MAX_MSG_LEN);
            ssize_t bytes_recv = recvfrom(sockfd, message, MAX_MSG_LEN, 0,
                                          (struct sockaddr *)&clientStorage, &clientLen);

            if (bytes_recv > 0)
            {
                message[bytes_recv] = '\0';
                // Implement message processing logic here
                if (strcmp(message, "/QUIT\n") == 0)
                {
                    running = 0;
                }
                else
                {
                    // Action print DATA
                    // printf("REC from the socket\n");
                    printf("%s", message);
                }
            }
        }
    }

    /* close socket */
    CHECK(sockfd);

    /* free memory */

    return 0;
}
