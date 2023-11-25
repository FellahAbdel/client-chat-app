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

    /* check if a client is already present */
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof server_addr) < 0)
    {
        if (errno == EADDRINUSE)
        {
            printf("L'adresse IP et le port sont déjà utilisés par un autre processus.\n");
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
        struct addrinfo *dstAddrResultLst;
        struct addrinfo hints = {0};

        int returnValue = getaddrinfo(NULL, argv[1], &hints, &dstAddrResultLst);
        if (returnValue != 0)
        {
            fprintf(stderr, "getaddrinfo : %s\n", gai_strerror(returnValue));
            exit(EXIT_FAILURE);
        }

        struct addrinfo *result = dstAddrResultLst;
        while (result != NULL)
        {
            void *addr;
            char ipstr[INET6_ADDRSTRLEN];

            if (result->ai_family == AF_INET)
            { // IPv4
                struct sockaddr_in *ipv4 = (struct sockaddr_in *)result->ai_addr;
                addr = &(ipv4->sin_addr);
            }
            else
            { // IPv6
                struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)result->ai_addr;
                addr = &(ipv6->sin6_addr);
            }

            // Convert the IP to a string and print it
            inet_ntop(result->ai_family, addr, ipstr, sizeof(ipstr));
            printf("Destination IP: %s\n", ipstr);

            result = result->ai_next;
        }
    }

    printf("Bind réussi.\n");
    // ici un client est présent.

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
    struct sockaddr_storage clientAddr;
    socklen_t clientAddrLen;
    (void)clientAddr;
    (void)clientAddrLen;

    /* main loop */
    int running = 1;
    while (running)
    {
        int activity = poll(fds, 2, -1); // -1 une attente indéfini.
        if (activity < 0)
        {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        if (fds[0].revents & POLLIN)
        {
            memset(message, 0, MAX_MSG_LEN);
            fgets(message, MAX_MSG_LEN, stdin);
            printf("Sending: %s", message);
            // Implement sending logic here using sendto()
        }

        break;
    }

    /* close socket */
    CHECK(sockfd);

    /* free memory */

    return 0;
}
