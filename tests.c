#include <stdio.h>
#include <stdlib.h>

#include "lbring.h"

#define EX_HASHSTR(s) #s
#define EX_STR(s) EX_HASHSTR(s)

#define CHECK

#ifdef CHECK
#define EXPECT(exp)                                                      \
    {                                                                    \
        if (!(exp))                                                      \
            fprintf(stderr, "FAILURE @ %s:%u; %s\n", __FILE__, __LINE__, \
                    EX_STR(exp)),                                        \
                abort();                                                 \
    }
#else
#define EXPECT(exp) ;
#endif

static void test_ringbuffer(uint32_t flags)
{
    void *vec[4];
    uint32_t ret;
    uint32_t idx;

    lbring_t *rb = lbring_alloc(2, flags);
    EXPECT(rb != NULL);

    ret = lbring_dequeue(rb, vec, 1, &idx);
    EXPECT(ret == 0);
    ret = lbring_enqueue(rb, (void *[]){(void *) 1}, 1);
    EXPECT(ret == 1);

    ret = lbring_dequeue(rb, vec, 1, &idx);
    EXPECT(ret == 1);
    EXPECT(idx == 0);
    EXPECT(vec[0] == (void *) 1);

    ret = lbring_dequeue(rb, vec, 1, &idx);
    EXPECT(ret == 0);

    ret = lbring_enqueue(rb, (void *[]){(void *) 2, (void *) 3, (void *) 4}, 3);
    EXPECT(ret == 2);

    ret = lbring_dequeue(rb, vec, 1, &idx);
    EXPECT(ret == 1);
    EXPECT(idx == 1);
    EXPECT(vec[0] == (void *) 2);

    ret = lbring_dequeue(rb, vec, 4, &idx);
    EXPECT(ret == 1);
    EXPECT(idx == 2);
    EXPECT(vec[0] == (void *) 3);

    lbring_free(rb);
}

int main(void)
{
    printf("testing MPMC lock-free ring\n");
    test_ringbuffer(LBRING_FLAG_MP | LBRING_FLAG_MC);

    printf("testing MPSC lock-free ring\n");
    test_ringbuffer(LBRING_FLAG_MP | LBRING_FLAG_SC);

    printf("testing SPMC lock-free ring\n");
    test_ringbuffer(LBRING_FLAG_SP | LBRING_FLAG_MC);

    printf("testing SPSC lock-free ring\n");
    test_ringbuffer(LBRING_FLAG_SP | LBRING_FLAG_SC);

    return 0;
}