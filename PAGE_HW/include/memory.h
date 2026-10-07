#ifndef SF_VM_MEMORY_H
#define SF_VM_MEMORY_H

#define SF_PAGE_SIZE 4096
#define SF_USER_VA_BASE SF_PAGE_SIZE

/* Override at build time to experiment with a larger physical memory. */
#ifndef SF_PHYS_MEM_SIZE
#define SF_PHYS_MEM_SIZE (4 * 1024 * 1024)
#endif

#define SF_NUM_FRAME (SF_PHYS_MEM_SIZE / SF_PAGE_SIZE)

_Static_assert(SF_PHYS_MEM_SIZE >= SF_PAGE_SIZE,
               "physical memory must contain at least one frame");
_Static_assert(SF_PHYS_MEM_SIZE % SF_PAGE_SIZE == 0,
               "physical memory must contain whole frames");

#endif
