/*
 * ccthread — Cross-platform C/C++ thread library
 *
 * Usage:
 *   #include "ccthread.h"
 *
 *   void* worker(void* arg) {
 *       int* n = (int*)arg;
 *       (*n)++;
 *       return n;
 *   }
 *
 *   int main(void) {
 *       int  val = 0;
 *       ccthread_t* th = ccthread_create(worker, &val);
 *       // if (!th) handle error ...
 *
 *       void* ret = NULL;
 *       ccthread_join(th, &ret);   // blocks, then auto-destroys th
 *       // val == 1, ret == &val
 *       return 0;
 *   }
 *
 * Lifecycle summary:
 *
 *   create() --> join()    (auto-destroy)
 *            \
 *             -> detach()  (auto-destroy)
 *
 *   self() --> destroy()   (manual destroy)
 */

#ifndef CCTHREAD_H
#define CCTHREAD_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Platform detection                                                 */
/* ------------------------------------------------------------------ */
#if defined(_WIN32) || defined(_WIN64)
  #define CCTHREAD_PLATFORM_WINDOWS 1
#else
  #define CCTHREAD_PLATFORM_POSIX   1
#endif

/* ---- platform headers (needed for struct fields) ---- */

#ifdef CCTHREAD_PLATFORM_WINDOWS
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#else
  #include <pthread.h>
#endif

/* ------------------------------------------------------------------ */
/*  Symbol export / import (MSVC / GCC visibility)                     */
/* ------------------------------------------------------------------ */

#if defined(_WIN32) || defined(_WIN64)
  #ifdef CCTHREAD_BUILD_DLL
    #define CCTHREAD_API __declspec(dllexport)
  #elif defined(CCTHREAD_USE_DLL)
    #define CCTHREAD_API __declspec(dllimport)
  #else
    #define CCTHREAD_API
  #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
  #define CCTHREAD_API __attribute__((visibility("default")))
#else
  #define CCTHREAD_API
#endif

/* ------------------------------------------------------------------ */
/*  Macros                                                             */
/* ------------------------------------------------------------------ */

/** Return codes */
#define CCTHREAD_SUCCESS       0
#define CCTHREAD_ERROR        (-1)

/** Max length for thread name (including null terminator) */
#define CCTHREAD_NAME_MAX      16

/* ------------------------------------------------------------------ */
/*  Types                                                              */
/* ------------------------------------------------------------------ */

/**
 * Thread entry-point signature.
 * @param arg  user-supplied argument passed to ccthread_create()
 * @return     arbitrary pointer retrieved via ccthread_join()
 */
typedef void* (*ccthread_func_t)(void* arg);

/**
 * Thread handle — heap-allocated by ccthread_create() / ccthread_self().
 * Fields are readable but should only be modified through the API.
 */
typedef struct ccthread_impl {
#ifdef CCTHREAD_PLATFORM_WINDOWS
    HANDLE           handle;     /* Win32 thread handle (NULL if closed) */
    DWORD            tid;        /* thread id for comparison */
#else
    pthread_t        handle;     /* POSIX thread id */
#endif

    ccthread_func_t  func;       /* user thread function */
    void*            arg;        /* user argument */
    void*            result;     /* return value captured on exit */

    volatile int     detached;   /* non-zero when ccthread_detach() called */
    volatile int     joined;     /* non-zero when ccthread_join() completed */
    volatile int     finished;   /* non-zero when wrapper has exited */

    int              is_self;    /* non-zero if created by ccthread_self() —
                                    join / detach are forbidden on these */
} ccthread_t;

/* ------------------------------------------------------------------ */
/*  Thread lifecycle API                                               */
/* ------------------------------------------------------------------ */

/**
 * Create and start a new thread.
 *
 * @param func  entry-point function (must not be NULL)
 * @param arg   argument forwarded to `func`
 * @return      new thread handle, or NULL on failure
 *
 * The returned handle MUST eventually be consumed by exactly one of:
 * - ccthread_join()   (auto-destroys after waiting)
 * - ccthread_detach() (auto-destroys after detaching)
 */
CCTHREAD_API ccthread_t* ccthread_create(ccthread_func_t func, void* arg);

/**
 * Wait for a thread to finish and retrieve its return value.
 *
 * @param thread  handle from ccthread_create() (must not be detached/joined)
 * @param result  output: return value from the thread function (may be NULL)
 * @return        CCTHREAD_SUCCESS, or CCTHREAD_ERROR on bad handle
 *
 * After a successful join the handle is DESTROYED — do not use it again.
 */
CCTHREAD_API int ccthread_join(ccthread_t* thread, void** result);

/**
 * Detach a thread so its resources are reclaimed automatically on exit.
 *
 * @param thread  handle from ccthread_create()
 * @return        CCTHREAD_SUCCESS, or CCTHREAD_ERROR on bad handle
 *
 * The handle is consumed — do not use it after calling this.
 * You can NOT join a detached thread.
 */
CCTHREAD_API int ccthread_detach(ccthread_t* thread);

/**
 * Manually destroy a thread handle.
 *
 * ONLY use this for handles returned by ccthread_self().
 * Handles returned by ccthread_create() are automatically destroyed
 * by ccthread_join() or ccthread_detach() — do NOT call this on them.
 *
 * Safe to call with NULL (no-op).
 */
CCTHREAD_API void ccthread_destroy(ccthread_t* thread);

/**
 * Exit the calling thread.
 *
 * Equivalent to `return result;` from the thread function, but can
 * be called from any depth in the call stack.
 */
CCTHREAD_API void ccthread_exit(void* result);

/**
 * Yield the CPU to allow other threads to run.
 */
CCTHREAD_API void ccthread_yield(void);

/**
 * Suspend the calling thread for at least `ms` milliseconds.
 */
CCTHREAD_API void ccthread_sleep(unsigned int ms);

/* ------------------------------------------------------------------ */
/*  Thread identification                                              */
/* ------------------------------------------------------------------ */

/**
 * Get a handle representing the calling thread.
 *
 * @return  new handle, or NULL on failure
 *
 * The returned handle MUST be freed with ccthread_destroy().
 * It can only be used for identification / comparison — you cannot
 * join or detach it.
 */
CCTHREAD_API ccthread_t* ccthread_self(void);

/**
 * Compare two thread handles for equality.
 *
 * @return  non-zero if `a` and `b` refer to the same OS thread,
 *          0 otherwise or if either is NULL
 */
CCTHREAD_API int ccthread_equal(ccthread_t* a, ccthread_t* b);

/* ------------------------------------------------------------------ */
/*  Thread naming (debugging / profiling)                              */
/* ------------------------------------------------------------------ */

/**
 * Assign a human-readable name to a thread.
 *
 * @param thread  handle to name, or NULL to name the calling thread
 * @param name    null-terminated string; silently truncated to
 *                CCTHREAD_NAME_MAX-1 characters
 * @return        CCTHREAD_SUCCESS, or CCTHREAD_ERROR on failure
 *
 * Platform notes:
 * - Windows 10+ and Linux: can name any thread
 * - macOS: can only name the calling thread (pass NULL, or a handle
 *   returned by ccthread_self() for the current thread)
 * - Other platforms: returns CCTHREAD_ERROR (unsupported)
 */
CCTHREAD_API int ccthread_set_name(ccthread_t* thread, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* CCTHREAD_H */
