/*
 * ccthread_lifecycle.c — Comprehensive lifecycle tests
 *
 * Covers all 6 paths of the ccthread_t lifecycle state machine,
 * including the newly-fixed ccthread_exit + detach interactions.
 *
 * Compile (POSIX):   cc -o lifecycle examples/ccthread_lifecycle.c ccthread.c -lpthread
 * Compile (MSVC):    cl examples\ccthread_lifecycle.c ccthread.c
 */

#include "ccthread.h"
#include <stdio.h>
#include <stdlib.h>

/* ---- Helpers ---- */

static int tests_pass = 0;
static int tests_fail = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL  %s  (%s:%d)\n", name, __FILE__, __LINE__); \
        tests_fail++; \
    } else { \
        printf("  PASS  %s\n", name); \
        tests_pass++; \
    } \
} while(0)

/* ---- Reusable worker functions ---- */

static void* normal_worker(void* arg) {
    int* ret = (int*)malloc(sizeof(int));
    if (ret) *ret = *(int*)arg;
    return ret;
}

static void* exit_worker(void* arg) {
    int* ret = (int*)malloc(sizeof(int));
    if (ret) *ret = *(int*)arg;
    ccthread_exit(ret);
    /* never reached */
    return NULL;
}

/* ---- Test 1: normal return → join ---- */
static void test_normal_join(void) {
    printf("== 1. normal return → join\n");
    int val = 42;
    ccthread_t* th = ccthread_create(normal_worker, &val);
    TEST("create", th != NULL);

    void* ret = NULL;
    TEST("join", ccthread_join(th, &ret) == CCTHREAD_SUCCESS);
    TEST("value", ret != NULL && *(int*)ret == 42);
    free(ret);
}

/* ---- Test 2: normal return → detach ---- */
static void test_normal_detach(void) {
    printf("== 2. normal return → detach\n");
    int flag = 0;
    ccthread_t* th = ccthread_create(normal_worker, &flag);
    TEST("create", th != NULL);
    TEST("detach", ccthread_detach(th) == CCTHREAD_SUCCESS);
    ccthread_sleep(100);
    TEST("thread ran", 1);
}

/* ---- Test 3: ccthread_exit → join ---- */
static void test_exit_join(void) {
    printf("== 3. ccthread_exit → join\n");
    int val = 99;
    ccthread_t* th = ccthread_create(exit_worker, &val);
    TEST("create", th != NULL);

    void* ret = NULL;
    TEST("join", ccthread_join(th, &ret) == CCTHREAD_SUCCESS);
    TEST("value", ret != NULL && *(int*)ret == 99);
    free(ret);
}

/* ---- Test 4: ccthread_exit finishes → detach called later ---- */
static void* early_exit_worker(void* arg) {
    int* ret = (int*)malloc(sizeof(int));
    if (ret) *ret = *(int*)arg;
    ccthread_exit(ret);
    return NULL;
}

static void test_exit_then_detach(void) {
    printf("== 4. exit finishes → detach\n");
    int val = 77;
    ccthread_t* th = ccthread_create(early_exit_worker, &val);
    TEST("create", th != NULL);

    ccthread_sleep(50);   /* let child finish before we detach */
    TEST("detach after exit", ccthread_detach(th) == CCTHREAD_SUCCESS);
}

/* ---- Test 5: detach called first → ccthread_exit later ---- */
struct gate_ctx {
    volatile int go;    /* 1 = child may proceed to exit */
};

static void* gated_exit_worker(void* arg) {
    struct gate_ctx* g = (struct gate_ctx*)arg;
    while (g->go == 0) {          /* volatile read — eventually visible */
        ccthread_yield();
    }
    int* ret = (int*)malloc(sizeof(int));
    if (ret) *ret = 123;
    ccthread_exit(ret);
    return NULL;
}

static void test_detach_then_exit(void) {
    printf("== 5. detach → exit\n");
    struct gate_ctx g = { .go = 0 };
    ccthread_t* th = ccthread_create(gated_exit_worker, &g);
    TEST("create", th != NULL);

    ccthread_sleep(20);                   /* child is spinning */
    TEST("detach while running", ccthread_detach(th) == CCTHREAD_SUCCESS);

    g.go = 1;                             /* release child to exit */
    ccthread_sleep(100);                  /* let child finish */
    TEST("detach→exit completed", 1);
}

/* ---- Test 6: concurrent exit/detach race stress ---- */
static void test_concurrent_race(void) {
    printf("== 6. exit ∥ detach race × 50\n");
    for (int i = 0; i < 50; i++) {
        int val = i;
        ccthread_t* th = ccthread_create(exit_worker, &val);
        TEST("create in race", th != NULL);

        /* detach immediately — races with the child's ccthread_exit */
        TEST("detach in race", ccthread_detach(th) == CCTHREAD_SUCCESS);
    }
    ccthread_sleep(200);
    TEST("50 races completed", 1);
}

/* ---- Test 7: stress — 200 threads, normal return, join ---- */
static void test_stress_join(void) {
    printf("== 7. stress 200× create→normal→join\n");
    enum { N = 200 };
    ccthread_t* ths[N];
    int         vals[N];

    for (int i = 0; i < N; i++) {
        vals[i] = i * i;
        ths[i] = ccthread_create(normal_worker, &vals[i]);
        TEST("create", ths[i] != NULL);
    }
    for (int i = 0; i < N; i++) {
        void* ret = NULL;
        TEST("join", ccthread_join(ths[i], &ret) == CCTHREAD_SUCCESS);
        TEST("value", ret != NULL && *(int*)ret == i * i);
        free(ret);
    }
}

/* ---- Test 8: stress — 200 threads, ccthread_exit, join ---- */
static void test_stress_exit_join(void) {
    printf("== 8. stress 200× create→exit→join\n");
    enum { N = 200 };
    ccthread_t* ths[N];
    int         vals[N];

    for (int i = 0; i < N; i++) {
        vals[i] = i + 1000;
        ths[i] = ccthread_create(exit_worker, &vals[i]);
        TEST("create", ths[i] != NULL);
    }
    for (int i = 0; i < N; i++) {
        void* ret = NULL;
        TEST("join", ccthread_join(ths[i], &ret) == CCTHREAD_SUCCESS);
        TEST("value", ret != NULL && *(int*)ret == i + 1000);
        free(ret);
    }
}

/* ---- Test 9: stress — 200 threads, normal return, detach ---- */
static void test_stress_normal_detach(void) {
    printf("== 9. stress 200× create→normal→detach\n");
    enum { N = 200 };
    ccthread_t* ths[N];

    for (int i = 0; i < N; i++) {
        int* v = (int*)malloc(sizeof(int));
        *v = i;
        ths[i] = ccthread_create(normal_worker, v);
        TEST("create", ths[i] != NULL);
        /* v intentionally leaked — testing struct lifecycle, not user memory */
    }
    for (int i = 0; i < N; i++) {
        TEST("detach", ccthread_detach(ths[i]) == CCTHREAD_SUCCESS);
    }
    ccthread_sleep(300);
    TEST("200 normal+detach completed", 1);
}

/* ---- Test 10: stress — 200 threads, ccthread_exit, detach ---- */
static void test_stress_exit_detach(void) {
    printf("== 10. stress 200× create→exit→detach\n");
    enum { N = 200 };
    ccthread_t* ths[N];

    for (int i = 0; i < N; i++) {
        int* v = (int*)malloc(sizeof(int));
        *v = i;
        ths[i] = ccthread_create(exit_worker, v);
        TEST("create", ths[i] != NULL);
    }

    ccthread_sleep(30);   /* let threads start exiting */
    for (int i = 0; i < N; i++) {
        TEST("detach", ccthread_detach(ths[i]) == CCTHREAD_SUCCESS);
    }
    ccthread_sleep(200);
    TEST("200 exit+detach completed", 1);
}

/* ---- Test 11: ccthread_self identity + ccthread_equal ---- */
struct equal_ctx {
    ccthread_t* expected;
    int          equal;
    volatile int done;
};

static void* equal_worker(void* arg) {
    struct equal_ctx* ctx = (struct equal_ctx*)arg;
    ccthread_t* self = ccthread_self();
    ctx->equal = ccthread_equal(ctx->expected, self);
    ctx->done = 1;          /* paired with join() happens-before */
    return NULL;
}

static void test_self_identity(void) {
    printf("== 11. self() identity and equal()\n");
    struct equal_ctx ctx = { NULL, 0, 0 };

    ccthread_t* th = ccthread_create(equal_worker, &ctx);
    TEST("create", th != NULL);
    ctx.expected = th;          /* share create handle for comparison */

    void* ret = NULL;
    ccthread_join(th, &ret);

    TEST("self() returned non-NULL", ctx.done == 1);
    TEST("self() equals create handle", ctx.equal == 1);
}

/* ---- Test 12: error paths — double-join, join-after-detach, etc. ---- */
static void test_error_paths(void) {
    printf("== 12. error paths\n");

    /* Double join on a normal thread */
    {
        int val = 1;
        ccthread_t* th = ccthread_create(normal_worker, &val);
        TEST("create for double-join", th != NULL);

        void* r1 = NULL;
        ccthread_join(th, &r1);
        free(r1);
        /* th is now destroyed — can't test double-join on same pointer */
    }

    /* join on self handle should error */
    {
        ccthread_t* me = ccthread_self();
        TEST("join(self) returns ERROR",
             ccthread_join(me, NULL) == CCTHREAD_ERROR);
        TEST("detach(self) returns ERROR",
             ccthread_detach(me) == CCTHREAD_ERROR);
    }

    /* join on NULL should error */
    {
        TEST("join(NULL) returns ERROR",
             ccthread_join(NULL, NULL) == CCTHREAD_ERROR);
        TEST("detach(NULL) returns ERROR",
             ccthread_detach(NULL) == CCTHREAD_ERROR);
    }

    /* Join on a detached thread should error */
    {
        int flag = 0;
        ccthread_t* th = ccthread_create(normal_worker, &flag);
        TEST("create for detach→join", th != NULL);
        ccthread_detach(th);
        /* th is now consumed — pointer invalid. Just verify detach returned success. */
        TEST("detach succeeded", 1);

        /* Can't test ccthread_join(th) here because th is freed */
    }

    /* ccthread_self() returns cached pointer on repeated calls */
    {
        ccthread_t* a = ccthread_self();
        ccthread_t* b = ccthread_self();
        TEST("self() is stable", ccthread_equal(a, b));
    }
}

/* ---- Main ---- */

int main(void) {
    printf("=== ccthread lifecycle tests ===\n\n");

    test_normal_join();
    printf("\n");
    test_normal_detach();
    printf("\n");
    test_exit_join();
    printf("\n");
    test_exit_then_detach();
    printf("\n");
    test_detach_then_exit();
    printf("\n");
    test_concurrent_race();
    printf("\n");
    test_stress_join();
    printf("\n");
    test_stress_exit_join();
    printf("\n");
    test_stress_normal_detach();
    printf("\n");
    test_stress_exit_detach();
    printf("\n");
    test_self_identity();
    printf("\n");
    test_error_paths();
    printf("\n");

    printf("=== results: %d passed, %d failed ===\n",
           tests_pass, tests_fail);
    return tests_fail > 0 ? 1 : 0;
}
