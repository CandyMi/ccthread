/*
 * ccthread_basic.c — Basic thread lifecycle
 *
 * Compile (POSIX):   cc -o basic examples/ccthread_basic.c ccthread.c -lpthread
 * Compile (MSVC):    cl examples\ccthread_basic.c ccthread.c
 */

#include "ccthread.h"
#include <stdio.h>
#include <stdlib.h>

/* ---- A worker that squares a number ---- */
static void* square_worker(void* arg) {
    int n = *(int*)arg;
    int* result = (int*)malloc(sizeof(int));
    *result = n * n;
    ccthread_yield();        /* give others a chance (harmless here) */
    return result;
}

/* ---- A worker that prints its argument ---- */
static void* print_worker(void* arg) {
    const char* msg = (const char*)arg;
    printf("  [thread] %s\n", msg);
    return NULL;
}

int main(void) {
    /* ----------------------------------------------------------- */
    /*  1. Create and join a single thread                         */
    /* ----------------------------------------------------------- */
    int input = 8;
    ccthread_t* th = ccthread_create(square_worker, &input);
    if (!th) {
        fprintf(stderr, "Failed to create thread\n");
        return 1;
    }

    void* ret = NULL;
    if (ccthread_join(th, &ret) != CCTHREAD_SUCCESS) {
        fprintf(stderr, "Failed to join thread\n");
        return 1;
    }

    /* `th` is now destroyed — do not use it */
    printf("8^2 = %d\n", *(int*)ret);
    free(ret);   /* we own the memory returned by the thread */

    /* ----------------------------------------------------------- */
    /*  2. Launch 4 threads in parallel and collect results        */
    /* ----------------------------------------------------------- */
    #define N 4
    int  vals[N] = {3, 5, 7, 11};
    ccthread_t* workers[N];

    for (int i = 0; i < N; i++) {
        workers[i] = ccthread_create(square_worker, &vals[i]);
    }

    for (int i = 0; i < N; i++) {
        void* r = NULL;
        ccthread_join(workers[i], &r);
        printf("%d^2 = %d\n", vals[i], *(int*)r);
        free(r);
    }

    /* ----------------------------------------------------------- */
    /*  3. Thread identity: self() and equal()                     */
    /* ----------------------------------------------------------- */
    {
        ccthread_t* me1 = ccthread_self();
        ccthread_t* me2 = ccthread_self();

        printf("same thread? %s\n",
               ccthread_equal(me1, me2) ? "yes" : "no");

        ccthread_destroy(me1);  /* self() handles must be destroyed */
        ccthread_destroy(me2);
    }

    /* ----------------------------------------------------------- */
    /*  4. Mixed parallel work                                     */
    /* ----------------------------------------------------------- */
    {
        ccthread_t* t1 = ccthread_create(print_worker, "hello");
        ccthread_t* t2 = ccthread_create(print_worker, "world");
        ccthread_join(t1, NULL);
        ccthread_join(t2, NULL);
    }

    printf("All done.\n");
    return 0;
}
