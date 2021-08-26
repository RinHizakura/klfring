#include <linux/module.h>
#include <linux/slab.h>

#include "lfring.h"

#define CACHE_LINE 64 /* FIXME: should be configurable */

#define SUPPORTED_FLAGS \
    (LFRING_FLAG_SP | LFRING_FLAG_MP | LFRING_FLAG_SC | LFRING_FLAG_MC)

typedef uintptr_t ringidx_t;
struct element {
    void *ptr;
    uintptr_t idx;
};

struct lfring {
    ringidx_t head;
    ringidx_t tail __aligned(CACHE_LINE);
    uint32_t mask;
    uint32_t flags;
    struct element ring[] __aligned(CACHE_LINE);
} __aligned(CACHE_LINE);

lfring_t *lfring_alloc(uint32_t n_elems, uint32_t flags)
{
    unsigned long ringsz;
    size_t nbytes;
    lfring_t *lfr;
    ringidx_t i;

    ringsz = roundup_pow_of_two(n_elems);
    if (n_elems == 0 || ringsz == 0 || ringsz > 0x80000000) {
        return NULL;
    }
    if ((flags & ~SUPPORTED_FLAGS) != 0) {
        return NULL;
    }

    nbytes = sizeof(lfring_t) + ringsz * sizeof(struct element);
    /* FIXME: The allocated object address of kmalloc may not align
     * the cache size under some cases. It could harm the performace. */
    lfr = kmalloc(ALIGN(nbytes, CACHE_LINE), GFP_KERNEL);

    if (!lfr)
        return NULL;

    lfr->head = 0, lfr->tail = 0;
    lfr->mask = ringsz - 1;
    lfr->flags = flags;
    for (i = 0; i < ringsz; i++) {
        lfr->ring[i].ptr = NULL;
        lfr->ring[i].idx = i - ringsz;
    }
    return lfr;
}

void lfring_free(lfring_t *lfr)
{
    if (!lfr)
        return;

    kfree(lfr);
}
