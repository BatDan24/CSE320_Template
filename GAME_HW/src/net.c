#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "net.h"

/* TODO bodies are placeholders. Implement the contracts in net.h. */

int open_listenfd(const char *port)
{
    // TODO: getaddrinfo() for a passive IPv4 TCP socket, then socket(),
    // SO_REUSEADDR, bind(), and listen(LISTENQ).
    (void)port;
    return -1;
}

int open_clientfd(const char *host, const char *port)
{
    // TODO: getaddrinfo() for host:port, then socket() and connect().
    (void)host;
    (void)port;
    return -1;
}

void rio_readinitb(rio_t *rp, int fd)
{
    // TODO: Associate fd with an empty read buffer.
    (void)rp;
    (void)fd;
}

ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    // TODO: Read one line through the internal buffer, retrying on EINTR.
    (void)rp;
    (void)usrbuf;
    (void)maxlen;
    return -1;
}

ssize_t rio_writen(int fd, const void *usrbuf, size_t n)
{
    // TODO: Write all n bytes, retrying after short writes and EINTR.
    (void)fd;
    (void)usrbuf;
    (void)n;
    return -1;
}
