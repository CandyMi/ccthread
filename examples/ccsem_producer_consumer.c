/*
 * ccsem_producer_consumer.c — Classic bounded-buffer with semaphores
 *
 * Demonstrates: ccsem_create, ccsem_wait, ccsem_post, ccsem_destroy.
 *
 * Pattern:
 *   empty_slots  — starts at BUFSIZE (how many slots are free)
 *   filled_slots — starts at 0          (how many items are ready)
 *
 * Compile (POSIX):   cc -o prodcon examples/ccsem_producer_consumer.c ccsem.c ccthread.c -lpthread
 * Compile (MSVC):    cl examples\ccsem_producer_consumer.c ccsem.c ccthread.c
 */

#include "ccsem.h"
#include "ccthread.h"
#include <stdio.h>
#include <stdlib.h>

#define BUFSIZE   4
#define ITEMS    10

static int        g_buffer[BUFSIZE];
static int        g_in  = 0;   /* next write position */
static int        g_out = 0;   /* next read position  */
static ccsem_t*   g_empty;     /* counts free slots    */
static ccsem_t*   g_filled;    /* counts ready items   */

/* ---- Producer: fills buffer ---- */
static void* producer(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < ITEMS; i++) {
        int item = id * 100 + i;

        ccsem_wait(g_empty);              /* wait for a free slot */

        g_buffer[g_in] = item;
        g_in = (g_in + 1) % BUFSIZE;
        printf("  [P%d] produced %3d\n", id, item);

        ccsem_post(g_filled);             /* signal a filled slot */
        ccthread_yield();
    }
    return NULL;
}

/* ---- Consumer: drains buffer ---- */
static void* consumer(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < ITEMS; i++) {
        ccsem_wait(g_filled);             /* wait for a ready item */

        int item = g_buffer[g_out];
        g_out = (g_out + 1) % BUFSIZE;
        printf("  [C%d] consumed %3d\n", id, item);

        ccsem_post(g_empty);              /* signal a free slot */
        ccthread_yield();
    }
    return NULL;
}

int main(void) {
    int p_id = 1, c_id = 1;

    g_empty  = ccsem_create(BUFSIZE);  /* initially all slots free */
    g_filled = ccsem_create(0);        /* no items yet             */

    ccthread_t* prod = ccthread_create(producer, &p_id);
    ccthread_t* cons = ccthread_create(consumer, &c_id);

    ccthread_join(prod, NULL);
    ccthread_join(cons, NULL);

    ccsem_destroy(g_empty);
    ccsem_destroy(g_filled);

    printf("Done — %d items produced and consumed.\n", ITEMS);
    return 0;
}
