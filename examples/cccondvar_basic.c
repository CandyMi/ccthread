/*
 * cccondvar_basic.c — Condition variable usage
 *
 * Pattern: producer signals consumer via condvar + mutex.
 *
 * Compile (POSIX): cc -o condvar examples/cccondvar_basic.c cccondvar.c ccmutex.c ccthread.c -lpthread
 * Compile (MSVC):  cl examples\cccondvar_basic.c cccondvar.c ccmutex.c ccthread.c
 */

#include "ccmutex.h"
#include "ccthread.h"
#include <stdio.h>
#include <stdlib.h>

/* ---- Bounded queue with condvar ---- */

#define QUEUE_CAPACITY 4

typedef struct {
    int            buf[QUEUE_CAPACITY];
    int            head;
    int            tail;
    int            count;
    ccmutex_t*     mtx;
    cccondvar_t*   not_full;    /* signalled when an item is removed */
    cccondvar_t*   not_empty;   /* signalled when an item is added  */
} queue_t;

static queue_t* queue_create(void) {
    queue_t* q = (queue_t*)calloc(1, sizeof(queue_t));
    if (!q) return NULL;
    q->mtx       = ccmutex_create(CCRECURSION_PLAIN);
    q->not_full  = cccondvar_create();
    q->not_empty = cccondvar_create();
    if (!q->mtx || !q->not_full || !q->not_empty) {
        if (q->mtx)       ccmutex_destroy(q->mtx);
        if (q->not_full)  cccondvar_destroy(q->not_full);
        if (q->not_empty) cccondvar_destroy(q->not_empty);
        free(q);
        return NULL;
    }
    return q;
}

static void queue_destroy(queue_t* q) {
    ccmutex_destroy(q->mtx);
    cccondvar_destroy(q->not_full);
    cccondvar_destroy(q->not_empty);
    free(q);
}

static void queue_push(queue_t* q, int item) {
    ccmutex_lock(q->mtx);
    while (q->count == QUEUE_CAPACITY) {
        cccondvar_wait(q->not_full, q->mtx);
    }
    q->buf[q->tail] = item;
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count++;
    cccondvar_signal(q->not_empty);
    ccmutex_unlock(q->mtx);
}

static int queue_pop(queue_t* q) {
    int item;
    ccmutex_lock(q->mtx);
    while (q->count == 0) {
        cccondvar_wait(q->not_empty, q->mtx);
    }
    item = q->buf[q->head];
    q->head = (q->head + 1) % QUEUE_CAPACITY;
    q->count--;
    cccondvar_signal(q->not_full);
    ccmutex_unlock(q->mtx);
    return item;
}

/* ---- Producer / Consumer ---- */

#define ITEMS 20

typedef struct {
    queue_t* q;
    int      id;
} pc_arg_t;

static void* producer(void* arg) {
    pc_arg_t* pa = (pc_arg_t*)arg;
    for (int i = 0; i < ITEMS; i++) {
        int item = pa->id * 100 + i;
        queue_push(pa->q, item);
        printf("  [P%d] pushed %d\n", pa->id, item);
        ccthread_yield();
    }
    return NULL;
}

static void* consumer(void* arg) {
    pc_arg_t* pa = (pc_arg_t*)arg;
    for (int i = 0; i < ITEMS; i++) {
        int item = queue_pop(pa->q);
        printf("  [C%d] popped %d\n", pa->id, item);
        ccthread_yield();
    }
    return NULL;
}

int main(void) {
    pc_arg_t p1 = { NULL, 1 };
    pc_arg_t c1 = { NULL, 1 };

    queue_t* q = queue_create();
    if (!q) {
        fprintf(stderr, "FAIL: queue_create\n");
        return 1;
    }
    p1.q = q;
    c1.q = q;

    ccthread_t* prod = ccthread_create(producer, &p1);
    ccthread_t* cons = ccthread_create(consumer, &c1);

    ccthread_join(prod, NULL);
    ccthread_join(cons, NULL);

    queue_destroy(q);

    printf("Done — %d items produced and consumed.\n", ITEMS);
    return 0;
}
