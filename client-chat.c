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

// Function to find the suitable address info
struct addrinfo *findAddressInfo(struct addrinfo *servinfo)
{
    struct addrinfo *p;

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        int sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
        {
            perror("listener: socket");
            continue;
        }
        close(sockfd); // Close socket as we're not binding here
        break;
    }

    return p; // Return the chosen address info
}

int main(int argc, char *argv[])
{
    int sockfd;
    int status;
    ssize_t bytesRecv;
    // int bindResult;

    /* test arg number */
    if (argc != 2)
    {
        usage(argv[0]);
    }

    /* convert and check port number */
    int portNumber = atoi(argv[1]);
    checkPortNumber(portNumber);

    /* Initiate structure*/
    struct addrinfo *serverInfo, *p; // p pointer to loop through the result.
    struct addrinfo hints = {0};
    hints.ai_addr = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE | AI_V4MAPPED;

    (void)p;

    if ((status = getaddrinfo(NULL, argv[1], &hints, &serverInfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    /* create socket */
    struct addrinfo *chosen = findAddressInfo(serverInfo); // Get the chosen address info

    if (chosen == NULL)
    {
        fprintf(stderr, "Failed to find suitable address info\n");
        freeaddrinfo(serverInfo);
        exit(EXIT_FAILURE);
    }

    sockfd = socket(chosen->ai_family, chosen->ai_socktype, chosen->ai_protocol);

    if (sockfd == -1)
    {
        perror("listener: socket");
        freeaddrinfo(serverInfo);
        exit(EXIT_FAILURE);
    }

    // CHECK(sockfd = socket(AF_INET6, SOCK_DGRAM, 0));

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
    /* We bind the socket to the port number passed*/
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0)
    {
        if (errno == EADDRINUSE)
        {
            // printf("L'adresse IP et le port sont déjà utilisés par un autre processus.\n");
            // Gérer l'erreur ici...
            // Action : Send /HELO.
            CHECK(sendto(sockfd, "/HELO", 5, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)));
        }
        else
        {
            perror("bind");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        /*
        Event: recv / HELO
        Action : print remote addr and port
        */
        int waiting = 1;
        // printf("Waiting...\n");
        while (waiting)
        {
            CHECK(bytesRecv = recvfrom(sockfd, buffer, MAX_MSG_LEN - 1, 0,
                                       (struct sockaddr *)&clientStorage, &clientLen));

            buffer[bytesRecv] = '\0'; // Null-terminate the received message
            // printf("%s\n", buffer);
            // Check if the received message is "/HELO"
            if (strncmp(buffer, "/HELO", 5) == 0)
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
            // memset(message, 0, MAX_MSG_LEN);
            fgets(message, MAX_MSG_LEN, stdin);
            // printf("Sending: %s", message);

            // Implement sending logic here using sendto()
            if (strncmp(message, "/QUIT", 5) == 0)
            {
                // Envoyer la commande /QUIT au serveur ou à une adresse spécifique
                struct sockaddr_in6 server_quit_addr = {0};
                server_quit_addr.sin6_family = AF_INET6;
                server_quit_addr.sin6_port = PORT(portNumber);

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

                // Free memory.
                freeaddrinfo(dstAddrLst);
            }
        }

        if (fds[1].revents & POLLIN)
        {
            memset(message, 0, MAX_MSG_LEN);
            CHECK(bytesRecv = recvfrom(sockfd, message, MAX_MSG_LEN, 0,
                                       (struct sockaddr *)&clientStorage, &clientLen));

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
                    printf("%s", message);
                }
            }
        }
    }

    /* close socket */
    CHECK(close(sockfd));

    /* free memory */
    freeaddrinfo(serverInfo);

    return 0;
}
