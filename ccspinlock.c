/**
 * @file      ccspinlock.c
 * @author    candy <https://github.com/CandyMi/ccthread>
 * @brief     Busy-wait spinlock implementation (part of ccthread)
 *
 * Acquire: ACQUIRE memory order via atomic_flag_test_and_set.
 * Release: RELEASE memory order via atomic_flag_clear.
 *
 * Recursive mode tracks owner via ccthread_gettid.
 * PLAIN mode ignores owner fields (calloc-zeroed, never set).
 */

#include "ccmutex.h"
#include "ccthread.h"
#include "ccatomic.h"
#include <stdlib.h>

/* ---- opaque struct definition ---- */

struct ccspinlock_impl {
    volatile long    flag;          /* 0 = free, 1 = locked */
    uint32_t       owner_tid;     /* owner TID (recursive mode) */
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
    if (!spin) {
        return CCMUTEX_ERROR;
    }

    /* Recursive path: same thread already holds the lock */
    if (spin->owner_tid && spin->owner_tid == ccthread_gettid(NULL)) {
        spin->recursion++;
        return CCMUTEX_SUCCESS;
    }

    /* Atomic acquire with ACQUIRE barrier */
    if (ccatomic_exchange_acquire(&spin->flag, 1) == 0) {
        spin->owner_tid = ccthread_gettid(NULL);
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

    spin->owner_tid = 0;
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
