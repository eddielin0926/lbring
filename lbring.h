/* Lock-free multiple-producer (MP) /multiple-consumer (MC) ring buffer. */

#pragma once

#include <stddef.h>
#include <stdint.h>

enum {
    LBRING_FLAG_MP = 0x0000 /* Multiple producer */,
    LBRING_FLAG_SP = 0x0001 /* Single producer */,
    LBRING_FLAG_MC = 0x0000 /* Multi consumer */,
    LBRING_FLAG_SC = 0x0002 /* Single consumer */,
};

typedef struct lbring lbring_t;

/* Allocate ring buffer with space for at least 'n_elems' elements.
 * 'n_elems' != 0 and 'n_elems' <= 0x80000000
 */
lbring_t *lbring_alloc(uint32_t n_elems, uint32_t flags);

/* Free ring buffer.
 * The ring buffer must be empty
 */
void lbring_free(lbring_t *lbr);

/* Enqueue elements on ring buffer.
 * The number of actually enqueued elements is returned.
 */
uint32_t lbring_enqueue(lbring_t *lbr, void *const elems[], uint32_t n_elems);

/* Dequeue elements from ring buffer.
 * The number of actually dequeued elements is returned.
 */
uint32_t lbring_dequeue(lbring_t *lbr,
                        void *elems[],
                        uint32_t n_elems,
                        uint32_t *index);