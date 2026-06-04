/*
 * ccsem — Cross-platform C/C++ semaphore library
 *
 * Usage:
 *   #include "ccsem.h"
 *
 *   ccsem_t* sem = ccsem_create(0);   // start locked
 *   // ... in thread A:
 *   ccsem_wait(sem);                  // blocks until someone posts
 *   // ... in thread B:
 *   ccsem_post(sem);                  // wakes thread A
 *   ccsem_destroy(sem);
 */

#ifndef CCSEM_H
#define CCSEM_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Platform detection                                                 */
/* ------------------------------------------------------------------ */
#if defined(_WIN32) || defined(_WIN64)
  #define CCSEM_PLATFORM_WINDOWS 1
#else
  #define CCSEM_PLATFORM_POSIX   1
#endif

/* ---- platform headers (needed for struct fields) ---- */

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
#endif

/* ------------------------------------------------------------------ */
/*  Symbol export / import (MSVC / GCC visibility)                     */
/* ------------------------------------------------------------------ */

#if defined(_WIN32) || defined(_WIN64)
  #ifdef CCSEM_BUILD_DLL
    #define CCSEM_API __declspec(dllexport)
  #elif defined(CCSEM_USE_DLL)
    #define CCSEM_API __declspec(dllimport)
  #else
    #define CCSEM_API
  #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
  #define CCSEM_API __attribute__((visibility("default")))
#else
  #define CCSEM_API
#endif

/* ------------------------------------------------------------------ */
/*  Macros                                                             */
/* ------------------------------------------------------------------ */

/** Return codes */
#define CCSEM_SUCCESS       0
#define CCSEM_ERROR        (-1)
#define CCSEM_TIMEOUT      (-2)    /* trywait would block / timedwait expired */

/* ------------------------------------------------------------------ */
/*  Types                                                              */
/* ------------------------------------------------------------------ */

/**
 * Semaphore handle — heap-allocated by ccsem_create().
 */
typedef struct ccsem_impl {
#ifdef CCSEM_PLATFORM_WINDOWS
    HANDLE                handle;   /* Win32 semaphore handle */
#elif defined(__APPLE__)
    dispatch_semaphore_t  sem;      /* GCD semaphore */
#else
    pthread_mutex_t       mutex;    /* protects `count` and `cond` */
    pthread_cond_t        cond;     /* wait / signal condition */
    unsigned int          count;    /* current semaphore count */
#endif
} ccsem_t;

/* ------------------------------------------------------------------ */
/*  Semaphore API                                                      */
/* ------------------------------------------------------------------ */

/**
 * Create a semaphore with an initial count.
 *
 * @param initial_count  starting value (0 = locked, N = allow N waiters)
 * @return               new semaphore handle, or NULL on failure
 *
 * The handle MUST be released with ccsem_destroy().
 */
CCSEM_API ccsem_t* ccsem_create(unsigned int initial_count);

/**
 * Wait (P / down / decrement) on the semaphore.
 *
 * If the count is > 0, decrements and returns immediately.
 * If the count is 0, blocks until another thread calls ccsem_post().
 *
 * @param sem  semaphore handle (must not be NULL)
 * @return     CCSEM_SUCCESS, or CCSEM_ERROR on invalid handle
 */
CCSEM_API int ccsem_wait(ccsem_t* sem);

/**
 * Try to decrement the semaphore without blocking.
 *
 * @return CCSEM_SUCCESS if acquired, CCSEM_TIMEOUT if count is 0,
 *         CCSEM_ERROR on invalid handle
 */
CCSEM_API int ccsem_trywait(ccsem_t* sem);

/**
 * Wait with a timeout.
 *
 * @param timeout_ms  max milliseconds to wait; 0 behaves like trywait
 * @return  CCSEM_SUCCESS if acquired, CCSEM_TIMEOUT on expiry,
 *          CCSEM_ERROR on invalid handle
 */
CCSEM_API int ccsem_timedwait(ccsem_t* sem, unsigned int timeout_ms);

/**
 * Signal (V / up / increment / post) the semaphore.
 *
 * Increments the count and wakes exactly one blocked waiter.
 * If no thread is waiting, the count is incremented for the next waiter.
 *
 * @param sem  semaphore handle (must not be NULL)
 * @return     CCSEM_SUCCESS, or CCSEM_ERROR on invalid handle
 */
CCSEM_API int ccsem_post(ccsem_t* sem);

/**
 * Destroy a semaphore and free its resources.
 *
 * Safe to call with NULL (no-op).
 * Do NOT destroy a semaphore while threads are waiting on it.
 */
CCSEM_API void ccsem_destroy(ccsem_t* sem);

#ifdef __cplusplus
}
#endif

#endif /* CCSEM_H */
