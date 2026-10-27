#include <stdlib.h>

#include "msgq.h"

/* TODO bodies are placeholders. Implement the contracts in msgq.h. */

void msgq_init(msgq_t *q, int n)
{
    // TODO: Like sbuf_init, for n message pointers.
    (void)q;
    (void)n;
}

void msgq_deinit(msgq_t *q)
{
    // TODO: free() every message still queued, then release the queue.
    (void)q;
}

void msgq_insert(msgq_t *q, char *msg)
{
    // TODO: Blocking insert at the rear. msg may be NULL (poison pill).
    (void)q;
    (void)msg;
}

int msgq_try_insert(msgq_t *q, char *msg)
{
    // TODO: Insert only if a slot is free right now (P_try). Return 0 on
    // success; return -1 if full, leaving msg owned by the caller.
    (void)q;
    (void)msg;
    return -1;
}

char *msgq_remove(msgq_t *q)
{
    // TODO: Blocking remove from the front. The caller frees the message.
    (void)q;
    return NULL;
}
