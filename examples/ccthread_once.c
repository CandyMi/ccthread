/*
 * ccthread_once.c — One-time initialisation primitive
 *
 * Tests: basic call, concurrent callers, re-init, NULL guards.
 *
 * Compile (POSIX): cc -o once examples/ccthread_once.c ccthread.c -lpthread
 * Compile (MSVC):  cl examples\ccthread_once.c ccthread.c
 */

#include "ccthread.h"
#include <stdio.h>
#include <stdlib.h>

/* ---- 1. Basic single-threaded once ---- */
static int s_init_count = 0;

static void init_marker(void* arg) {
    (void)arg;
    s_init_count++;
}

static int test_basic(void) {
    ccthread_once_t once = CCTHREAD_ONCE_INIT;

    /* First call should run init */
    if (ccthread_once(&once, init_marker, NULL) != CCTHREAD_SUCCESS) return 0;
    if (s_init_count != 1) return 0;

    /* Second call should NOT run init */
    if (ccthread_once(&once, init_marker, NULL) != CCTHREAD_SUCCESS) return 0;
    if (s_init_count != 1) return 0;

    /* Third call — still once */
    if (ccthread_once(&once, init_marker, NULL) != CCTHREAD_SUCCESS) return 0;
    if (s_init_count != 1) return 0;

    printf("  [basic] once=%d count=%d\n", once, s_init_count);
    return 1;
}

/* ---- 2. Concurrent callers (parallel once) ---- */
typedef struct {
    ccthread_once_t* once;
    int              result;
} conc_arg_t;

static void* conc_worker(void* arg) {
    conc_arg_t* ca = (conc_arg_t*)arg;
    ca->result = ccthread_once(ca->once, init_marker, NULL);
    return NULL;
}

static int test_concurrent(void) {
    ccthread_once_t once = CCTHREAD_ONCE_INIT;
    ccthread_t* threads[20];
    conc_arg_t  args[20];
    int i, ok = 0;
    int prev_count = s_init_count;

    for (i = 0; i < 20; i++) {
        args[i].once   = &once;
        args[i].result = -1;
        threads[i] = ccthread_create(conc_worker, &args[i]);
        if (!threads[i]) return 0;
    }

    for (i = 0; i < 20; i++) {
        ccthread_join(threads[i], NULL);
        if (args[i].result == CCTHREAD_SUCCESS) ok++;
    }

    /* Exactly one more init call across all threads */
    if (s_init_count != prev_count + 1) return 0;
    /* All 20 threads got SUCCESS */
    if (ok != 20) return 0;

    /* After completion, another sequential call is still a no-op */
    if (ccthread_once(&once, init_marker, NULL) != CCTHREAD_SUCCESS) return 0;
    if (s_init_count != prev_count + 1) return 0;

    printf("  [concurrent] count_inc=%d ok=%d/20\n",
           s_init_count - prev_count, ok);
    return 1;
}

/* ---- 3. Re-initialisation (reset + re-use) ---- */
static int test_reset(void) {
    ccthread_once_t once = CCTHREAD_ONCE_INIT;
    int prev_count = s_init_count;

    /* First round */
    if (ccthread_once(&once, init_marker, NULL) != CCTHREAD_SUCCESS) return 0;
    if (s_init_count != prev_count + 1) return 0;

    /* Reset — for library users who need to re-init */
    once = CCTHREAD_ONCE_INIT;

    /* Second round */
    if (ccthread_once(&once, init_marker, NULL) != CCTHREAD_SUCCESS) return 0;
    if (s_init_count != prev_count + 2) return 0;

    printf("  [reset] count=%d (expected %d)\n",
           s_init_count, prev_count + 2);
    return 1;
}

/* ---- 4. NULL guards ---- */
static int test_guards(void) {
    int rc;

    rc = ccthread_once(NULL, init_marker, NULL);
    if (rc != CCTHREAD_ERROR) {
        printf("  [guards] NULL once got %d, expected %d\n", rc, CCTHREAD_ERROR);
        return 0;
    }

    {
        ccthread_once_t once = CCTHREAD_ONCE_INIT;
        rc = ccthread_once(&once, NULL, NULL);
        if (rc != CCTHREAD_ERROR) {
            printf("  [guards] NULL func got %d, expected %d\n",
                   rc, CCTHREAD_ERROR);
            return 0;
        }
    }

    printf("  [guards] both NULL checks passed\n");
    return 1;
}

/* ---- 5. Argument forwarding ---- */
static int s_arg_value = 0;

static void init_with_arg(void* arg) {
    s_arg_value = *(int*)arg;
}

static int test_arg(void) {
    ccthread_once_t once = CCTHREAD_ONCE_INIT;
    int magic = 42;

    s_arg_value = 0;
    if (ccthread_once(&once, init_with_arg, &magic) != CCTHREAD_SUCCESS) return 0;
    if (s_arg_value != 42) return 0;

    printf("  [arg] value=%d (expected 42)\n", s_arg_value);
    return 1;
}

int main(void) {
    int passed = 0, total = 5;

    printf("ccthread_once tests:\n");

    if (test_basic())       passed++; else fprintf(stderr, "  FAIL basic\n");
    if (test_concurrent())  passed++; else fprintf(stderr, "  FAIL concurrent\n");
    if (test_reset())       passed++; else fprintf(stderr, "  FAIL reset\n");
    if (test_guards())      passed++; else fprintf(stderr, "  FAIL guards\n");
    if (test_arg())         passed++; else fprintf(stderr, "  FAIL arg\n");

    printf("%d/%d passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
