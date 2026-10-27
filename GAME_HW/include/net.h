#ifndef NET_H
#define NET_H

#include <sys/types.h>

/*
 * Socket helpers and Robust I/O (rio), modeled on the textbook's versions.
 */

#define LISTENQ 1024     /* Second argument to listen() */
#define RIO_BUFSIZE 8192 /* Size of the rio read buffer */

/*
 * Open a TCP socket listening on the given port (IPv4, all interfaces).
 * Returns the listening descriptor, or -1 on error.
 */
int open_listenfd(const char *port);

/*
 * Open a TCP connection to host:port.
 * Returns the connected descriptor, or -1 on error.
 */
int open_clientfd(const char *host, const char *port);

/* Buffered reader state for one descriptor. Not shared between threads. */
typedef struct {
    int rio_fd;                /* Descriptor for this internal buffer */
    ssize_t rio_cnt;           /* Unread bytes in internal buffer */
    char *rio_bufptr;          /* Next unread byte in internal buffer */
    char rio_buf[RIO_BUFSIZE]; /* Internal buffer */
} rio_t;

/* Associate descriptor fd with the read buffer rp. */
void rio_readinitb(rio_t *rp, int fd);

/*
 * Read one text line (including the '\n', if any) into usrbuf and
 * NUL-terminate it. At most maxlen-1 bytes are stored; a longer line is
 * returned in pieces.
 * Returns the number of bytes stored, 0 on EOF before any byte was read,
 * or -1 on error.
 */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);

/*
 * Write exactly n bytes from usrbuf to fd, retrying after short writes and
 * interrupted system calls. Returns n on success or -1 on error.
 */
ssize_t rio_writen(int fd, const void *usrbuf, size_t n);

#endif /* NET_H */
