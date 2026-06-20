/**
 * @file      ccsem.c
 * @author    candy <https://github.com/CandyMi/ccthread>
 * @brief     Counting semaphore implementation (part of ccthread library)
 *
 * Windows : Win32 semaphore  (CreateSemaphore / WaitForSingleObject / ReleaseSemaphore)
 * macOS   : Grand Central Dispatch  (dispatch_semaphore)
 * POSIX   : pthread mutex + condition variable
 */

#if defined(_WIN32) || defined(_WIN64)
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
#endif

#include "ccsem.h"
#include "ccthread.h"

#ifdef CCTHREAD_PLATFORM_WINDOWS
  #include <windows.h>
#elif defined(__APPLE__)
  #include <dispatch/dispatch.h>
#else
  #include <pthread.h>
  #include <errno.h>
  #include <time.h>
#endif

#include <stdlib.h>

/* ---- CLOCK_MONOTONIC portability (HP-UX < 11i v3 uses CLOCK_HIGHRES) ---- */
#ifndef CLOCK_MONOTONIC
  #if defined(CLOCK_HIGHRES)
    #define CCMUTEX_CLOCK_ID CLOCK_HIGHRES
  #else
    #define CCMUTEX_CLOCK_ID CLOCK_REALTIME
  #endif
#else
  #define CCMUTEX_CLOCK_ID CLOCK_MONOTONIC
#endif

/* ---- opaque struct definition ---- */

struct ccsem_impl {
#ifdef CCTHREAD_PLATFORM_WINDOWS
    HANDLE                handle;
#elif defined(__APPLE__)
    dispatch_semaphore_t  sem;
#else
    pthread_mutex_t       mutex;
    pthread_cond_t        cond;
    unsigned int          count;
#endif
};

/* ================================================================== */
/*  ccsem_create                                                        */
/* ================================================================== */

ccsem_t* ccsem_create(unsigned int initial_count) {
    ccsem_t* sem;

    sem = (ccsem_t*)calloc(1, sizeof(ccsem_t));
    if (!sem) {
        return NULL;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    /* CreateSemaphoreW uses LONG (32-bit signed).  Clamp to LONG_MAX
     * so values > 2^31-1 don't wrap negative through the (LONG) cast
     * and cause silent failure.  The POSIX backend can handle
     * the full unsigned int range. */
    if (initial_count > LONG_MAX) initial_count = LONG_MAX;

    sem->handle = CreateSemaphoreW(
        NULL,                    /* default security */
        (LONG)initial_count,     /* initial count */
        LONG_MAX,                /* max count (effectively unlimited) */
        NULL                     /* unnamed */
    );

    if (!sem->handle) {
        free(sem);
        return NULL;
    }
#elif defined(__APPLE__)
    /* dispatch_semaphore_create takes long (64-bit on modern macOS,
     * 32-bit on older).  Clamp at LONG_MAX via its literal value to
     * avoid silent truncation on 32-bit macOS. */
    if ((unsigned long)initial_count > 2147483647U) initial_count = 2147483647U;
    sem->sem = dispatch_semaphore_create((long)initial_count);
    if (!sem->sem) {
        free(sem);
        return NULL;
    }
#else
    {
        int rc;

        rc = pthread_mutex_init(&sem->mutex, NULL);
        if (rc != 0) {
            free(sem);
            return NULL;
        }

        {
            pthread_condattr_t attr;
            pthread_condattr_init(&attr);
            pthread_condattr_setclock(&attr, CCMUTEX_CLOCK_ID);
            rc = pthread_cond_init(&sem->cond, &attr);
            pthread_condattr_destroy(&attr);
        }
        if (rc != 0) {
            pthread_mutex_destroy(&sem->mutex);
            free(sem);
            return NULL;
        }

        sem->count = initial_count;
    }
#endif

    return sem;
}

/* ================================================================== */
/*  ccsem_wait                                                          */
/* ================================================================== */

ccmutex_state_t ccsem_wait(ccsem_t* sem) {
    if (!sem) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    {
        DWORD rc = WaitForSingleObject(sem->handle, INFINITE);
        return (rc == WAIT_OBJECT_0) ? CCMUTEX_SUCCESS : CCMUTEX_ERROR;
    }
#elif defined(__APPLE__)
    return (dispatch_semaphore_wait(sem->sem, DISPATCH_TIME_FOREVER) == 0)
               ? CCMUTEX_SUCCESS : CCMUTEX_ERROR;
#else
    {
        int rc;

        rc = pthread_mutex_lock(&sem->mutex);
        if (rc != 0) {
            return CCMUTEX_ERROR;
        }

        while (sem->count == 0) {
            rc = pthread_cond_wait(&sem->cond, &sem->mutex);
            if (rc != 0) {
                pthread_mutex_unlock(&sem->mutex);
                return CCMUTEX_ERROR;
            }
        }

        sem->count--;
        pthread_mutex_unlock(&sem->mutex);
        return CCMUTEX_SUCCESS;
    }
#endif
}

/* ================================================================== */
/*  ccsem_trywait                                                       */
/* ================================================================== */

ccmutex_state_t ccsem_trywait(ccsem_t* sem) {
    if (!sem) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    {
        DWORD rc = WaitForSingleObject(sem->handle, 0);
        if (rc == WAIT_OBJECT_0)  return CCMUTEX_SUCCESS;
        if (rc == WAIT_TIMEOUT)   return CCMUTEX_TIMEOUT;
        return CCMUTEX_ERROR;
    }
#elif defined(__APPLE__)
    return (dispatch_semaphore_wait(sem->sem, DISPATCH_TIME_NOW) == 0)
               ? CCMUTEX_SUCCESS : CCMUTEX_TIMEOUT;
#else
    {
        int rc = pthread_mutex_lock(&sem->mutex);
        if (rc != 0) {
            return CCMUTEX_ERROR;
        }
        if (sem->count > 0) {
            sem->count--;
            pthread_mutex_unlock(&sem->mutex);
            return CCMUTEX_SUCCESS;
        }
        pthread_mutex_unlock(&sem->mutex);
        return CCMUTEX_TIMEOUT;
    }
#endif
}

/* ================================================================== */
/*  ccsem_timedwait                                                     */
/* ================================================================== */

ccmutex_state_t ccsem_timedwait(ccsem_t* sem, unsigned int timeout_ms) {
    if (!sem) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    {
        DWORD rc = WaitForSingleObject(sem->handle, (DWORD)timeout_ms);
        if (rc == WAIT_OBJECT_0)  return CCMUTEX_SUCCESS;
        if (rc == WAIT_TIMEOUT)   return CCMUTEX_TIMEOUT;
        return CCMUTEX_ERROR;
    }
#elif defined(__APPLE__)
    {
        dispatch_time_t deadline = dispatch_time(
            DISPATCH_TIME_NOW, (int64_t)timeout_ms * NSEC_PER_MSEC);
        return (dispatch_semaphore_wait(sem->sem, deadline) == 0)
                   ? CCMUTEX_SUCCESS : CCMUTEX_TIMEOUT;
    }
#else
    {
        int rc;

        rc = pthread_mutex_lock(&sem->mutex);
        if (rc != 0) {
            return CCMUTEX_ERROR;
        }

        if (sem->count > 0) {
            sem->count--;
            pthread_mutex_unlock(&sem->mutex);
            return CCMUTEX_SUCCESS;
        }

        {
            struct timespec ts;
            clock_gettime(CCMUTEX_CLOCK_ID, &ts);
            ts.tv_sec  += (time_t)(timeout_ms / 1000);
            ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
            if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000L;
            }

            while (sem->count == 0) {
                rc = pthread_cond_timedwait(&sem->cond, &sem->mutex, &ts);
                if (rc == ETIMEDOUT) {
                    pthread_mutex_unlock(&sem->mutex);
                    return CCMUTEX_TIMEOUT;
                }
                if (rc != 0) {
                    pthread_mutex_unlock(&sem->mutex);
                    return CCMUTEX_ERROR;
                }
            }
        }

        sem->count--;
        pthread_mutex_unlock(&sem->mutex);
        return CCMUTEX_SUCCESS;
    }
#endif
}

/* ================================================================== */
/*  ccsem_post                                                          */
/* ================================================================== */

ccmutex_state_t ccsem_post(ccsem_t* sem) {
    if (!sem) {
        return CCMUTEX_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    {
        BOOL ok = ReleaseSemaphore(sem->handle, 1, NULL);
        return ok ? CCMUTEX_SUCCESS : CCMUTEX_ERROR;
    }
#elif defined(__APPLE__)
    dispatch_semaphore_signal(sem->sem);
    return CCMUTEX_SUCCESS;
#else
    {
        int rc;

        rc = pthread_mutex_lock(&sem->mutex);
        if (rc != 0) {
            return CCMUTEX_ERROR;
        }

        sem->count++;

        rc = pthread_cond_signal(&sem->cond);
        pthread_mutex_unlock(&sem->mutex);

        return (rc == 0) ? CCMUTEX_SUCCESS : CCMUTEX_ERROR;
    }
#endif
}

/* ================================================================== */
/*  ccsem_destroy                                                       */
/* ================================================================== */

void ccsem_destroy(ccsem_t* sem) {
    if (!sem) {
        return;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    if (sem->handle) {
        CloseHandle(sem->handle);
    }
#elif defined(__APPLE__)
    dispatch_release(sem->sem);
#else
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->cond);
#endif

    free(sem);
}
