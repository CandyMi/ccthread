/*
 * ccsem_timeout.c — Non-blocking and timed wait patterns
 *
 * Demonstrates: ccsem_trywait, ccsem_timedwait.
 *
 * Compile (POSIX):   cc -o timeout examples/ccsem_timeout.c ccsem.c ccthread.c -lpthread
 * Compile (MSVC):    cl examples\ccsem_timeout.c ccsem.c ccthread.c
 */

#include "ccsem.h"
#include "ccthread.h"
#include <stdio.h>

/* ---- Pattern 1: trywait — poll without blocking ---- */
static void poll_demo(void) {
    ccsem_t* sem = ccsem_create(0);   /* locked */

    printf("  trywait on locked sem:  ");
    switch (ccsem_trywait(sem)) {
    case CCSEM_SUCCESS: printf("acquired\n");  break;
    case CCSEM_TIMEOUT: printf("would block\n"); break;
    default:            printf("error\n");       break;
    }

    ccsem_post(sem);  /* unlock */

    printf("  trywait after post:     ");
    switch (ccsem_trywait(sem)) {
    case CCSEM_SUCCESS: printf("acquired\n");  break;
    case CCSEM_TIMEOUT: printf("would block\n"); break;
    default:            printf("error\n");       break;
    }

    ccsem_destroy(sem);
}

/* ---- Pattern 2: timedwait — wait with deadline ---- */
static void* late_poster(void* arg) {
    ccsem_t* sem = (ccsem_t*)arg;
    ccthread_sleep(80);          /* post after 80ms delay */
    printf("  [poster] signalling now\n");
    ccsem_post(sem);
    return NULL;
}

static void timed_demo(void) {
    ccsem_t* sem = ccsem_create(0);

    /* Launch a thread that will post after 80ms */
    ccthread_t* poster = ccthread_create(late_poster, sem);

    /* Wait up to 200ms — should wake before timeout */
    printf("  timedwait(200ms): ");
    int rc = ccsem_timedwait(sem, 200);
    printf("%s\n", rc == CCSEM_SUCCESS ? "acquired" :
                   rc == CCSEM_TIMEOUT ? "timeout" : "error");

    ccthread_join(poster, NULL);

    /* Now try a timeout that expires before anyone posts */
    printf("  timedwait(50ms) on locked: ");
    rc = ccsem_timedwait(sem, 50);
    printf("%s\n", rc == CCSEM_SUCCESS ? "acquired" :
                   rc == CCSEM_TIMEOUT ? "timeout" : "error");

    ccsem_destroy(sem);
}

/* ---- Pattern 3: timedwait in a loop (periodic check) ---- */
static void periodic_demo(void) {
    ccsem_t* sem = ccsem_create(0);
    int      attempts = 0;

    printf("  polling every 10ms... ");
    while (ccsem_timedwait(sem, 10) == CCSEM_TIMEOUT) {
        attempts++;
        if (attempts >= 5) {
            printf("giving up after %d attempts\n", attempts);
            break;
        }
    }
    /* (a real program would ccsem_post(sem) from elsewhere) */

    ccsem_destroy(sem);
}

int main(void) {
    printf("=== trywait (non-blocking poll) ===\n");
    poll_demo();

    printf("\n=== timedwait (deadline) ===\n");
    timed_demo();

    printf("\n=== timedwait (periodic poll) ===\n");
    periodic_demo();

    printf("\nDone.\n");
    return 0;
}
