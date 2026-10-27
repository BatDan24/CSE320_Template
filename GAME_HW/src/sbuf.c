#include "sbuf.h"

/* TODO bodies are placeholders. Implement the lecture's sbuf package. */

void sbuf_init(sbuf_t *sp, int n)
{
    // TODO: Allocate n slots and initialize the mutex, slots, and items
    // semaphores.
    (void)sp;
    (void)n;
}

void sbuf_deinit(sbuf_t *sp)
{
    // TODO: Release the buffer and the semaphores.
    (void)sp;
}

void sbuf_insert(sbuf_t *sp, int item)
{
    // TODO: Wait for a free slot, insert item at the rear, announce an item.
    (void)sp;
    (void)item;
}

int sbuf_remove(sbuf_t *sp)
{
    // TODO: Wait for an item, remove it from the front, announce a free slot.
    (void)sp;
    return -1;
}
