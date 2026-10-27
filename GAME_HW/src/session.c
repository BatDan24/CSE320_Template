#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "debug.h"
#include "log.h"
#include "net.h"
#include "protocol.h"
#include "server.h"

/* TODO bodies are placeholders. Implement the contracts in server.h and the
 * session description in the README. You will also need a sender thread
 * routine and one handler per command; add them as static functions here.
 */

void *worker_thread(void *vargp)
{
    // TODO: Repeatedly take a connfd from the sbuf and serve it; return
    // when given -1.
    (void)vargp;
    return NULL;
}

void serve_client(server_t *s, int connfd)
{
    // TODO: Set up the session, register it, start its sender, read and
    // execute commands until EOF/QUIT, then tear everything down in order.
    (void)s;
    close(connfd);
}
