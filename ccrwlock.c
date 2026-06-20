/**
 * @file      ccrwlock.c
 * @author    candy <https://github.com/CandyMi/ccthread>
 * @brief     Read-write lock implementation (part of ccthread)
 *
 * Writer-preferring (writers are not starved by a stream of readers).
 *
 * Windows: SRWLOCK + manual writer tracking via ccthread_gettid.
 *   unlock() auto-detects whether caller is the writer or a reader.
 *
 * POSIX:   pthread_rwlock_t (writer-preferring by default on glibc/macOS).
 */

#if defined(_WIN32) || defined(_WIN64)
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
#endif

#include "ccmutex.h"
#include "ccthread.h"

#ifdef CCTHREAD_PLATFORM_WINDOWS
  #include <windows.h>
#else
  #include <pthread.h>
#endif

#include <stdlib.h>

/* ---- opaque struct definition ---- */

struct ccrwlock_impl {
#ifdef CCTHREAD_PLATFORM_WINDOWS
    SRWLOCK          srw;
    uint32_t         writer_tid;   /* current writer TID, or 0 */
#else
    pthread_rwlock_t rwlock;
#endif
};

/* ================================================================== */
/*  ccrwlock_create                                                     */
/* ================================================================== */

ccrwlock_t* ccrwlock_create(void) {
    ccrwlock_t* rw;

    rw = (ccrwlock_t*)calloc(1, sizeof(ccrwlock_t));
    if (!rw) {
        return NULL;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    InitializeSRWLock(&rw->srw);
    /* writer_tid = 0, reader_count = 0 from calloc */
#else
    {
        int rc = pthread_rwlock_init(&rw->rwlock, NULL);
        if (rc != 0) {
            free(rw);
            return NULL;
        }
    }
#endif

    return rw;
}

/* ================================================================== */
/*  ccrwlock_rdlock                                                     */
/* ================================================================== */

ccmutex_state_t ccrwlock_rdlock(ccrwlock_t* rw) {
    if (!rw) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    AcquireSRWLockShared(&rw->srw);
#else
    {
        int rc = pthread_rwlock_rdlock(&rw->rwlock);
        if (rc != 0) return CCMUTEX_ERROR;
    }
#endif
    return CCMUTEX_SUCCESS;
}

/* ================================================================== */
/*  ccrwlock_tryrdlock                                                  */
/* ================================================================== */

ccmutex_state_t ccrwlock_tryrdlock(ccrwlock_t* rw) {
    if (!rw) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    if (!TryAcquireSRWLockShared(&rw->srw)) {
        return CCMUTEX_ERROR;
    }
    return CCMUTEX_SUCCESS;
#else
    return (pthread_rwlock_tryrdlock(&rw->rwlock) == 0)
               ? CCMUTEX_SUCCESS : CCMUTEX_ERROR;
#endif
}

/* ================================================================== */
/*  ccrwlock_wrlock                                                     */
/* ================================================================== */

ccmutex_state_t ccrwlock_wrlock(ccrwlock_t* rw) {
    if (!rw) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    AcquireSRWLockExclusive(&rw->srw);
    rw->writer_tid = ccthread_gettid(NULL);
#else
    {
        int rc = pthread_rwlock_wrlock(&rw->rwlock);
        if (rc != 0) return CCMUTEX_ERROR;
    }
#endif
    return CCMUTEX_SUCCESS;
}

/* ================================================================== */
/*  ccrwlock_trywrlock                                                  */
/* ================================================================== */

ccmutex_state_t ccrwlock_trywrlock(ccrwlock_t* rw) {
    if (!rw) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    if (!TryAcquireSRWLockExclusive(&rw->srw)) {
        return CCMUTEX_ERROR;
    }
    rw->writer_tid = ccthread_gettid(NULL);
    return CCMUTEX_SUCCESS;
#else
    return (pthread_rwlock_trywrlock(&rw->rwlock) == 0)
               ? CCMUTEX_SUCCESS : CCMUTEX_ERROR;
#endif
}

/* ================================================================== */
/*  ccrwlock_unlock                                                     */
/* ================================================================== */

ccmutex_state_t ccrwlock_unlock(ccrwlock_t* rw) {
    if (!rw) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    /* Auto-detect: writer or reader?
     * writer_owner tracks the exclusive lock holder (NULL = no writer).
     * If caller is the writer → exclusive release; otherwise → shared release.
     * No reader_count needed — SRWLOCK tracks shared ownership internally. */
    if (rw->writer_tid && rw->writer_tid == ccthread_gettid(NULL)) {
        rw->writer_tid = 0;
        ReleaseSRWLockExclusive(&rw->srw);
    } else {
        ReleaseSRWLockShared(&rw->srw);
    }
#else
    {
        int rc = pthread_rwlock_unlock(&rw->rwlock);
        if (rc != 0) return CCMUTEX_ERROR;
    }
#endif
    return CCMUTEX_SUCCESS;
}

/* ================================================================== */
/*  ccrwlock_destroy                                                    */
/* ================================================================== */

void ccrwlock_destroy(ccrwlock_t* rw) {
    if (!rw) {
        return;
    }

#ifndef CCTHREAD_PLATFORM_WINDOWS
    pthread_rwlock_destroy(&rw->rwlock);
#endif

    free(rw);
}
