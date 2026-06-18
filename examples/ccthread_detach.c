/*
 * ccthread_detach.c — Fire-and-forget with detached threads
 *
 * A detached thread runs independently — the caller does not (and
 * cannot) join it.  The OS reclaims thread resources on exit.
 *
 * Compile (POSIX):   cc -o detach examples/ccthread_detach.c ccthread.c -lpthread
 * Compile (MSVC):    cl examples\ccthread_detach.c ccthread.c
 */

#include "ccthread.h"
#include <stdio.h>
#include <stdlib.h>

static void* fire_and_forget(void* arg) {
    int id = *(int*)arg;
    printf("  [detached #%d] started\n", id);

    /* Simulate work that takes varying time */
    ccthread_sleep((unsigned int)(30 + id * 20));

    printf("  [detached #%d] finished\n", id);
    return NULL;
}

int main(void) {
    printf("Launching 3 detached threads...\n");

    int ids[3] = {1, 2, 3};

    for (int i = 0; i < 3; i++) {
        ccthread_t* th = ccthread_create(fire_and_forget, &ids[i]);
        if (!th) {
            fprintf(stderr, "Failed to create thread %d\n", ids[i]);
            return 1;
        }
        ccthread_detach(th);
        /* th is now consumed — do not use it */
    }

    printf("Main thread continues working...\n");
    ccthread_sleep(200);   /* give detached threads time to finish */

    printf("Done.\n");
    return 0;
}
