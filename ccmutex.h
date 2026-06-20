/**
 * @file        ccmutex.h
 * @author      candy <https://github.com/CandyMi/ccthread>
 * @brief       Mutex / spinlock / rwlock — part of the ccthread library
 *
 * Standalone header — defines ccrecursion_t, ccmutex_state_t,
 * and all three lock type declarations without including ccthread.h.
 *
 * ## ccmutex — Recursive/non-recursive mutex
 * | Platform   | PLAIN (non-recursive) | RECURSIVE |
 * |------------|----------------------|-----------|
 * | Windows    | `SRWLOCK`            | `CRITICAL_SECTION` |
 * | POSIX      | `pthread_mutex` default | `pthread_mutex` RECURSIVE |
 *
 * ## ccspinlock — Busy-wait spinlock
 * Acquire uses ACQUIRE memory order; release uses RELEASE.
 * Recursive mode tracks owner via ccthread_self/equal.
 *
 * ## ccrwlock — Read-write lock (writer-preferring)
 * | Platform | Implementation |
 * |----------|----------------|
 * | Windows  | `SRWLOCK` + `ccthread_self/equal` for writer tracking |
 * | POSIX    | `pthread_rwlock_t` |
 */

#ifndef CCMUTEX_H
#define CCMUTEX_H

/* ---- standalone export macro (guarded: OK if ccthread.h already defined it) ---- */
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

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/*  Shared types                                                       */
/* ================================================================== */

#ifndef CCRECURSION_T_DEFINED
#define CCRECURSION_T_DEFINED
typedef enum {
    CCRECURSION_PLAIN     = 0,
    CCRECURSION_RECURSIVE = 1
} ccrecursion_t;
#endif

#ifndef CCMUTEX_STATE_T_DEFINED
#define CCMUTEX_STATE_T_DEFINED
typedef enum {
    CCMUTEX_SUCCESS =  0,
    CCMUTEX_ERROR   = -1,
    CCMUTEX_TIMEOUT = -2
} ccmutex_state_t;
#endif

/* ================================================================== */
/*  ccmutex — Recursive/non-recursive mutex                            */
/* ================================================================== */

typedef struct ccmutex_impl ccmutex_t;

/**
 * @brief Create a mutex.
 *
 * @param[in]  type  @c CCRECURSION_PLAIN (fast) or @c CCRECURSION_RECURSIVE
 * @return           new handle on success
 * @retval NULL      allocation or platform init failed
 */
CCTHREAD_API ccmutex_t*        ccmutex_create(ccrecursion_t type);

/**
 * @brief Lock the mutex, blocking until acquired.
 *
 * @param[in]  mtx  mutex handle
 * @return           CCMUTEX_SUCCESS on success
 * @retval CCMUTEX_ERROR  @p mtx is NULL
 *
 * @note Unlike a spinlock, this never busy-waits — the calling thread is
 *       suspended by the OS until the lock becomes available.
 */
CCTHREAD_API ccmutex_state_t   ccmutex_lock(ccmutex_t* mtx);

/**
 * @brief Try to lock without blocking.
 *
 * @param[in]  mtx  mutex handle
 * @return           CCMUTEX_SUCCESS if the lock was acquired
 * @retval CCMUTEX_ERROR  @p mtx is NULL or the lock is already held
 */
CCTHREAD_API ccmutex_state_t   ccmutex_trylock(ccmutex_t* mtx);

/**
 * @brief Unlock the mutex.
 *
 * @param[in]  mtx  mutex handle
 * @return           CCMUTEX_SUCCESS on success
 * @retval CCMUTEX_ERROR  @p mtx is NULL
 *
 * @pre @p mtx must be locked by the calling thread.
 */
CCTHREAD_API ccmutex_state_t   ccmutex_unlock(ccmutex_t* mtx);

/**
 * @brief Destroy a mutex and free its memory.
 *
 * @param[in]  mtx  mutex handle, or NULL (safe no-op)
 *
 * @post The handle is invalid — do not use it after this call.
 */
CCTHREAD_API void              ccmutex_destroy(ccmutex_t* mtx);

/* ================================================================== */
/*  ccspinlock — Busy-wait spinlock (recursive/non-recursive)          */
/* ================================================================== */

typedef struct ccspinlock_impl ccspinlock_t;

/**
 * @brief Create a spinlock.
 *
 * @param[in]  type  @c CCRECURSION_PLAIN or @c CCRECURSION_RECURSIVE
 * @return           new handle on success
 * @retval NULL      allocation failed
 */
CCTHREAD_API ccspinlock_t*     ccspinlock_create(ccrecursion_t type);

/**
 * @brief Acquire the spinlock, busy-waiting until free.
 *
 * Uses @c ccatomic_pause() during the wait loop to reduce CPU contention.
 *
 * @param[in]  spin  spinlock handle
 * @return            CCMUTEX_SUCCESS on success
 * @retval CCMUTEX_ERROR  @p spin is NULL
 */
CCTHREAD_API ccmutex_state_t   ccspinlock_lock(ccspinlock_t* spin);

/**
 * @brief Try to acquire the spinlock without waiting.
 *
 * @param[in]  spin  spinlock handle
 * @return            CCMUTEX_SUCCESS if acquired
 * @retval CCMUTEX_ERROR  @p spin is NULL or lock is already held
 */
CCTHREAD_API ccmutex_state_t   ccspinlock_trylock(ccspinlock_t* spin);

/**
 * @brief Release the spinlock.
 *
 * @param[in]  spin  spinlock handle
 * @return            CCMUTEX_SUCCESS on success
 * @retval CCMUTEX_ERROR  @p spin is NULL
 *
 * @pre @p spin must be held by the calling thread.
 */
CCTHREAD_API ccmutex_state_t   ccspinlock_unlock(ccspinlock_t* spin);

/**
 * @brief Destroy a spinlock and free its memory.
 *
 * @param[in]  spin  spinlock handle, or NULL (safe no-op)
 *
 * @post The handle is invalid — do not use it after this call.
 */
CCTHREAD_API void              ccspinlock_destroy(ccspinlock_t* spin);

/* ================================================================== */
/*  ccrwlock — Read-write lock (writer-preferring)                     */
/* ================================================================== */

typedef struct ccrwlock_impl ccrwlock_t;

/**
 * @brief Create a read-write lock (writer-preferring).
 *
 * @return  new handle on success
 * @retval NULL  allocation or platform init failed
 */
CCTHREAD_API ccrwlock_t*       ccrwlock_create(void);

/**
 * @brief Acquire the lock in shared (read) mode.
 *
 * Multiple readers may hold the lock concurrently.  A writer will
 * be blocked while any reader holds the lock.
 *
 * @param[in]  rw   rwlock handle
 * @return           CCMUTEX_SUCCESS on success
 * @retval CCMUTEX_ERROR  @p rw is NULL
 */
CCTHREAD_API ccmutex_state_t   ccrwlock_rdlock(ccrwlock_t* rw);

/**
 * @brief Try to acquire the read lock without blocking.
 *
 * @param[in]  rw   rwlock handle
 * @return           CCMUTEX_SUCCESS if the read lock was acquired
 * @retval CCMUTEX_ERROR  @p rw is NULL or a writer holds the lock
 */
CCTHREAD_API ccmutex_state_t   ccrwlock_tryrdlock(ccrwlock_t* rw);

/**
 * @brief Acquire the lock in exclusive (write) mode.
 *
 * Blocks until all readers and the current writer (if any) release.
 *
 * @param[in]  rw   rwlock handle
 * @return           CCMUTEX_SUCCESS on success
 * @retval CCMUTEX_ERROR  @p rw is NULL
 */
CCTHREAD_API ccmutex_state_t   ccrwlock_wrlock(ccrwlock_t* rw);

/**
 * @brief Try to acquire the write lock without blocking.
 *
 * @param[in]  rw   rwlock handle
 * @return           CCMUTEX_SUCCESS if the write lock was acquired
 * @retval CCMUTEX_ERROR  @p rw is NULL or the lock is held by another
 */
CCTHREAD_API ccmutex_state_t   ccrwlock_trywrlock(ccrwlock_t* rw);

/**
 * @brief Release a held read or write lock.
 *
 * Windows: auto-detects whether the caller holds the read or write lock
 * via thread identity.  POSIX: delegates to @c pthread_rwlock_unlock.
 *
 * @param[in]  rw   rwlock handle
 * @return           CCMUTEX_SUCCESS on success
 * @retval CCMUTEX_ERROR  @p rw is NULL, or the caller does not hold the lock
 *
 * @pre @p rw must be held by the calling thread in either read or write mode.
 */
CCTHREAD_API ccmutex_state_t   ccrwlock_unlock(ccrwlock_t* rw);

/**
 * @brief Destroy an rwlock and free its memory.
 *
 * @param[in]  rw   rwlock handle, or NULL (safe no-op)
 *
 * @post The handle is invalid — do not use it after this call.
 */
CCTHREAD_API void              ccrwlock_destroy(ccrwlock_t* rw);

/* ================================================================== */
/*  cccondvar — Condition variable (mutex-based)                      */
/* ================================================================== */

/** @brief Condition variable handle.  Heap-allocated by cccondvar_create(). */
typedef struct cccondvar_impl cccondvar_t;

/** @brief Return 1 if the mutex was created with @c CCRECURSION_RECURSIVE. */
CCTHREAD_API int              ccmutex_is_recursive(const ccmutex_t* mtx);

/** @brief Return a pointer to the underlying platform mutex object.
 *
 * Internal use by @c cccondvar.
 *
 * @par Return type by platform
 * | Platform         | Recursive | Type returned           |
 * |------------------|-----------|------------------------|
 * | Windows          | no        | @c PSRWLOCK             |
 * | Windows          | yes       | @c PCRITICAL_SECTION    |
 * | POSIX (all)      | either    | @c pthread_mutex_t*     |
 */
CCTHREAD_API void*            ccmutex_native_handle(ccmutex_t* mtx);

/**
 * @brief Create a condition variable.
 *
 * @return  new handle on success
 * @retval NULL  allocation or platform init failed
 */
CCTHREAD_API cccondvar_t*     cccondvar_create(void);

/**
 * @brief Wait on the condition variable.
 *
 * Atomically releases @p mtx and blocks until another thread calls
 * @c cccondvar_signal() or @c cccondvar_broadcast().  Upon return
 * @p mtx is re-acquired.
 *
 * @param[in]  cv   condition variable handle
 * @param[in]  mtx  locked mutex (must be held by the calling thread)
 * @return           CCMUTEX_SUCCESS on success
 * @retval CCMUTEX_ERROR  @p cv or @p mtx is NULL, or the platform wait failed
 *
 * @pre @p mtx must be locked by the calling thread before calling wait().
 * @post @p mtx is locked by the calling thread when wait() returns.
 */
CCTHREAD_API ccmutex_state_t  cccondvar_wait(cccondvar_t* cv, ccmutex_t* mtx);

/**
 * @brief Wait on the condition variable with a timeout.
 *
 * Same semantics as cccondvar_wait(), but returns @c CCMUTEX_TIMEOUT
 * if the condition is not signalled within @p timeout_ms milliseconds.
 *
 * @param[in]  cv          condition variable handle
 * @param[in]  mtx         locked mutex (must be held by the calling thread)
 * @param[in]  timeout_ms  timeout in milliseconds
 * @return                 CCMUTEX_SUCCESS on signal
 * @retval CCMUTEX_TIMEOUT  no signal within the timeout
 * @retval CCMUTEX_ERROR    @p cv or @p mtx is NULL, or the platform call failed
 *
 * @pre @p mtx must be locked by the calling thread.
 * @post @p mtx is locked by the calling thread when this function returns.
 */
CCTHREAD_API ccmutex_state_t  cccondvar_timedwait(cccondvar_t* cv, ccmutex_t* mtx,
                                                    unsigned int timeout_ms);

/**
 * @brief Wake one thread waiting on the condition variable.
 *
 * Has no effect if no threads are currently waiting.
 *
 * @param[in]  cv   condition variable handle
 * @return           CCMUTEX_SUCCESS on success
 * @retval CCMUTEX_ERROR  @p cv is NULL
 */
CCTHREAD_API ccmutex_state_t  cccondvar_signal(cccondvar_t* cv);

/**
 * @brief Wake all threads waiting on the condition variable.
 *
 * Has no effect if no threads are currently waiting.
 *
 * @param[in]  cv   condition variable handle
 * @return           CCMUTEX_SUCCESS on success
 * @retval CCMUTEX_ERROR  @p cv is NULL
 */
CCTHREAD_API ccmutex_state_t  cccondvar_broadcast(cccondvar_t* cv);

/**
 * @brief Destroy a condition variable and free its memory.
 *
 * @param[in]  cv   handle from cccondvar_create(), or NULL (safe no-op)
 *
 * @post The handle is invalid — do not use it after this call.
 */
CCTHREAD_API void             cccondvar_destroy(cccondvar_t* cv);


#ifdef __cplusplus
}
#endif

#endif /* CCMUTEX_H */
