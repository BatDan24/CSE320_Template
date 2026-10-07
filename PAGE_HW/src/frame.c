#include "frame.h"

/* Keep all persistent metadata in allocator->state.
 * Its capacity is SF_FRAME_ALLOCATOR_STATE_SIZE bytes. Do not access guest bytes.
 * Required pointers are non-NULL; initialize the allocator before other calls.
 */

void sf_frame_allocator_init(sf_frame_allocator_t *allocator)
{
    // TODO: Initialize metadata so every frame, including frame 0, is free.
    // Reset any previous ownership state.
    (void)allocator;
}

bool sf_frame_allocator_alloc(
    sf_frame_allocator_t *allocator, sf_frame_t *out_frame
)
{
    // TODO: Allocate the lowest free frame, write its ID to *out_frame, and return true.
    // If none is free, return false without changing state or *out_frame.
    (void)allocator;
    (void)out_frame;
    return false;
}

bool sf_frame_allocator_free(sf_frame_allocator_t *allocator, sf_frame_t frame)
{
    // TODO: Mark an allocated frame free and return true.
    // For an invalid or already-free frame, return false without changes.
    (void)allocator;
    (void)frame;
    return false;
}

bool sf_frame_allocator_is_allocated(
    const sf_frame_allocator_t *allocator, sf_frame_t frame
)
{
    // TODO: Return true if the frame is allocated, or false if free or invalid.
    // Do not change state.
    (void)allocator;
    (void)frame;
    return false;
}

bool sf_frame_allocator_find_free_run(
    const sf_frame_allocator_t *allocator, size_t frame_count,
    sf_frame_t *out_first
)
{
    // TODO: Find the lowest start of frame_count consecutive free frames.
    // Write it to *out_first and return true, without allocating the frames.
    // For zero, oversized, or unsatisfiable requests, return false.
    // Leave *out_first unchanged on failure; never change allocator state.
    (void)allocator;
    (void)frame_count;
    (void)out_first;
    return false;
}
