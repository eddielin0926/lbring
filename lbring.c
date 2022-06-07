#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include "arch.h"
#include "common.h"
#include "lbring.h"
#define LOCK_BASED
#include "lock.h"

#define SUPPORTED_FLAGS \
    (LBRING_FLAG_SP | LBRING_FLAG_MP | LBRING_FLAG_SC | LBRING_FLAG_MC)

#define MIN(a, b)                      \
    ({                                 \
        __typeof__(a) tmp_a = (a);     \
        __typeof__(b) tmp_b = (b);     \
        tmp_a < tmp_b ? tmp_a : tmp_b; \
    })

typedef uintptr_t ringidx_t;

struct element {
    void *ptr;
    uintptr_t idx;
};

struct lbring {
    ringidx_t head;
    ringidx_t tail ALIGNED(CACHE_LINE);
    uint32_t mask;
    uint32_t flags;
    ptlock_t hlock;
    ptlock_t tlock;
    struct element ring[] ALIGNED(CACHE_LINE);
} ALIGNED(CACHE_LINE);

lbring_t *lbring_alloc(uint32_t n_elems, uint32_t flags)
{
    unsigned long ringsz = ROUNDUP_POW2(n_elems);
    if (n_elems == 0 || ringsz == 0 || ringsz > 0x80000000) {
        assert(0 && "invalid number of elements");
        return NULL;
    }
    if ((flags & ~SUPPORTED_FLAGS) != 0) {
        assert(0 && "invalid flags");
        return NULL;
    }

    size_t nbytes = sizeof(lbring_t) + ringsz * sizeof(struct element);
    lbring_t *lbr = osal_alloc(nbytes, CACHE_LINE);
    if (!lbr)
        return NULL;

    lbr->head = 0, lbr->tail = 0;
    lbr->mask = ringsz - 1;
    lbr->flags = flags;
    for (ringidx_t i = 0; i < ringsz; i++) {
        lbr->ring[i].ptr = NULL;
        lbr->ring[i].idx = i - ringsz;
    }

    INIT_LOCK(&lbr->hlock);
    INIT_LOCK(&lbr->tlock);

    return lbr;
}

void lbring_free(lbring_t *lbr)
{
    if (!lbr)
        return;

    if (lbr->head != lbr->tail) {
        assert(0 && "ring buffer not empty");
        return;
    }
    osal_free(lbr);
}

/* Enqueue elements at tail */
uint32_t lbring_enqueue(lbring_t *lbr,
                        void *const *restrict elems,
                        uint32_t n_elems)
{
    intptr_t actual = 0;
    ringidx_t mask = lbr->mask;
    ringidx_t size = mask + 1;
    LOCK(&lbr->tlock);
    ringidx_t tail = __atomic_load_n(&lbr->tail, __ATOMIC_RELAXED);
    ringidx_t head = __atomic_load_n(&lbr->head, __ATOMIC_ACQUIRE);
    actual = MIN((intptr_t)(head + size - tail), (intptr_t) n_elems);
    if (actual <= 0)
        return 0;

    for (uint32_t i = 0; i < (uint32_t) actual; i++) {
        assert(lbr->ring[tail & mask].idx == tail - size);
        lbr->ring[tail & mask].ptr = *elems++;
        lbr->ring[tail & mask].idx = tail;
        tail++;
    }
    __atomic_store_n(&lbr->tail, tail, __ATOMIC_RELEASE);
    UNLOCK(&lbr->tlock);
    return (uint32_t) actual;
}

/* Dequeue elements from head */
uint32_t lbring_dequeue(lbring_t *lfr,
                        void **restrict elems,
                        uint32_t n_elems,
                        uint32_t *index)
{
    ringidx_t mask = lfr->mask;
    intptr_t actual;

    LOCK(&lfr->hlock);
    
    ringidx_t head = __atomic_load_n(&lfr->head, __ATOMIC_RELAXED);
    ringidx_t tail = __atomic_load_n(&lfr->tail, __ATOMIC_ACQUIRE);
    do {
        actual = MIN((intptr_t)(tail - head), (intptr_t) n_elems);
        if (UNLIKELY(actual <= 0)) {
            /* Ring buffer is empty, scan for new but unreleased elements */
            tail = __atomic_load_n(&lfr->tail, __ATOMIC_ACQUIRE);
            actual = MIN((intptr_t)(tail - head), (intptr_t) n_elems);
            if (actual <= 0) {
                UNLOCK(&lfr->hlock);
                return 0;
            }
        }
        for (uint32_t i = 0; i < (uint32_t) actual; i++)
            elems[i] = lfr->ring[(head + i) & mask].ptr;
        smp_fence(LoadStore);                        // Order loads only
        if (UNLIKELY(lfr->flags & LBRING_FLAG_SC)) { /* Single-consumer */
            __atomic_store_n(&lfr->head, head + actual, __ATOMIC_RELAXED);
            break;
        }

        /* else: lock-free multi-consumer */
    } while (!__atomic_compare_exchange_n(
        &lfr->head, &head, /* Updated on failure */
        /* XXXXX HHH */ head + actual,
        /* weak */ false, __ATOMIC_RELAXED, __ATOMIC_RELAXED));
    *index = (uint32_t) head;
    
    UNLOCK(&lfr->hlock);

    return (uint32_t) actual;
}