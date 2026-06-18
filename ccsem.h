/**
 * @file        ccsem.h
 * @brief       Cross-platform C/C++ counting semaphore library
 *
 * Single-header API for counting semaphores with blocking, non-blocking,
 * and timed wait operations.  Zero external dependencies.
 *
 * @par Backends
 * | Platform        | Implementation |
 * |-----------------|----------------|
 * | Windows          | Win32 `CreateSemaphore` |
 * | macOS            | GCD `dispatch_semaphore` |
 * | Linux / BSD      | `pthread_mutex` + `pthread_cond` with `CLOCK_MONOTONIC` |
 *
 * @see API.md  — complete function reference
 * @see DESIGN.md  — backend selection rationale and performance analysis
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
  #define CCSEM_PLATFORM_WINDOWS 1   /**< Windows (MSVC / MinGW) */
#else
  #define CCSEM_PLATFORM_POSIX   1   /**< POSIX (Linux / macOS / BSD) */
#endif

/* ---- struct is opaque (definition lives in ccsem.c) ---- */
/* ---- platform headers are internal to the implementation ---- */

/* ------------------------------------------------------------------ */
/*  Symbol export / import (MSVC / GCC visibility)                     */
/* ------------------------------------------------------------------ */

#if defined(_WIN32) || defined(_WIN64)
  #ifdef CCSEM_BUILD_DLL
    #define CCSEM_API __declspec(dllexport)      /**< Export from DLL */
  #elif defined(CCSEM_USE_DLL)
    #define CCSEM_API __declspec(dllimport)       /**< Import from DLL */
  #else
    #define CCSEM_API                             /**< Static link */
  #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
  #define CCSEM_API __attribute__((visibility("default")))   /**< Shared lib export */
#else
  #define CCSEM_API
#endif

/* ------------------------------------------------------------------ */
/*  Macros                                                             */
/* ------------------------------------------------------------------ */

/** @brief Semaphore acquired successfully. */
#define CCSEM_SUCCESS       0

/** @brief Operation failed (invalid argument, OS error, etc.). */
#define CCSEM_ERROR        (-1)

/**
 * @brief Non-blocking wait would have blocked, or timed wait expired.
 *
 * Returned by ccsem_trywait() when count == 0, and by ccsem_timedwait()
 * when the deadline elapses before the semaphore is signalled.
 */
#define CCSEM_TIMEOUT      (-2)

/* ------------------------------------------------------------------ */
/*  @defgroup ccsem_types  Types                                       */
/*  @{                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Semaphore handle.
 *
 * Opaque struct — heap-allocated by ccsem_create(); must be freed with
 * ccsem_destroy().  All access is through the public API functions.
 *
 * @note The full struct definition is in ccsem.c to avoid exposing
 *       platform-specific headers (Win32 / GCD / pthread) to the consumer.
 */
typedef struct ccsem_impl ccsem_t;

/** @} */ /* end of ccsem_types */

/* ------------------------------------------------------------------ */
/*  @defgroup ccsem_blocking  Blocking operations                      */
/*  @{                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Create a semaphore with an initial count.
 *
 * @param[in]  initial_count  starting value — 0 means locked,
 *                            N means up to N waiters can pass without blocking
 * @return                    new semaphore handle on success
 * @retval NULL                memory allocation failed
 *
 * @note The returned handle must be released with ccsem_destroy().
 */
CCSEM_API ccsem_t* ccsem_create(unsigned int initial_count);

/**
 * @brief Wait on the semaphore (P / decrement / "down").
 *
 * If the count is > 0 it is atomically decremented and the call
 * returns immediately.  If the count is 0 the calling thread
 * blocks until another thread calls ccsem_post().
 *
 * @param[in]  sem  semaphore handle (must not be NULL)
 * @return          CCSEM_SUCCESS once the semaphore is acquired
 * @retval CCSEM_ERROR  @p sem is NULL
 *
 * @note This function blocks indefinitely — there is no timeout.
 *       Use ccsem_trywait() or ccsem_timedwait() for bounded waits.
 */
CCSEM_API int ccsem_wait(ccsem_t* sem);

/**
 * @brief Signal the semaphore (V / increment / "up" / post).
 *
 * Atomically increments the count.  If any threads are blocked in
 * ccsem_wait() / ccsem_trywait() / ccsem_timedwait(), exactly one
 * is woken.  If no threads are waiting, the count is simply incremented
 * so the next waiter can pass.
 *
 * @param[in]  sem  semaphore handle (must not be NULL)
 * @return          CCSEM_SUCCESS
 * @retval CCSEM_ERROR  @p sem is NULL, or the platform call failed
 */
CCSEM_API int ccsem_post(ccsem_t* sem);

/** @} */ /* end of ccsem_blocking */

/* ------------------------------------------------------------------ */
/*  @defgroup ccsem_nonblocking  Non-blocking and timed operations     */
/*  @{                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Try to decrement the semaphore without blocking.
 *
 * If the count is > 0 it is atomically decremented and the call
 * returns CCSEM_SUCCESS.  If the count is 0 the call returns
 * CCSEM_TIMEOUT immediately — the caller is **not** put to sleep.
 *
 * @param[in]  sem  semaphore handle (must not be NULL)
 * @return          CCSEM_SUCCESS if acquired
 * @retval CCSEM_TIMEOUT  count was 0 (would have blocked)
 * @retval CCSEM_ERROR    @p sem is NULL
 *
 * @note Equivalent to `ccsem_timedwait(sem, 0)`.
 * @see ccsem_timedwait()
 */
CCSEM_API int ccsem_trywait(ccsem_t* sem);

/**
 * @brief Wait on the semaphore with a timeout.
 *
 * If the count is > 0 the call returns CCSEM_SUCCESS immediately.
 * If the count is 0 the calling thread blocks for at most
 * @p timeout_ms milliseconds.  If the semaphore is signalled within
 * that window the call returns CCSEM_SUCCESS; otherwise it returns
 * CCSEM_TIMEOUT.
 *
 * @param[in]  sem         semaphore handle (must not be NULL)
 * @param[in]  timeout_ms  maximum wait time in milliseconds;
 *                         0 is equivalent to ccsem_trywait()
 * @return                 CCSEM_SUCCESS if acquired within the deadline
 * @retval CCSEM_TIMEOUT   the deadline expired without a signal
 * @retval CCSEM_ERROR     @p sem is NULL
 *
 * @par Timer precision
 * The library adds ~100 ns of overhead.  Remaining jitter comes from
 * the OS timer subsystem: ~50 µs–10 ms depending on platform and
 * system load.  The deadline is measured against `CLOCK_MONOTONIC`
 * (immune to wall-clock adjustments).  See DESIGN.md §2–3 for details.
 *
 * @note For sub-millisecond deadlines, use a spinning ccsem_trywait()
 *       loop to avoid the kernel timer path entirely.
 * @see ccsem_trywait()
 * @see ccsem_wait()
 */
CCSEM_API int ccsem_timedwait(ccsem_t* sem, unsigned int timeout_ms);

/** @} */ /* end of ccsem_nonblocking */

/* ------------------------------------------------------------------ */
/*  @defgroup ccsem_cleanup  Cleanup                                   */
/*  @{                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Destroy a semaphore and free its resources.
 *
 * @param[in]  sem  semaphore handle to free; NULL is a safe no-op
 *
 * @warning Do NOT call this while any thread is blocked on the
 *          semaphore.  This is a universal constraint across all
 *          semaphore implementations (POSIX `sem_destroy`, Win32
 *          `CloseHandle`, GCD `dispatch_release`).
 */
CCSEM_API void ccsem_destroy(ccsem_t* sem);

/** @} */ /* end of ccsem_cleanup */

#ifdef __cplusplus
}
#endif

#endif /* CCSEM_H */
