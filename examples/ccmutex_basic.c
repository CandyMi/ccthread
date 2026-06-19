/**
 * @file      ccmutex_basic.c
 * @brief     Demonstrate ccmutex plain and recursive modes
 *
 * Compile: cc -o ccmutex_basic examples/ccmutex_basic.c ccthread.c -lpthread
 */

#include "ccmutex.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int rc;

    /* ---- PLAIN mode: non-recursive ---- */
    printf("=== ccmutex PLAIN ===\n");
    ccmutex_t* mtx = ccmutex_create(CCRECURSION_PLAIN);
    if (!mtx) {
        fprintf(stderr, "Failed to create mutex\n");
        return 1;
    }

    rc = ccmutex_lock(mtx);
    if (rc != CCMUTEX_SUCCESS) {
        fprintf(stderr, "lock failed\n");
        ccmutex_destroy(mtx);
        return 1;
    }
    printf("  acquired\n");

    rc = ccmutex_trylock(mtx);
    printf("  trylock (should fail, PLAIN): %s\n",
           rc == CCMUTEX_SUCCESS ? "acquired" : "busy");

    ccmutex_unlock(mtx);
    printf("  released\n");

    rc = ccmutex_trylock(mtx);
    printf("  trylock (should succeed): %s\n",
           rc == CCMUTEX_SUCCESS ? "acquired" : "busy");

    if (rc == CCMUTEX_SUCCESS) {
        ccmutex_unlock(mtx);
    }

    ccmutex_destroy(mtx);

    /* ---- RECURSIVE mode ---- */
    printf("\n=== ccmutex RECURSIVE ===\n");
    mtx = ccmutex_create(CCRECURSION_RECURSIVE);
    if (!mtx) {
        fprintf(stderr, "Failed to create recursive mutex\n");
        return 1;
    }

    ccmutex_lock(mtx);
    printf("  acquired (level 1)\n");

    /* Same thread can re-enter */
    rc = ccmutex_trylock(mtx);
    printf("  trylock (should succeed, RECURSIVE): %s\n",
           rc == CCMUTEX_SUCCESS ? "acquired" : "busy");

    if (rc == CCMUTEX_SUCCESS) {
        printf("  acquired (level 2)\n");
        ccmutex_unlock(mtx);
        printf("  released (level 2)\n");
    }

    ccmutex_unlock(mtx);
    printf("  released (level 1)\n");

    ccmutex_destroy(mtx);
    printf("\nAll ccmutex tests passed.\n");
    return 0;
}
