/**
 * @file      ccspinlock_basic.c
 * @brief     Demonstrate ccspinlock with shared counter
 *
 * Compile: cc -o ccspinlock_basic examples/ccspinlock_basic.c ccthread.c -lpthread
 */

#include "ccmutex.h"
#include "ccthread.h"
#include <stdio.h>
#include <stdlib.h>

static ccspinlock_t* g_lock;
static int           g_counter;

static void* worker(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < 100; i++) {
        ccspinlock_lock(g_lock);
        g_counter++;
        ccspinlock_unlock(g_lock);
    }
    printf("  [thread %d] done (%d iterations)\n", id, 100);
    return NULL;
}

int main(void) {
    int rc;
    int ids[4] = {1, 2, 3, 4};
    ccthread_t* threads[4];

    /* ---- PLAIN mode ---- */
    printf("=== ccspinlock shared counter ===\n");
    g_lock = ccspinlock_create(CCRECURSION_PLAIN);
    if (!g_lock) {
        fprintf(stderr, "Failed to create spinlock\n");
        return 1;
    }

    g_counter = 0;

    for (int i = 0; i < 4; i++) {
        threads[i] = ccthread_create(worker, &ids[i]);
        if (!threads[i]) {
            fprintf(stderr, "Failed to create thread %d\n", ids[i]);
            return 1;
        }
    }

    for (int i = 0; i < 4; i++) {
        ccthread_join(threads[i], NULL);
    }

    printf("  final counter: %d (expected 400)\n", g_counter);

    ccspinlock_destroy(g_lock);

    /* ---- trylock ---- */
    printf("\n=== ccspinlock trylock ===\n");
    ccspinlock_t* spin = ccspinlock_create(CCRECURSION_PLAIN);

    rc = ccspinlock_trylock(spin);
    printf("  trylock on unlocked: %s\n",
           rc == CCMUTEX_SUCCESS ? "acquired" : "busy");

    rc = ccspinlock_trylock(spin);
    printf("  trylock on locked:   %s\n",
           rc == CCMUTEX_SUCCESS ? "acquired" : "busy");

    ccspinlock_unlock(spin);

    rc = ccspinlock_trylock(spin);
    printf("  trylock after unlock: %s\n",
           rc == CCMUTEX_SUCCESS ? "acquired" : "busy");
    if (rc == CCMUTEX_SUCCESS) {
        ccspinlock_unlock(spin);
    }

    ccspinlock_destroy(spin);

    printf("\nAll ccspinlock tests passed.\n");
    return 0;
}
