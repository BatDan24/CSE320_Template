#ifndef SF_VM_FRAME_H
#define SF_VM_FRAME_H

#include <stdbool.h>
#include <stddef.h>

#include "memory.h"
#include "sf_types.h"

#define SF_INVALID_FRAME UINT64_MAX

/* Per-instance metadata budget, not additional guest physical memory. Backend
 * layouts are private; this budget does not prescribe an allocator algorithm.
 */
#define SF_FRAME_ALLOCATOR_STATE_SIZE (128 * (size_t)SF_NUM_FRAME)

typedef struct {
    unsigned char state[SF_FRAME_ALLOCATOR_STATE_SIZE];
} sf_frame_allocator_t;

/* Reset ownership: every frame, including frame 0, starts free.
 * Do not reset an allocator while live VM mappings reference its frames.
 */
void sf_frame_allocator_init(sf_frame_allocator_t *allocator);

/* Allocate the lowest free frame ID. On exhaustion, leave *out_frame unchanged.
 * This module manages ownership only; it never reads or clears physical bytes.
 * Pointer arguments must be non-NULL, and the allocator must be initialized.
 */
bool sf_frame_allocator_alloc(
    sf_frame_allocator_t *allocator, sf_frame_t *out_frame
);

/* Invalid frame IDs and already-free frames return false without changes. */
bool sf_frame_allocator_free(sf_frame_allocator_t *allocator, sf_frame_t frame);

/* Inspect ownership without changing allocator state; invalid IDs are false. */
bool sf_frame_allocator_is_allocated(
    const sf_frame_allocator_t *allocator, sf_frame_t frame
);

/* Find the lowest frame ID starting a free run of at least frame_count frames.
 * This query does not allocate. Zero, oversized, or unsatisfiable requests
 * return false and leave *out_first unchanged. Pointers must be non-NULL and
 * the allocator must be initialized.
 */
bool sf_frame_allocator_find_free_run(
    const sf_frame_allocator_t *allocator,
    size_t frame_count,
    sf_frame_t *out_first
);

#endif
