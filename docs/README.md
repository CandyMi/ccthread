# ccthread — architecture overview

```mermaid
graph TB
    subgraph Headers["Public Headers"]
        CTH[ccthread.h<br/>Thread API]
        CSH[ccsem.h<br/>Semaphore API]
        CMH[ccmutex.h<br/>Mutex / Spinlock / RWLock API]
    end

    subgraph Sources["Implementations"]
        CTC[ccthread.c<br/>Win32 CreateThread / pthread]
        CSC[ccsem.c<br/>Win32 Semaphore / GCD / pthread+condvar]
        CMC[ccmutex.c<br/>SRWLOCK / CRITICAL_SECTION / pthread_mutex]
        CSP[ccspinlock.c<br/>atomic_flag / __atomic / InterlockedExchange]
        CRW[ccrwlock.c<br/>SRWLOCK+owner / pthread_rwlock]
    end

    CTH --> CTC
    CSH --> CSC
    CMH --> CMC
    CMH --> CSP
    CMH --> CRW

    CTH -.->|#include| CMH
    CSH -.->|standalone| CTH
    CMH -.->|standalone| CTH

    style CTH fill:#4a9,color:#fff
    style CSH fill:#4a9,color:#fff
    style CMH fill:#4a9,color:#fff
```

## Concurrency primitives

| Primitive | Backend (Windows) | Backend (macOS) | Backend (Linux / BSD) | QEMU-tested archs |
|-----------|-------------------|-----------------|----------------------|-------------------|
| **Thread** (`ccthread`) | `CreateThread` | `pthread` | `pthread` | ARM32 / AArch64 / PowerPC64 / MIPS64 / LoongArch64 |
| **Semaphore** (`ccsem`) | `CreateSemaphore` | GCD `dispatch_semaphore` | `pthread_mutex` + `pthread_cond` | ^^ |
| **Mutex** (`ccmutex`) | `SRWLOCK` / `CRITICAL_SECTION` | `pthread_mutex` | `pthread_mutex` | ^^ |
| **Spinlock** (`ccspinlock`) | `InterlockedExchange` | `atomic_flag` / `__atomic` | `atomic_flag` / `__atomic` | ^^ |
| **RWLock** (`ccrwlock`) | `SRWLOCK` + owner tracking | `pthread_rwlock` | `pthread_rwlock` | ^^ |
| **Once** (`ccthread_once`) | atomic state machine | atomic state machine | atomic state machine | ^^ |

## Ownership & lifecycle

```mermaid
stateDiagram-v2
    state "create() → handle" as Create
    state "destroy(handle)" as Destroy
    state "lock / wait / rdlock / wrlock" as Acquire
    state "unlock / post / trylock" as Release

    [*] --> Create: calloc + platform init
    Create --> Acquire: user owns handle
    Acquire --> Release: lock held
    Release --> Acquire: re-acquire
    Release --> Destroy: user done
    Destroy --> [*]: free + platform cleanup
```

## Build matrix

```mermaid
graph LR
    subgraph CMake["CMake Options"]
        O1[BUILD_SHARED_LIBS=ON<br/>Shared .so/.dylib/.dll]
        O2[BUILD_EXAMPLES=ON<br/>Example binaries]
        O3[BUILD_TESTING=ON<br/>CTest registration]
    end

    subgraph Output["Build Output"]
        L1[libccthread.a — static]
        L2[libccthread.so — shared]
        E1[ccthread_basic]
        E2[ccsem_timeout]
        E3[ccmutex_basic]
        E4[ccspinlock_basic]
        E5[ccrwlock_basic]
        E6[ccthread_once]
    end

    CMake --> Output
```

## Cross-platform detection

```mermaid
flowchart TD
    A[Preprocessor check] --> B{"_WIN32 or _WIN64?"}
    B -->|Yes| C[Windows backend]
    B -->|No| D{"__APPLE__?"}
    D -->|Yes| E[macOS backend]
    D -->|No| F{"__linux__ or __FreeBSD__ ..."}
    F -->|Yes| G[POSIX fallback]
    F -->|No| H["#error unsupported"]
```

## Dependency graph

```
ccmutex.h  (standalone — defines ccrecursion_t, ccmutex_state_t)
  │
  ├── ccspinlock.c  ───┬── ccthread.h (for yield/self/equal)
  │                    └── ccmutex.h
  │
  ├── ccrwlock.c    ───┬── ccthread.h
  │                    └── ccmutex.h
  │
  └── ccmutex.c     ───┬── ccthread.h
                       └── ccmutex.h

ccsem.h    (standalone — defines its own CCTHREAD_API + ccmutex_state_t guard)
  │
  └── ccsem.c  ───┬── ccthread.h
                  └── ccsem.h

ccthread.h  (thread API — standalone, no extra includes)
  │
  └── ccthread.c  ───┬── ccatomic.h (once state machine)
                     └── ccthread_once (atomic state machine, no extra deps)
```

## Design decisions

- **Single library, multiple source files.** All primitives compile into one target (`libccthread`). Headers are standalone — each can be `#include`'d without pulling in unrelated APIs.
- **C99 + no GNU extensions.** Compiles with `-std=c99 -pedantic`. The spinlock uses `atomic_flag` when the compiler provides `<stdatomic.h>`; falls back to `__atomic` builtins or platform intrinsics — no C-standard version bump needed.
- **Static + shared.** Both `libccthread.a` and `libccthread.so` / `ccthread.dll` are produced from the same object code. PIC is enabled for the static library so it can be linked into shared libraries.
- **Opaque structs.** All `typedef struct x_impl x_t` are defined in the `.c` files. Public headers only expose forward declarations.
- **Return codes.** `CCMUTEX_SUCCESS` (0), `CCMUTEX_ERROR` (-1), `CCMUTEX_TIMEOUT` (-2 for semaphore). No `errno`.
- **Thread ID.** `ccthread_gettid(NULL)` returns the calling thread's OS TID; `ccthread_gettid(thread)` reads the TID from a thread handle.
