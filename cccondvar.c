/**
 * @file      cccondvar.c
 * @author    candy <https://github.com/CandyMi/ccthread>
 * @brief     Condition variable implementation (part of ccthread)
 *
 * Windows:   CONDITION_VARIABLE (Vista+)
 * POSIX:     pthread_cond_t
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
  #include <time.h>
  #include <errno.h>
#endif

#include <stdlib.h>

/* ---- CLOCK_MONOTONIC portability ---- */
#ifndef CLOCK_MONOTONIC
  #if defined(CLOCK_HIGHRES)
    #define CCCONDVAR_CLOCK_ID CLOCK_HIGHRES
  #else
    #define CCCONDVAR_CLOCK_ID CLOCK_REALTIME
  #endif
#else
  #define CCCONDVAR_CLOCK_ID CLOCK_MONOTONIC
#endif

/* ---- opaque struct definition ---- */

struct cccondvar_impl {
#ifdef CCTHREAD_PLATFORM_WINDOWS
    CONDITION_VARIABLE  cv;
#else
    pthread_cond_t      cond;
#endif
};

/* ================================================================== */
/*  cccondvar_create                                                    */
/* ================================================================== */

cccondvar_t* cccondvar_create(void) {
    cccondvar_t* cv;

    cv = (cccondvar_t*)calloc(1, sizeof(cccondvar_t));
    if (!cv) {
        return NULL;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    InitializeConditionVariable(&cv->cv);
#else
    {
        int rc;
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
#ifndef __APPLE__
        /* Set monotonic clock so timedwait is immune to system clock changes.
         * macOS condattr does not support CLOCK_MONOTONIC; macOS uses
         * pthread_cond_timedwait_relative_np instead (see timedwait). */
        pthread_condattr_setclock(&attr, CCCONDVAR_CLOCK_ID);
#endif
        rc = pthread_cond_init(&cv->cond, &attr);
        pthread_condattr_destroy(&attr);
        if (rc != 0) {
            free(cv);
            return NULL;
        }
    }
#endif

    return cv;
}

/* ================================================================== */
/*  cccondvar_wait                                                      */
/* ================================================================== */

ccmutex_state_t cccondvar_wait(cccondvar_t* cv, ccmutex_t* mtx) {
    if (!cv || !mtx) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    if (ccmutex_is_recursive(mtx)) {
        if (!SleepConditionVariableCS(&cv->cv,
                (PCRITICAL_SECTION)ccmutex_native_handle(mtx), INFINITE))
            return CCMUTEX_ERROR;
    } else {
        if (!SleepConditionVariableSRW(&cv->cv,
                (PSRWLOCK)ccmutex_native_handle(mtx), INFINITE, 0))
            return CCMUTEX_ERROR;
    }
    return CCMUTEX_SUCCESS;
#else
    return (pthread_cond_wait(&cv->cond,
                (pthread_mutex_t*)ccmutex_native_handle(mtx)) == 0)
               ? CCMUTEX_SUCCESS : CCMUTEX_ERROR;
#endif
}

/* ================================================================== */
/*  cccondvar_timedwait                                                 */
/* ================================================================== */

ccmutex_state_t cccondvar_timedwait(cccondvar_t* cv, ccmutex_t* mtx,
                                     unsigned int timeout_ms) {
    if (!cv || !mtx) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    {
        BOOL ok;
        if (ccmutex_is_recursive(mtx))
            ok = SleepConditionVariableCS(&cv->cv,
                    (PCRITICAL_SECTION)ccmutex_native_handle(mtx),
                    (DWORD)timeout_ms);
        else
            ok = SleepConditionVariableSRW(&cv->cv,
                    (PSRWLOCK)ccmutex_native_handle(mtx),
                    (DWORD)timeout_ms, 0);
        if (ok) return CCMUTEX_SUCCESS;
        return (GetLastError() == WAIT_TIMEOUT)
                   ? CCMUTEX_TIMEOUT : CCMUTEX_ERROR;
    }
#else
    {
        int rc;

#ifdef __APPLE__
        /* macOS: pthread_cond_timedwait_relative_np takes a relative
         * timeout — no clock dependency, immune to wall-clock changes.
         * Available since macOS 10.6, never deprecated. */
        {
            struct timespec ts;
            ts.tv_sec  = (time_t)(timeout_ms / 1000);
            ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
            rc = pthread_cond_timedwait_relative_np(&cv->cond,
                    (pthread_mutex_t*)ccmutex_native_handle(mtx), &ts);
        }
#else
        /* Linux/BSD: condattr was set to CCCONDVAR_CLOCK_ID (MONOTONIC)
         * in cccondvar_create — compute absolute time from it. */
        {
            struct timespec ts;
            clock_gettime(CCCONDVAR_CLOCK_ID, &ts);
            ts.tv_sec  += (time_t)(timeout_ms / 1000);
            ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000L;
            }
            rc = pthread_cond_timedwait(&cv->cond,
                    (pthread_mutex_t*)ccmutex_native_handle(mtx), &ts);
        }
#endif
        if (rc == 0)         return CCMUTEX_SUCCESS;
        if (rc == ETIMEDOUT) return CCMUTEX_TIMEOUT;
        return CCMUTEX_ERROR;
    }
#endif
}

/* ================================================================== */
/*  cccondvar_signal                                                    */
/* ================================================================== */

ccmutex_state_t cccondvar_signal(cccondvar_t* cv) {
    if (!cv) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    WakeConditionVariable(&cv->cv);
    return CCMUTEX_SUCCESS;
#else
    return (pthread_cond_signal(&cv->cond) == 0)
               ? CCMUTEX_SUCCESS : CCMUTEX_ERROR;
#endif
}

/* ================================================================== */
/*  cccondvar_broadcast                                                 */
/* ================================================================== */

ccmutex_state_t cccondvar_broadcast(cccondvar_t* cv) {
    if (!cv) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    WakeAllConditionVariable(&cv->cv);
    return CCMUTEX_SUCCESS;
#else
    return (pthread_cond_broadcast(&cv->cond) == 0)
               ? CCMUTEX_SUCCESS : CCMUTEX_ERROR;
#endif
}

/* ================================================================== */
/*  cccondvar_destroy                                                   */
/* ================================================================== */

void cccondvar_destroy(cccondvar_t* cv) {
    if (!cv) {
        return;
    }

#ifndef CCTHREAD_PLATFORM_WINDOWS
    pthread_cond_destroy(&cv->cond);
#endif

    free(cv);
}
