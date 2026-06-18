/*
 * ccsem.c — Cross-platform C/C++ semaphore library implementation
 *
 * Windows : Win32 semaphore  (CreateSemaphore / WaitForSingleObject / ReleaseSemaphore)
 * macOS   : Grand Central Dispatch  (dispatch_semaphore)
 * POSIX   : pthread mutex + condition variable
 */

#include "ccsem.h"

#ifdef CCSEM_PLATFORM_WINDOWS
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#elif defined(__APPLE__)
  #include <dispatch/dispatch.h>
#else
  #include <pthread.h>
  #include <errno.h>
  #include <time.h>         /* clock_gettime / struct timespec */
#endif

#include <stdlib.h>

/* ================================================================== */
/*  ccsem_create                                                        */
/* ================================================================== */

ccsem_t* ccsem_create(unsigned int initial_count) {
    ccsem_t* sem;

    sem = (ccsem_t*)calloc(1, sizeof(ccsem_t));
    if (!sem) {
        return NULL;
    }

#ifdef CCSEM_PLATFORM_WINDOWS
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
    sem->sem = dispatch_semaphore_create((long)initial_count);
    if (!sem->sem) {
        free(sem);
        return NULL;
    }
#else
    /* pthread mutex + condvar (Linux / BSD / other POSIX) */
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
            pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
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

int ccsem_wait(ccsem_t* sem) {
    if (!sem) {
        return CCSEM_ERROR;
    }

#ifdef CCSEM_PLATFORM_WINDOWS
    {
        DWORD rc = WaitForSingleObject(sem->handle, INFINITE);
        return (rc == WAIT_OBJECT_0) ? CCSEM_SUCCESS : CCSEM_ERROR;
    }
#elif defined(__APPLE__)
    return (dispatch_semaphore_wait(sem->sem, DISPATCH_TIME_FOREVER) == 0)
               ? CCSEM_SUCCESS : CCSEM_ERROR;
#else
    {
        int rc;

        rc = pthread_mutex_lock(&sem->mutex);
        if (rc != 0) {
            return CCSEM_ERROR;
        }

        /* Loop to guard against spurious wakeups */
        while (sem->count == 0) {
            rc = pthread_cond_wait(&sem->cond, &sem->mutex);
            if (rc != 0) {
                pthread_mutex_unlock(&sem->mutex);
                return CCSEM_ERROR;
            }
        }

        sem->count--;
        pthread_mutex_unlock(&sem->mutex);
        return CCSEM_SUCCESS;
    }
#endif
}

/* ================================================================== */
/*  ccsem_trywait                                                       */
/* ================================================================== */

int ccsem_trywait(ccsem_t* sem) {
    if (!sem) {
        return CCSEM_ERROR;
    }

#ifdef CCSEM_PLATFORM_WINDOWS
    {
        DWORD rc = WaitForSingleObject(sem->handle, 0);
        if (rc == WAIT_OBJECT_0)  return CCSEM_SUCCESS;
        if (rc == WAIT_TIMEOUT)   return CCSEM_TIMEOUT;
        return CCSEM_ERROR;
    }
#elif defined(__APPLE__)
    return (dispatch_semaphore_wait(sem->sem, DISPATCH_TIME_NOW) == 0)
               ? CCSEM_SUCCESS : CCSEM_TIMEOUT;
#else
    {
        int rc = pthread_mutex_lock(&sem->mutex);
        if (rc != 0) {
            return CCSEM_ERROR;
        }
        if (sem->count > 0) {
            sem->count--;
            pthread_mutex_unlock(&sem->mutex);
            return CCSEM_SUCCESS;
        }
        pthread_mutex_unlock(&sem->mutex);
        return CCSEM_TIMEOUT;
    }
#endif
}

/* ================================================================== */
/*  ccsem_timedwait                                                     */
/* ================================================================== */

int ccsem_timedwait(ccsem_t* sem, unsigned int timeout_ms) {
    if (!sem) {
        return CCSEM_ERROR;
    }

#ifdef CCSEM_PLATFORM_WINDOWS
    {
        DWORD rc = WaitForSingleObject(sem->handle, (DWORD)timeout_ms);
        if (rc == WAIT_OBJECT_0)  return CCSEM_SUCCESS;
        if (rc == WAIT_TIMEOUT)   return CCSEM_TIMEOUT;
        return CCSEM_ERROR;
    }
#elif defined(__APPLE__)
    {
        dispatch_time_t deadline = dispatch_time(
            DISPATCH_TIME_NOW, (int64_t)timeout_ms * NSEC_PER_MSEC);
        return (dispatch_semaphore_wait(sem->sem, deadline) == 0)
                   ? CCSEM_SUCCESS : CCSEM_TIMEOUT;
    }
#else
    {
        int rc;

        rc = pthread_mutex_lock(&sem->mutex);
        if (rc != 0) {
            return CCSEM_ERROR;
        }

        /* Fast path: count already available */
        if (sem->count > 0) {
            sem->count--;
            pthread_mutex_unlock(&sem->mutex);
            return CCSEM_SUCCESS;
        }

        /* Compute absolute deadline */
        {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
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
                    return CCSEM_TIMEOUT;
                }
                if (rc != 0) {
                    pthread_mutex_unlock(&sem->mutex);
                    return CCSEM_ERROR;
                }
            }
        }

        sem->count--;
        pthread_mutex_unlock(&sem->mutex);
        return CCSEM_SUCCESS;
    }
#endif
}

/* ================================================================== */
/*  ccsem_post                                                          */
/* ================================================================== */

int ccsem_post(ccsem_t* sem) {
    if (!sem) {
        return CCSEM_ERROR;
    }

#ifdef CCSEM_PLATFORM_WINDOWS
    {
        BOOL ok = ReleaseSemaphore(sem->handle, 1, NULL);
        return ok ? CCSEM_SUCCESS : CCSEM_ERROR;
    }
#elif defined(__APPLE__)
    /* dispatch_semaphore_signal always succeeds; return value
     * indicates whether a waiter was woken (non-zero) or not. */
    dispatch_semaphore_signal(sem->sem);
    return CCSEM_SUCCESS;
#else
    {
        int rc;

        rc = pthread_mutex_lock(&sem->mutex);
        if (rc != 0) {
            return CCSEM_ERROR;
        }

        sem->count++;

        /* Wake one waiter */
        rc = pthread_cond_signal(&sem->cond);
        pthread_mutex_unlock(&sem->mutex);

        return (rc == 0) ? CCSEM_SUCCESS : CCSEM_ERROR;
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

#ifdef CCSEM_PLATFORM_WINDOWS
    if (sem->handle) {
        CloseHandle(sem->handle);
    }
#elif defined(__APPLE__)
    /* dispatch_release() is managed by the ObjC runtime on modern macOS;
     * calling it from pure C can hang.  The semaphore is reclaimed on
     * process exit — we only free the wrapper struct. */
    (void)sem->sem;
#else
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->cond);
#endif

    free(sem);
}
