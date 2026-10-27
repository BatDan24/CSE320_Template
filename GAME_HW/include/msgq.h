#ifndef MSGQ_H
#define MSGQ_H

#include "sync.h"

/*
 * Bounded FIFO of heap-allocated strings: the "outbox" of one client.
 *
 * Every connected client has exactly one sender thread that removes
 * messages from its outbox and writes them to the socket. Any thread may
 * insert (the client's own worker for replies, the tick thread for game
 * frames, other workers for chat). Because only the sender ever writes to
 * the socket, messages from different threads can never interleave on the
 * wire.
 *
 * Ownership: a message inserted into the queue belongs to the queue. The
 * thread that removes it must free() it. A NULL message is a sentinel that
 * tells the consumer to stop ("poison pill").
 */
typedef struct {
    char **buf;  /* Buffer array of message pointers */
    int n;       /* Maximum number of slots */
    int front;   /* buf[(front+1)%n] is first item */
    int rear;    /* buf[rear%n] is last item */
    sem_t mutex; /* Protects accesses to buf */
    sem_t slots; /* Counts available slots */
    sem_t items; /* Counts available items */
} msgq_t;

/* Create an empty queue with room for n messages. */
void msgq_init(msgq_t *q, int n);
/* Free every message still in the queue, then the queue itself. */
void msgq_deinit(msgq_t *q);
/* Insert msg at the rear, blocking while the queue is full. */
void msgq_insert(msgq_t *q, char *msg);
/*
 * Insert msg at the rear only if there is a free slot right now.
 * Returns 0 on success. Returns -1 if the queue is full, in which case the
 * caller still owns msg (and must free it).
 */
int msgq_try_insert(msgq_t *q, char *msg);
/* Remove and return the first message, blocking while the queue is empty. */
char *msgq_remove(msgq_t *q);

#endif /* MSGQ_H */
