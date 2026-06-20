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

/**
 * @brief Create a counting semaphore.
 *
 * @param[in]  initial_count  initial semaphore value (>= 0)
 * @return                    new handle on success
 * @retval NULL               allocation or platform init failed
 */
CCTHREAD_API ccsem_t*         ccsem_create(unsigned int initial_count);

/**
 * @brief Decrement the semaphore, blocking until > 0.
 *
 * @param[in]  sem  semaphore handle
 * @return           CCMUTEX_SUCCESS on success
 * @retval CCMUTEX_ERROR  @p sem is NULL
 */
CCTHREAD_API ccmutex_state_t  ccsem_wait(ccsem_t* sem);

/**
 * @brief Increment (signal) the semaphore.
 *
 * Wakes one waiting thread, if any.
 *
 * @param[in]  sem  semaphore handle
 * @return           CCMUTEX_SUCCESS on success
 * @retval CCMUTEX_ERROR  @p sem is NULL
 */
CCTHREAD_API ccmutex_state_t  ccsem_post(ccsem_t* sem);

/**
 * @brief Try to decrement without blocking.
 *
 * @param[in]  sem  semaphore handle
 * @return           CCMUTEX_SUCCESS if the count was > 0
 * @retval CCMUTEX_TIMEOUT  count was 0
 * @retval CCMUTEX_ERROR    @p sem is NULL
 */
CCTHREAD_API ccmutex_state_t  ccsem_trywait(ccsem_t* sem);

/**
 * @brief Decrement the semaphore with a timeout.
 *
 * @param[in]  sem         semaphore handle
 * @param[in]  timeout_ms  timeout in milliseconds
 * @return                  CCMUTEX_SUCCESS if decremented before timeout
 * @retval CCMUTEX_TIMEOUT  count was 0 for the entire timeout
 * @retval CCMUTEX_ERROR    @p sem is NULL
 */
CCTHREAD_API ccmutex_state_t  ccsem_timedwait(ccsem_t* sem, unsigned int timeout_ms);

/**
 * @brief Destroy a semaphore and free its memory.
 *
 * @param[in]  sem  semaphore handle, or NULL (safe no-op)
 *
 * @post The handle is invalid — do not use it after this call.
 */
CCTHREAD_API void             ccsem_destroy(ccsem_t* sem);

#ifdef __cplusplus
}
#endif

#endif /* CCSEM_H */
