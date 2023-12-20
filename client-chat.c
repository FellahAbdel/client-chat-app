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
#include <stdnoreturn.h>

#include <sys/stat.h>
#include <limits.h>

#define CHECK(op)               \
    do                          \
    {                           \
        if ((op) == -1)         \
        {                       \
            perror(#op);        \
            exit(EXIT_FAILURE); \
        }                       \
    } while (0)

#define CHKN(op)          \
    do                    \
    {                     \
        if ((op) == NULL) \
            raler(#op);   \
    } while (0)

noreturn void raler(const char *msg)
{
    perror(msg);
    exit(1);
}

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

#define HELO ((uint8_t)0x01) // 1 octet pour HELO au lieu de 5 octets.
#define QUIT ((uint8_t)0x02) // 1 octet pour QUIT au lieu de 5 octets.

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
    fflush(stdout);
}

#define MAX_MSG_LEN 1024

#ifdef FILEIO

#define BUF_SIZE 2048 // Max buffer size of the data in a frame

/*A frame packet with unique id, length and data*/
struct frame_t
{
    long int ID;
    long int length;
    char data[BUF_SIZE];
};

void createFilePath(const char *fileName, char option, char *outputPath)
{
    const char *directory;

    if (option == 's')
    {
        directory = "./serverFiles/";
    }
    else
    {
        directory = "./clientFiles/";
    }

    int result = snprintf(outputPath, PATH_MAX, "%s%s", directory, fileName);
    if (result < 0 || result >= PATH_MAX)
    {
        fprintf(stderr, "Error: Failed to create file path or path length exceeded.\n");
        exit(EXIT_FAILURE);
    }
}

void usageFileIO(char *programName)
{
    fprintf(stderr, "usage: %s serverName fileName\n", programName);
    exit(EXIT_FAILURE);
}

void sendFile(int sockfd, struct sockaddr *clientStorage, socklen_t clientLen,
              const char *fileName)
{
    FILE *file = fopen(fileName, "rb"); // Ouvrir le fichier en mode lecture binaire ("rb")
    if (file == NULL)
    {
        perror("fopen");
        return;
    }

    char buffer[MAX_MSG_LEN];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, MAX_MSG_LEN, file)) > 0)
    {
        CHECK(sendto(sockfd, buffer, bytesRead, 0, clientStorage, clientLen));
    }

    fclose(file);
}
#endif

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

#ifdef FILEIO
    char filePath[PATH_MAX];
    char fileName[FILENAME_MAX] = {0};
    struct timeval t_out = {0, 0};
    // struct stat st;
    struct frame_t frame;
    char ack_send[4] = "ACK";

    ssize_t serverLen = 0;
    // off_t f_size = 0;

    FILE *fptr;

    /*Clear all the data buffer and structure*/
    memset(ack_send, 0, sizeof(ack_send));

#endif
    // #ifdef FILEIO
    //     if (argc != 3)
    //     {
    //         usageFileIO(argv[0]);
    //         exit(EXIT_FAILURE);
    //     }
    // #endif

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
            // printf("L'adresse IP et le port sont déjà utilisés par un autre"
            //             "processus.\n");
            // Gérer l'erreur ici...
            // Action : Send /HELO.
            // Sending /HELO to the existing user occupying the port
            printf("I'm a client sending /HELO to the server to initiate a"
                   "connection\n");

            // Store existing user address otherwise it won't work.
            struct sockaddr_in6 existingUserAddr = serverAddr;
            size_t size;
#ifdef BIN
            uint8_t binBuff[1] = {HELO};
            const void *buff = binBuff;
            size = 1; // 1 octet.
#else
            const char *messageBuff = "/HELO";
            const void *buff = messageBuff;
            size = 5; // 5 caractères
#endif
            CHECK(sendto(sockfd, buff, size, 0,
                         (struct sockaddr *)&existingUserAddr,
                         sizeof(existingUserAddr)));
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
            void *genericPtr = NULL;

            // Allocate memory based on whether BIN is defined or not
#ifdef BIN
            // Allocate memory for uint8_t buffer
            uint8_t *binBuff;
            CHKN(binBuff = malloc(sizeof(uint8_t)));

            // Set genericPtr to point to binBuff
            genericPtr = (void *)binBuff;
#else
            // Allocate memory for char buffer
            char *receivedText;
            CHKN(receivedText = malloc(MAX_MSG_LEN));

            // Set genericPtr to point to receivedText
            genericPtr = (void *)receivedText;

#endif
            CHECK(bytesRecv = recvfrom(sockfd, genericPtr, MAX_MSG_LEN - 1, 0,
                                       (struct sockaddr *)&clientStorage,
                                       &clientLen));
#ifdef BIN
            if (*(uint8_t *)genericPtr == HELO)
            {
                displayClientInfo((struct sockaddr *)&clientStorage, &clientLen,
                                  host, service);
                waiting = 0; // Stop waiting
            }

            printf("Received binary msg.\n");
            fflush(stdout);

            free(genericPtr); // Free allocated memory
#else
            // A message is well received.
            ((char *)genericPtr)[bytesRecv] = '\0'; // Null-terminate the
                                                    // received message

            // Check if the received message is "/HELO"
            if (strncmp((char *)genericPtr, "/HELO", 5) == 0)
            {
                // We display client information.
                displayClientInfo((struct sockaddr *)&clientStorage, &clientLen,
                                  host, service);
                waiting = 0; // Stop waiting
            }

            free(genericPtr); // Free allocated memory
#endif
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

            // We load the data to transmit.
#ifdef BIN
            // if it's a "/QUIT" command then we used binary form.
            if (strncmp(message, "/QUIT", 5) == 0)
            {
                // We only send QUIT command in binary form which is one byte.
                message[0] = QUIT;
                message[1] = '\0';
            }
#endif

            // And Then we transmit the data.
            CHECK(sendto(sockfd, message, strlen(message), 0,
                         (struct sockaddr *)&clientStorage,
                         clientLen));

#ifdef FILEIO
            // The client.
            if (strncmp(message, "/GET", 4) == 0)
            {
                // We get the file name.
                if (sscanf(message, "/GET %255s", fileName) != 1)
                    exit(EXIT_FAILURE);

                // We make sure there is something inside.
                if (fileName[0] != '\0')
                {
                    long int totalFrame = 0;
                    long int bytesRec = 0, i = 0;

                    t_out.tv_sec = 2;
                    // Enable the timeout option if client does not respond
                    CHECK(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
                                     (char *)&t_out, sizeof(struct timeval)));

                    // Get the total number of frame to recieve
                    CHECK(recvfrom(sockfd, &(totalFrame), sizeof(totalFrame), 0,
                                   (struct sockaddr *)&serverAddr,
                                   (socklen_t *)&serverLen));

                    // Disable the timeout option
                    t_out.tv_sec = 0;
                    CHECK(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
                                     (char *)&t_out, sizeof(struct timeval)));

                    if (totalFrame > 0)
                    {
                        CHECK(sendto(sockfd, &(totalFrame), sizeof(totalFrame),
                                     0, (struct sockaddr *)&clientStorage,
                                     sizeof(clientStorage)));
                        printf("----> %ld\n", totalFrame);

                        // We construct the file path.
                        memset(filePath, 0, sizeof(filePath));
                        createFilePath(fileName, 'c', filePath);
                        printf("path : %s\n", filePath);
                        CHKN(fptr = fopen(filePath, "wb")); // open the file in write mode

                        /*Recieve all the frames and send the acknowledgement sequentially*/
                        for (i = 1; i <= totalFrame; i++)
                        {
                            memset(&frame, 0, sizeof(frame));

                            // Recieve the frame
                            CHECK(recvfrom(sockfd, &(frame), sizeof(frame), 0,
                                           (struct sockaddr *)&serverAddr,
                                           (socklen_t *)&serverLen));

                            // Send the ack
                            CHECK(sendto(sockfd, &(frame.ID), sizeof(frame.ID),
                                         0, (struct sockaddr *)&clientStorage,
                                         sizeof(clientStorage)));

                            /*Drop the repeated frame*/
                            if ((frame.ID < i) || (frame.ID > i))
                                i--;
                            else
                            {
                                fwrite(frame.data, 1, frame.length, fptr); /*Write the recieved data to the file*/
                                printf("frame.ID ---> %ld	frame.length ---> %ld\n", frame.ID, frame.length);
                                bytesRec += frame.length;
                            }

                            if (i == totalFrame)
                            {
                                printf("File recieved\n");
                            }
                        }
                        printf("Total bytes recieved ---> %ld\n", bytesRec);
                        fclose(fptr);
                    }
                    else
                    {
                        printf("File is empty\n");
                    }
                }
            }
#endif
#ifdef BIN
            // running is 0 if message is equal to QUIT, so we quit the loop.
            running = !(message[0] == QUIT);
#else
            // Same here folks.
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
            // if message is QUIT we quit the loop else print the received the
            // received message.
            message[0] == QUIT ? running = 0 : printf("%s", message);
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

                // if FILEIO is defined we check if the client is requesting a
                // file by typing this command /GET fileName.
#ifdef FILEIO
                // if it's a get command.
                if (strncmp(message, "/GET", 4) == 0)
                {
                    // then we get the file name from the message.
                    if (sscanf(message, "/GET %255s", fileName) != 1)
                    {
                        exit(EXIT_FAILURE);
                    }

                    // We make sure there is something in the buffer.
                    if (fileName[0] != '\0')
                    {
                        // And finally, we send the file.
                        sendFile(sockfd, (struct sockaddr *)&clientStorage,
                                 clientLen, fileName);
                    }
                }
#endif // FILEIO.
            }

#endif // Basic version.
        }
    }

    /* close socket */
    CHECK(close(sockfd));

    /* free memory */
    return 0;
}
