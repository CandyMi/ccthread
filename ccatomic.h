/**
 * @file      ccatomic.h
 * @author    candy <https://github.com/CandyMi/ccthread>
 * @brief     Cross-platform atomic operation wrappers
 *
 * Type-generic macros for release-store, acquire-load, acquire-exchange,
 * and CPU pause — backed by compiler builtins or platform intrinsics.
 * C99-compatible, zero project-level header dependencies.
 *
 * @par Compiler support matrix
 * | Compiler     | Store/load/exchange          | Pause                |
 * |--------------|------------------------------|----------------------|
 * | GCC / Clang  | `__atomic_store/load/exchange_n` | `__builtin_ia32_pause` / `yield` / `pause` / `or 1,1,1` |
 * | MSVC         | `_InterlockedExchange` / `_InterlockedCompareExchange` | `_mm_pause` / `__yield` |
 * | AIX XL C     | `__sync_lock_test_and_set` / `__sync_synchronize` | `((void)0)` |
 * | Solaris Studio | same as AIX                | `((void)0)` |
 */

#ifndef CCATOMIC_H
#define CCATOMIC_H

/* ---- MSVC: all intrinsics live in <intrin.h> ---- */
#if defined(_MSC_VER)
  #include <intrin.h>
#endif

/* ================================================================== */
/*  Release store                                                      */
/* ================================================================== */

#if defined(__GNUC__) || defined(__clang__)

  #ifdef __ATOMIC_RELEASE
    #define ccatomic_store_release(p, v) \
        __atomic_store_n((p), (v), __ATOMIC_RELEASE)
  #else
    /* __sync_synchronize is a full barrier; store + barrier = release store */
    #define ccatomic_store_release(p, v) \
        do { *(p) = (v); __sync_synchronize(); } while(0)
  #endif

#elif defined(_MSC_VER)

  /* _InterlockedExchange = full seq_cst barrier (stronger than release, correct) */
  #define ccatomic_store_release(p, v) \
      _InterlockedExchange((volatile long*)(p), (long)(v))

#elif defined(__IBMC__) || defined(__SUNPRO_C)

  /* __sync_synchronize is a full barrier; store + barrier = release store */
  #define ccatomic_store_release(p, v) \
      do { *(p) = (v); __sync_synchronize(); } while(0)

#else

  #define ccatomic_store_release(p, v) \
      (*(p) = (v))

#endif

/* ================================================================== */
/*  Acquire load                                                       */
/* ================================================================== */

#if defined(__GNUC__) || defined(__clang__)

  #ifdef __ATOMIC_RELEASE
    #define ccatomic_load_acquire(p) \
        __atomic_load_n((p), __ATOMIC_ACQUIRE)
  #else
    #define ccatomic_load_acquire(p) \
        (__sync_synchronize(), *(p))
  #endif

#elif defined(_MSC_VER)

  /* _InterlockedCompareExchange(p, 0, 0) = atomic read + full barrier */
  #define ccatomic_load_acquire(p) \
      _InterlockedCompareExchange((volatile long*)(p), 0, 0)

#elif defined(__IBMC__) || defined(__SUNPRO_C)

  #define ccatomic_load_acquire(p) \
      (__sync_synchronize(), *(p))

#else

  #define ccatomic_load_acquire(p) \
      (*(p))

#endif

/* ================================================================== */
/*  Atomic exchange with acquire                                       */
/* ================================================================== */

#if defined(__GNUC__) || defined(__clang__)

  #ifdef __ATOMIC_RELEASE
    #define ccatomic_exchange_acquire(p, v) \
        __atomic_exchange_n((p), (v), __ATOMIC_ACQUIRE)
  #else
    #define ccatomic_exchange_acquire(p, v) \
        __sync_lock_test_and_set((p), (v))
  #endif

#elif defined(_MSC_VER)

  #define ccatomic_exchange_acquire(p, v) \
      _InterlockedExchange((volatile long*)(p), (long)(v))

#elif defined(__IBMC__) || defined(__SUNPRO_C)

  #define ccatomic_exchange_acquire(p, v) \
      __sync_lock_test_and_set((p), (v))

#else

  /* non-atomic fallback — not safe for concurrent use */
  #define ccatomic_exchange_acquire(p, v) \
      (*(p) = (v))

#endif

/* ================================================================== */
/*  CPU pause hint (spin-wait)                                         */
/* ================================================================== */

#if defined(__GNUC__) || defined(__clang__)

  #if defined(__x86_64__) || defined(__i386__)
    #define ccatomic_pause()  __builtin_ia32_pause()
  #elif defined(__aarch64__) || defined(__arm__)
    #define ccatomic_pause()  __asm__ volatile("yield" ::: "memory")
  #elif defined(__powerpc__) || defined(__PPC__)
    /* or 1,1,1 = yield hint (ISA 2.06+, executes as nop on older PPC) */
    #define ccatomic_pause()  __asm__ volatile("or 1,1,1" ::: "memory")
  #elif defined(__loongarch__)
    #define ccatomic_pause()  __asm__ volatile("pause" ::: "memory")
  #elif defined(__mips__) && __mips_isa_rev >= 2
    #define ccatomic_pause()  __asm__ volatile("pause" ::: "memory")
  #else
    #define ccatomic_pause()  ((void)0)
  #endif

#elif defined(_MSC_VER)

  #if defined(_M_AMD64) || defined(_M_IX86)
    #define ccatomic_pause()  _mm_pause()
  #elif defined(_M_ARM64) || defined(_M_ARM)
    #define ccatomic_pause()  __yield()
  #else
    #define ccatomic_pause()  ((void)0)
  #endif

#else

  #define ccatomic_pause()  ((void)0)

#endif

#endif /* CCATOMIC_H */
