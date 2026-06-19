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

CCTHREAD_API ccmutex_t*        ccmutex_create(ccrecursion_t type);
CCTHREAD_API ccmutex_state_t   ccmutex_lock(ccmutex_t* mtx);
CCTHREAD_API ccmutex_state_t   ccmutex_trylock(ccmutex_t* mtx);
CCTHREAD_API ccmutex_state_t   ccmutex_unlock(ccmutex_t* mtx);
CCTHREAD_API void              ccmutex_destroy(ccmutex_t* mtx);

/* ================================================================== */
/*  ccspinlock — Busy-wait spinlock (recursive/non-recursive)          */
/* ================================================================== */

typedef struct ccspinlock_impl ccspinlock_t;

CCTHREAD_API ccspinlock_t*     ccspinlock_create(ccrecursion_t type);
CCTHREAD_API ccmutex_state_t   ccspinlock_lock(ccspinlock_t* spin);
CCTHREAD_API ccmutex_state_t   ccspinlock_trylock(ccspinlock_t* spin);
CCTHREAD_API ccmutex_state_t   ccspinlock_unlock(ccspinlock_t* spin);
CCTHREAD_API void              ccspinlock_destroy(ccspinlock_t* spin);

/* ================================================================== */
/*  ccrwlock — Read-write lock (writer-preferring)                     */
/* ================================================================== */

typedef struct ccrwlock_impl ccrwlock_t;

CCTHREAD_API ccrwlock_t*       ccrwlock_create(void);
CCTHREAD_API ccmutex_state_t   ccrwlock_rdlock(ccrwlock_t* rw);
CCTHREAD_API ccmutex_state_t   ccrwlock_tryrdlock(ccrwlock_t* rw);
CCTHREAD_API ccmutex_state_t   ccrwlock_wrlock(ccrwlock_t* rw);
CCTHREAD_API ccmutex_state_t   ccrwlock_trywrlock(ccrwlock_t* rw);
CCTHREAD_API ccmutex_state_t   ccrwlock_unlock(ccrwlock_t* rw);
CCTHREAD_API void              ccrwlock_destroy(ccrwlock_t* rw);

#ifdef __cplusplus
}
#endif

#endif /* CCMUTEX_H */
