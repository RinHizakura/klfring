#include <linux/module.h>
#include <linux/slab.h>

#include "lfring.h"
#define CACHE_LINE 64 /* FIXME: should be configurable */

#define SUPPORTED_FLAGS \
    (LFRING_FLAG_SP | LFRING_FLAG_MP | LFRING_FLAG_SC | LFRING_FLAG_MC)

typedef atomic_long_t ringidx_t;
typedef unsigned long __ringidx_t;

struct element {
    void *ptr;
    ringidx_t idx;
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
    uint32_t i;

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

    atomic_long_set(&lfr->head, 0);
    atomic_long_set(&lfr->tail, 0);
    lfr->mask = ringsz - 1;
    lfr->flags = flags;
    for (i = 0; i < ringsz; i++) {
        lfr->ring[i].ptr = NULL;
        atomic_long_set(&lfr->ring[i].idx, i - ringsz);
    }

    return lfr;
}

void lfring_free(lfring_t *lfr)
{
    if (!lfr)
        return;

    kfree(lfr);
}

/* True if 'a' is before 'b' ('a' < 'b') in serial number arithmetic */
static inline bool before(__ringidx_t a, __ringidx_t b)
{
    return (long) (a - b) < 0;
}


static inline __ringidx_t cond_update(ringidx_t *loc, __ringidx_t neu)
{
    __ringidx_t old = atomic_long_read(loc);
    do {
        if (before(neu, old)) /* neu < old */
            return old;
        /* if neu > old, need to update *loc */
    } while (!atomic_long_try_cmpxchg(loc, &old, neu));
    return neu;
}

uint32_t lfring_enqueue(lfring_t *lfr, void *const *elems, uint32_t n_elems)
{
    long actual = 0;
    __ringidx_t mask = lfr->mask;
    __ringidx_t size = mask + 1;
    __ringidx_t tail = atomic_long_read(&lfr->tail);

    uint32_t i;

    if (lfr->flags & LFRING_FLAG_SP) {
        uint32_t head = atomic_long_read(&lfr->head);

        actual = min((long) (head + size - tail), (long) n_elems);
        if (actual <= 0)
            return 0;

        for (i = 0; i < (uint32_t) actual; i++) {
            lfr->ring[tail & mask].ptr = *elems++;
            atomic_long_set(&lfr->ring[tail & mask].idx, tail);
            tail++;
        }
        atomic_long_set(&lfr->tail, tail);
        return (uint32_t) actual;
    }

    /* TODO: support lock-free multi-producer */
    return 0;
}

static inline __ringidx_t find_tail(lfring_t *lfr,
                                    __ringidx_t head,
                                    __ringidx_t tail)
{
    __ringidx_t mask, size;

    if (lfr->flags & LFRING_FLAG_SP) /* single-producer enqueue */
        return atomic_long_read(&lfr->tail);

    /* Multi-producer enqueue.
     * Scan ring for new elements that have been written but not released.
     */
    mask = lfr->mask;
    size = mask + 1;

    while (before(tail, head + size) &&
           atomic_long_read(&lfr->ring[tail & mask].idx) == tail)
        tail++;
    tail = cond_update(&lfr->tail, tail);
    return tail;
}

uint32_t lfring_dequeue(lfring_t *lfr,
                        void **elems,
                        uint32_t n_elems,
                        uint32_t *index)
{
    long actual;
    __ringidx_t mask = lfr->mask;
    __ringidx_t head = atomic_long_read(&lfr->head);
    __ringidx_t tail = atomic_long_read(&lfr->tail);
    uint32_t i;

    do {
        actual = min((long) (tail - head), (long) n_elems);
        if (unlikely(actual <= 0)) {
            /* Ring buffer is empty, scan for new but unreleased elements */
            tail = find_tail(lfr, head, tail);
            actual = min((long) (tail - head), (long) n_elems);
            if (actual <= 0)
                return 0;
        }
        for (i = 0; i < (uint32_t) actual; i++)
            elems[i] = lfr->ring[(head + i) & mask].ptr;
        // smp_fence(LoadStore);                        // Order loads only
        if (lfr->flags & LFRING_FLAG_SC) { /* Single-consumer */
            atomic_long_set(&lfr->head, head + actual);
            break;
        }

        /* else: lock-free multi-consumer */
    } while (!atomic_long_try_cmpxchg(&lfr->head, &head, head + actual));

    *index = (uint32_t) head;
    return (uint32_t) actual;
}
