/**
 * @file        ccthread.h
 * @brief       Cross-platform C/C++ thread library
 *
 * Single-header API for thread lifecycle management with zero external
 * dependencies.  Supports Windows (Win32) and all POSIX systems (pthread).
 *
 * @see API.md  — complete function reference
 * @see DESIGN.md  — design rationale
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
  #define CCTHREAD_PLATFORM_WINDOWS 1   /**< Windows (MSVC / MinGW) */
#else
  #define CCTHREAD_PLATFORM_POSIX   1   /**< POSIX (Linux / macOS / BSD) */
#endif

/* ---- struct is opaque (definition lives in ccthread.c) ---- */
/* ---- platform headers are internal to the implementation ---- */

/* ------------------------------------------------------------------ */
/*  Symbol export / import (MSVC / GCC visibility)                     */
/* ------------------------------------------------------------------ */

#if defined(_WIN32) || defined(_WIN64)
  #ifdef CCTHREAD_BUILD_DLL
    #define CCTHREAD_API __declspec(dllexport)      /**< Export from DLL */
  #elif defined(CCTHREAD_USE_DLL)
    #define CCTHREAD_API __declspec(dllimport)       /**< Import from DLL */
  #else
    #define CCTHREAD_API                             /**< Static link */
  #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
  #define CCTHREAD_API __attribute__((visibility("default")))   /**< Shared lib export */
#else
  #define CCTHREAD_API
#endif

/* ------------------------------------------------------------------ */
/*  Macros                                                             */
/* ------------------------------------------------------------------ */

/** @brief Operation completed successfully. */
#define CCTHREAD_SUCCESS       0

/** @brief Operation failed (invalid argument, OS error, etc.). */
#define CCTHREAD_ERROR        (-1)

/** @brief Maximum thread name length in bytes, including the null terminator. */
#define CCTHREAD_NAME_MAX      16

/* ------------------------------------------------------------------ */
/*  @defgroup ccthread_types  Types                                    */
/*  @{                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Thread entry-point function signature.
 *
 * @param[in]  arg  user-supplied argument passed to ccthread_create()
 * @return          arbitrary pointer retrieved by ccthread_join()
 *
 * @see ccthread_create()
 * @see ccthread_join()
 */
typedef void* (*ccthread_func_t)(void* arg);

/**
 * @brief Thread handle.
 *
 * Opaque struct — heap-allocated by ccthread_create() or ccthread_self().
 * All access is through the public API functions.
 *
 * @note The full struct definition is in ccthread.c to avoid exposing
 *       platform-specific headers (Win32 / pthread) to the consumer.
 */
typedef struct ccthread_impl ccthread_t;

/** @} */ /* end of ccthread_types */

/* ------------------------------------------------------------------ */
/*  @defgroup ccthread_lifecycle  Thread lifecycle                     */
/*  @{                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Create and start a new thread.
 *
 * The thread begins executing @p func(@p arg) immediately.
 *
 * @param[in]  func  entry-point function (must not be NULL)
 * @param[in]  arg   argument forwarded to @p func
 * @return           new thread handle on success
 * @retval NULL      allocation or platform thread-creation failed
 *
 * @note The returned handle must be consumed by exactly one of
 *       ccthread_join() or ccthread_detach().
 * @note Do NOT call ccthread_destroy() on a handle returned by this function.
 */
CCTHREAD_API ccthread_t* ccthread_create(ccthread_func_t func, void* arg);

/**
 * @brief Wait for a thread to finish and retrieve its return value.
 *
 * Blocks until the thread exits, then auto-destroys the handle.
 *
 * @param[in]  thread  handle from ccthread_create()
 * @param[out] result  receives the return value from the thread function;
 *                     may be NULL if the caller does not need the value
 * @return             CCTHREAD_SUCCESS on success
 * @retval CCTHREAD_ERROR  @p thread is NULL, already detached, already
 *                          joined, or was created by ccthread_self()
 *
 * @pre @p thread must be joinable (not detached, not already joined).
 * @post The handle is destroyed — do not use it after this call.
 * @see ccthread_detach()
 */
CCTHREAD_API int ccthread_join(ccthread_t* thread, void** result);

/**
 * @brief Detach a thread so resources are reclaimed automatically on exit.
 *
 * After detaching you cannot join the thread or retrieve its return value.
 * The handle is auto-destroyed.
 *
 * @param[in]  thread  handle from ccthread_create()
 * @return             CCTHREAD_SUCCESS on success
 * @retval CCTHREAD_ERROR  @p thread is NULL, already detached, already
 *                          joined, or was created by ccthread_self()
 *
 * @post The handle is destroyed — do not use it after this call.
 * @see ccthread_join()
 */
CCTHREAD_API int ccthread_detach(ccthread_t* thread);

/** @} */ /* end of ccthread_lifecycle */

/* ------------------------------------------------------------------ */
/*  @defgroup ccthread_control  Thread control                         */
/*  @{                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Manually destroy a thread handle.
 *
 * @param[in]  thread  handle to free; NULL is a safe no-op
 *
 * @note Use this ONLY for handles returned by ccthread_self().
 *       Handles from ccthread_create() are auto-destroyed by
 *       ccthread_join() or ccthread_detach().
 * @warning Destroying a handle from ccthread_create() on a live
 *          joinable thread leaks the OS thread resource.
 */
CCTHREAD_API void ccthread_destroy(ccthread_t* thread);

/**
 * @brief Exit the calling thread immediately.
 *
 * Equivalent to returning @p result from the thread function, but
 * can be called from any depth in the call stack.
 *
 * @param[in]  result  return value made available to ccthread_join()
 *
 * @note On Windows this calls ExitThread(); on POSIX, pthread_exit().
 */
CCTHREAD_API void ccthread_exit(void* result);

/**
 * @brief Yield the CPU to allow other threads to run.
 *
 * A hint to the scheduler — the calling thread is willing to be
 * de-scheduled in favour of another runnable thread.
 *
 * @note On single-core or heavily loaded systems this is advisory;
 *       there is no guarantee the OS will actually switch contexts.
 */
CCTHREAD_API void ccthread_yield(void);

/**
 * @brief Suspend the calling thread for at least @p ms milliseconds.
 *
 * @param[in]  ms  minimum sleep duration in milliseconds
 *
 * @note The actual sleep may be slightly longer due to OS timer
 *       granularity (typically ≤ 1 tick, ~1–10 ms depending on the platform).
 * @see DESIGN.md §3 — timer precision analysis
 */
CCTHREAD_API void ccthread_sleep(unsigned int ms);

/** @} */ /* end of ccthread_control */

/* ------------------------------------------------------------------ */
/*  @defgroup ccthread_ident  Thread identification                    */
/*  @{                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Get a handle representing the calling thread.
 *
 * The returned handle is a **new allocation** — you own it and must
 * call ccthread_destroy() to free it.
 *
 * @return     new handle referring to the current OS thread
 * @retval NULL memory allocation or platform call failed
 *
 * @note The handle can only be used for identification / comparison
 *       (ccthread_equal()) and naming (ccthread_set_name()).
 *       You cannot join or detach it.
 * @note Unlike pthread_self() which returns a value, this function
 *       heap-allocates — the caller is responsible for cleanup.
 */
CCTHREAD_API ccthread_t* ccthread_self(void);

/**
 * @brief Compare two thread handles for equality.
 *
 * @param[in]  a  first handle (may be NULL)
 * @param[in]  b  second handle (may be NULL)
 * @return        non-zero if both refer to the same OS thread
 * @retval 0      handles differ, or at least one is NULL
 *
 * @note Two NULL handles are considered equal.
 */
CCTHREAD_API int ccthread_equal(ccthread_t* a, ccthread_t* b);

/** @} */ /* end of ccthread_ident */

/* ------------------------------------------------------------------ */
/*  @defgroup ccthread_naming  Thread naming (debug / profiling)       */
/*  @{                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Assign a human-readable name to a thread.
 *
 * Named threads are easier to identify in debuggers (gdb, lldb,
 * WinDbg) and system monitors (top -H, htop, Process Explorer).
 *
 * @param[in]  thread  handle to name, or NULL to name the calling thread
 * @param[in]  name    null-terminated string; silently truncated to
 *                     CCTHREAD_NAME_MAX-1 characters
 * @return             CCTHREAD_SUCCESS on success
 * @retval CCTHREAD_ERROR  @p name is NULL, or the platform does not
 *                          support this operation for the given thread
 *
 * @par Platform support
 * | Platform        | Behaviour |
 * |-----------------|-----------|
 * | Windows 10+     | Any thread can be named (API dynamically loaded) |
 * | Linux           | Any thread can be named |
 * | macOS           | Only the **calling** thread can be named |
 * | Other           | Returns CCTHREAD_ERROR |
 *
 * @note On Windows < 10 the SetThreadDescription API is unavailable;
 *       this function returns CCTHREAD_ERROR silently.
 */
CCTHREAD_API int ccthread_set_name(ccthread_t* thread, const char* name);

/** @} */ /* end of ccthread_naming */

#ifdef __cplusplus
}
#endif

#endif /* CCTHREAD_H */
