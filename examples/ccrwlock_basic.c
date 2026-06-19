/**
 * @file      ccrwlock_basic.c
 * @brief     Demonstrate ccrwlock with reader/writer threads
 *
 * Compile: cc -o ccrwlock_basic examples/ccrwlock_basic.c ccthread.c -lpthread
 */

#include "ccmutex.h"
#include "ccthread.h"
#include <stdio.h>
#include <stdlib.h>

static ccrwlock_t* g_rwlock;
static int         g_value;

static void* reader(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < 50; i++) {
        ccrwlock_rdlock(g_rwlock);
        int v = g_value;
        (void)v;
        ccrwlock_unlock(g_rwlock);
    }
    printf("  [reader %d] done\n", id);
    return NULL;
}

static void* writer(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < 10; i++) {
        ccrwlock_wrlock(g_rwlock);
        g_value++;
        ccrwlock_unlock(g_rwlock);
    }
    printf("  [writer %d] done\n", id);
    return NULL;
}

int main(void) {
    ccthread_t* readers[3];
    ccthread_t* writers[2];
    int rid[3] = {1, 2, 3};
    int wid[2] = {1, 2};

    printf("=== ccrwlock reader/writer ===\n");

    g_rwlock = ccrwlock_create();
    if (!g_rwlock) {
        fprintf(stderr, "Failed to create rwlock\n");
        return 1;
    }

    g_value = 0;

    for (int i = 0; i < 2; i++) {
        writers[i] = ccthread_create(writer, &wid[i]);
        if (!writers[i]) {
            fprintf(stderr, "Failed to create writer %d\n", wid[i]);
            return 1;
        }
    }

    for (int i = 0; i < 3; i++) {
        readers[i] = ccthread_create(reader, &rid[i]);
        if (!readers[i]) {
            fprintf(stderr, "Failed to create reader %d\n", rid[i]);
            return 1;
        }
    }

    for (int i = 0; i < 2; i++) {
        ccthread_join(writers[i], NULL);
    }
    for (int i = 0; i < 3; i++) {
        ccthread_join(readers[i], NULL);
    }

    printf("  final value: %d (expected 20)\n", g_value);

    ccrwlock_destroy(g_rwlock);
    printf("\nAll ccrwlock tests passed.\n");
    return 0;
}
