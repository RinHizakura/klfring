#pragma once

#if defined(__x86_64__)
union u128 {
    struct {
        uint64_t lo, hi;
    } s;
    __int128 ui;
};

static inline bool lf_compare_exchange(register __int128 *var,
                                       __int128 *exp,
                                       __int128 neu)
{
    union u128 cmp = {.ui = *exp}, with = {.ui = neu};
    bool ret;
    __asm__ __volatile__("lock cmpxchg16b %1\n\tsetz %0"
                         : "=q"(ret), "+m"(*var), "+d"(cmp.s.hi), "+a"(cmp.s.lo)
                         : "c"(with.s.hi), "b"(with.s.lo)
                         : "cc", "memory");
    if (unlikely(!ret))
        *exp = cmp.ui;
    return ret;
}

#else
#error "Unsupported architecture"
#endif
