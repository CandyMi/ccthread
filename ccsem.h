/**
 * @file        ccsem.h
 * @author      candy <https://github.com/CandyMi/ccthread>
 * @brief       Counting semaphore — part of the ccthread concurrency library
 *
 * Blocking, non-blocking, and timed-wait operations.
 *
 * @par Backends
 * | Platform        | Implementation |
 * |-----------------|----------------|
 * | Windows         | Win32 `CreateSemaphore` |
 * | macOS           | GCD `dispatch_semaphore` |
 * | Linux / BSD     | `pthread_mutex` + `pthread_cond` with `CLOCK_MONOTONIC` |
 */

#ifndef CCSEM_H
#define CCSEM_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- standalone export macro (guarded — OK if ccmutex.h / ccthread.h already defined it) ---- */
#if defined(_WIN32) || defined(_WIN64)
  #ifndef CCTHREAD_API
    #ifdef CCTHREAD_BUILD_DLL
      #define CCTHREAD_API __declspec(dllexport)
    #elif defined(CCTHREAD_USE_DLL)
      #define CCTHREAD_API __declspec(dllimport)
    #else
      #define CCTHREAD_API
    #endif
  #endif
#else
  #ifndef CCTHREAD_API
    #if defined(__GNUC__) && __GNUC__ >= 4
      #define CCTHREAD_API __attribute__((visibility("default")))
    #else
      #define CCTHREAD_API
    #endif
  #endif
#endif

/* ---- shared return type (guarded — OK if ccmutex.h already defined it) ---- */
#ifndef CCMUTEX_STATE_T_DEFINED
#define CCMUTEX_STATE_T_DEFINED
typedef enum {
    CCMUTEX_SUCCESS =  0,
    CCMUTEX_ERROR   = -1,
    CCMUTEX_TIMEOUT = -2
} ccmutex_state_t;
#endif

/* ------------------------------------------------------------------ */
/*  Types                                                              */
/* ------------------------------------------------------------------ */

/** @brief Semaphore handle. Opaque — heap-allocated by ccsem_create(). */
typedef struct ccsem_impl ccsem_t;

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

CCTHREAD_API ccsem_t*         ccsem_create(unsigned int initial_count);
CCTHREAD_API ccmutex_state_t  ccsem_wait(ccsem_t* sem);
CCTHREAD_API ccmutex_state_t  ccsem_post(ccsem_t* sem);
CCTHREAD_API ccmutex_state_t  ccsem_trywait(ccsem_t* sem);
CCTHREAD_API ccmutex_state_t  ccsem_timedwait(ccsem_t* sem, unsigned int timeout_ms);
CCTHREAD_API void             ccsem_destroy(ccsem_t* sem);

#ifdef __cplusplus
}
#endif

#endif /* CCSEM_H */
