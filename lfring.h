#ifndef LFRING_H
#define LFRING_H

#include <linux/compiler_attributes.h>

enum {
    LFRING_FLAG_MP = 0x0000 /* Multiple producer */,
    LFRING_FLAG_SP = 0x0001 /* Single producer */,
    LFRING_FLAG_MC = 0x0000 /* Multi consumer */,
    LFRING_FLAG_SC = 0x0002 /* Single consumer */,
};

typedef struct lfring lfring_t;

lfring_t *lfring_alloc(uint32_t n_elems, uint32_t flags);
void lfring_free(lfring_t *lfr);
uint32_t lfring_enqueue(lfring_t *lfr, void *const *elems, uint32_t n_elems);
uint32_t lfring_dequeue(lfring_t *lfr,
                        void **elems,
                        uint32_t n_elems,
                        uint32_t *index);

#endif
