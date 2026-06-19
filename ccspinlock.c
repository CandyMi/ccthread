/**
 * @file      ccspinlock.c
 * @author    candy <https://github.com/CandyMi/ccthread>
 * @brief     Busy-wait spinlock implementation (part of ccthread)
 *
 * Acquire: ACQUIRE memory order via atomic_flag_test_and_set.
 * Release: RELEASE memory order via atomic_flag_clear.
 *
 * Recursive mode tracks owner via ccthread_self/equal.
 * PLAIN mode ignores owner fields (calloc-zeroed, never set).
 */

#include "ccmutex.h"
#include "ccthread.h"
#include "ccatomic.h"
#include <stdlib.h>

/* ---- opaque struct definition ---- */

struct ccspinlock_impl {
    volatile long    flag;          /* 0 = free, 1 = locked */
    ccthread_t*    owner;         /* current owner (recursive mode) */
    int            recursion;     /* re-entry depth */
};

/* ================================================================== */
/*  ccspinlock_create                                                   */
/* ================================================================== */

ccspinlock_t* ccspinlock_create(ccrecursion_t type) {
    ccspinlock_t* spin;

    spin = (ccspinlock_t*)calloc(1, sizeof(ccspinlock_t));
    if (!spin) {
        return NULL;
    }
    /* flag=clear, owner=NULL, recursion=0 from calloc.
     * For atomic_flag, zero-initialised memory == ATOMIC_FLAG_INIT on all
     * known implementations. */
    (void)type;
    return spin;
}

/* ================================================================== */
/*  ccspinlock_trylock                                                  */
/* ================================================================== */

ccmutex_state_t ccspinlock_trylock(ccspinlock_t* spin) {
    ccthread_t* self;

    if (!spin) {
        return CCMUTEX_ERROR;
    }

    self = ccthread_self();
    if (!self) {
        return CCMUTEX_ERROR;
    }

    /* Recursive path: same thread already holds the lock */
    if (spin->owner && ccthread_equal(spin->owner, self)) {
        spin->recursion++;
        return CCMUTEX_SUCCESS;
    }

    /* Atomic acquire with ACQUIRE barrier */
    if (ccatomic_exchange_acquire(&spin->flag, 1) == 0) {
        spin->owner     = self;
        spin->recursion = 1;
        return CCMUTEX_SUCCESS;
    }

    return CCMUTEX_ERROR;
}

/* ================================================================== */
/*  ccspinlock_lock                                                     */
/* ================================================================== */

ccmutex_state_t ccspinlock_lock(ccspinlock_t* spin) {
    if (!spin) {
        return CCMUTEX_ERROR;
    }
    while (ccspinlock_trylock(spin) != CCMUTEX_SUCCESS) {
        ccatomic_pause();
    }
    return CCMUTEX_SUCCESS;
}

/* ================================================================== */
/*  ccspinlock_unlock                                                   */
/* ================================================================== */

ccmutex_state_t ccspinlock_unlock(ccspinlock_t* spin) {
    if (!spin) {
        return CCMUTEX_ERROR;
    }

    if (spin->recursion > 1) {
        spin->recursion--;
        return CCMUTEX_SUCCESS;
    }

    spin->owner     = NULL;
    spin->recursion = 0;

    /* RELEASE barrier before clearing flag */
    ccatomic_store_release(&spin->flag, 0);
    return CCMUTEX_SUCCESS;
}

/* ================================================================== */
/*  ccspinlock_destroy                                                  */
/* ================================================================== */

void ccspinlock_destroy(ccspinlock_t* spin) {
    free(spin);
}
