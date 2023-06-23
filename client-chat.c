#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <poll.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define CHECK(op)   do { if ( (op) == -1) { perror (#op); exit (EXIT_FAILURE); } \
                    } while (0)

#define PORT(p) htons(p)

int main (int argc, char *argv [])
{
    /* test arg number */

    /* convert and check port number */

    /* create socket */

    /* set dual stack socket */

    /* set local addr */

    /* check if a client is already present */

    /* prepare struct pollfd with stdin and socket for incoming data */

    /* main loop */

    /* close socket */

    /* free memory */

    return 0;
}
