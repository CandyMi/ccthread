/**
 * @file      ccmutex.c
 * @author    candy <https://github.com/CandyMi/ccthread>
 * @brief     Recursive/non-recursive mutex implementation (part of ccthread)
 *
 * Windows PLAIN:        SRWLOCK (Vista+)
 * Windows RECURSIVE:    CRITICAL_SECTION
 * POSIX PLAIN:          pthread_mutex_t (PTHREAD_MUTEX_DEFAULT)
 * POSIX RECURSIVE:      pthread_mutex_t (PTHREAD_MUTEX_RECURSIVE)
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

struct ccmutex_impl {
#ifdef CCTHREAD_PLATFORM_WINDOWS
    int recursive;
    union {
        SRWLOCK          srw;      /* PLAIN mode */
        CRITICAL_SECTION cs;       /* RECURSIVE mode */
    } u;
#else
    pthread_mutex_t mutex;         /* attr: DEFAULT or RECURSIVE */
#endif
};

/* ================================================================== */
/*  ccmutex_create                                                      */
/* ================================================================== */

ccmutex_t* ccmutex_create(ccrecursion_t type) {
    ccmutex_t* mtx;

    mtx = (ccmutex_t*)calloc(1, sizeof(ccmutex_t));
    if (!mtx) {
        return NULL;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    mtx->recursive = (type != CCRECURSION_PLAIN);
    if (mtx->recursive) {
        InitializeCriticalSection(&mtx->u.cs);
    }
    /* PLAIN: SRWLOCK is zero-initialised by calloc */
#else
    {
        int rc;
        if (type != CCRECURSION_PLAIN) {
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            rc = pthread_mutex_init(&mtx->mutex, &attr);
            pthread_mutexattr_destroy(&attr);
        } else {
            rc = pthread_mutex_init(&mtx->mutex, NULL);
        }
        if (rc != 0) {
            free(mtx);
            return NULL;
        }
    }
#endif

    return mtx;
}

/* ================================================================== */
/*  ccmutex_trylock                                                     */
/* ================================================================== */

ccmutex_state_t ccmutex_trylock(ccmutex_t* mtx) {
    if (!mtx) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    if (mtx->recursive) {
        return TryEnterCriticalSection(&mtx->u.cs)
                   ? CCMUTEX_SUCCESS : CCMUTEX_ERROR;
    }
    return TryAcquireSRWLockExclusive(&mtx->u.srw)
               ? CCMUTEX_SUCCESS : CCMUTEX_ERROR;
#else
    return (pthread_mutex_trylock(&mtx->mutex) == 0)
               ? CCMUTEX_SUCCESS : CCMUTEX_ERROR;
#endif
}

/* ================================================================== */
/*  ccmutex_lock                                                        */
/* ================================================================== */

ccmutex_state_t ccmutex_lock(ccmutex_t* mtx) {
    if (!mtx) {
        return CCMUTEX_ERROR;
    }
    while (ccmutex_trylock(mtx) != CCMUTEX_SUCCESS) {
        ccthread_yield();
    }
    return CCMUTEX_SUCCESS;
}

/* ================================================================== */
/*  ccmutex_unlock                                                      */
/* ================================================================== */

ccmutex_state_t ccmutex_unlock(ccmutex_t* mtx) {
    if (!mtx) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    if (mtx->recursive) {
        LeaveCriticalSection(&mtx->u.cs);
    } else {
        ReleaseSRWLockExclusive(&mtx->u.srw);
    }
#else
    pthread_mutex_unlock(&mtx->mutex);
#endif
    return CCMUTEX_SUCCESS;
}

/* ================================================================== */
/*  ccmutex_destroy                                                     */
/* ================================================================== */

void ccmutex_destroy(ccmutex_t* mtx) {
    if (!mtx) {
        return;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    if (mtx->recursive) {
        DeleteCriticalSection(&mtx->u.cs);
    }
    /* SRWLOCK: no cleanup needed */
#else
    pthread_mutex_destroy(&mtx->mutex);
#endif

    free(mtx);
}

/* ================================================================== */
/*  ccmutex_is_recursive / ccmutex_native_handle (for cccondvar)       */
/* ================================================================== */

int ccmutex_is_recursive(const ccmutex_t* mtx) {
    if (!mtx) return 0;
#ifdef CCTHREAD_PLATFORM_WINDOWS
    return mtx->recursive;
#else
    return 0;
#endif
}

void* ccmutex_native_handle(ccmutex_t* mtx) {
    if (!mtx) return NULL;
#ifdef CCTHREAD_PLATFORM_WINDOWS
    if (mtx->recursive)
        return (void*)&mtx->u.cs;
    else
        return (void*)&mtx->u.srw;
#else
    return (void*)&mtx->mutex;
#endif
}
