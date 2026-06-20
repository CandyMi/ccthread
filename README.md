# ccthread

[![Linux](https://img.shields.io/github/actions/workflow/status/CandyMi/ccthread/ci.yml?branch=master&job=linux&label=Linux&logo=linux&logoColor=white)](https://github.com/CandyMi/ccthread/actions/workflows/ci.yml)
[![macOS](https://img.shields.io/github/actions/workflow/status/CandyMi/ccthread/ci.yml?branch=master&job=macos&label=macOS&logo=apple&logoColor=white)](https://github.com/CandyMi/ccthread/actions/workflows/ci.yml)
[![Windows](https://img.shields.io/github/actions/workflow/status/CandyMi/ccthread/ci.yml?branch=master&job=windows&label=Windows&logo=windows&logoColor=white)](https://github.com/CandyMi/ccthread/actions/workflows/ci.yml)
[![Cross-build (ARM / PPC / MIPS / LoongArch)](https://img.shields.io/github/actions/workflow/status/CandyMi/ccthread/cross-build.yml?branch=master&label=Cross-build&logo=linux&logoColor=white)](https://github.com/CandyMi/ccthread/actions/workflows/cross-build.yml)

[![Language](https://img.shields.io/badge/language-C%20%2F%20C%2B%2B-555?logo=c&logoColor=white)](.)
[![Standard](https://img.shields.io/badge/standard-C99%20%2F%20C%2B%2B11-004080)](.)
[![Docs](https://img.shields.io/badge/docs-architecture-blue?logo=readthedocs&logoColor=white)](https://github.com/CandyMi/ccthread/blob/master/docs/README.md)

[![Model](https://img.shields.io/badge/model-Thread%20%2F%20Semaphore%20%2F%20Lock-7b2d8e)](.)
[![Use Cases](https://img.shields.io/badge/use%20cases-Embedded%20%2F%20Systems%20%2F%20Games-2a9d46)](.)

Cross-platform C/C++ concurrency primitives — threads, semaphores, mutexes,
spinlocks, and read-write locks — with zero external dependencies beyond the OS.

## At a glance

| Header | Provides | Platform backends |
|--------|----------|-------------------|
| [`ccthread.h`](https://github.com/CandyMi/ccthread/blob/master/ccthread.h) | Thread lifecycle, naming, sleep/yield | Win32 `CreateThread` / POSIX `pthread` |
| [`ccsem.h`](https://github.com/CandyMi/ccthread/blob/master/ccsem.h) | Counting semaphore (blocking / try / timed) | Win32 `CreateSemaphore` / macOS GCD / POSIX `pthread_mutex`+`cond` |
| [`ccmutex.h`](https://github.com/CandyMi/ccthread/blob/master/ccmutex.h) | Mutex, spinlock, read-write lock | `SRWLOCK` / `CRITICAL_SECTION` / `pthread_mutex` / `atomic_flag` / `pthread_rwlock` |
| [`ccatomic.h`](https://github.com/CandyMi/ccthread/blob/master/ccatomic.h) | Atomic load/store/release (header-only) | `__atomic` builtins / MSVC `_InterlockedExchange` / GCC `__sync` |

All headers:
- `extern "C"` — drop into C or C++ projects  
- Heap-allocated opaque handles (`typedef struct x_impl x_t*`)
- Consistent return codes: `CCMUTEX_SUCCESS` (0), `CCMUTEX_ERROR` (-1), `CCMUTEX_TIMEOUT` (-2)

## Platforms

| Feature | Windows (Vista+) | macOS | Linux / BSD | PowerPC / MIPS / LoongArch (QEMU) |
|---------|-----------------|-------|-------------|-----------------------------------|
| Threads | ✅ `CreateThread` | ✅ `pthread` | ✅ `pthread` | ✅ (via QEMU) |
| Semaphore | ✅ `CreateSemaphore` | ✅ GCD | ✅ `pthread_mutex`+`condvar` | ✅ (via QEMU) |
| Mutex (plain) | ✅ `SRWLOCK` | ✅ `pthread_mutex` | ✅ `pthread_mutex` | ✅ (via QEMU) |
| Mutex (recursive) | ✅ `CRITICAL_SECTION` | ✅ `pthread_mutex` recursive | ✅ `pthread_mutex` recursive | ✅ (via QEMU) |
| Spinlock | ✅ `InterlockedExchange` | ✅ `atomic_flag` / `__atomic` | ✅ `atomic_flag` / `__atomic` | ✅ (via QEMU) |
| RWLock | ✅ `SRWLOCK` + owner | ✅ `pthread_rwlock` | ✅ `pthread_rwlock` | ✅ (via QEMU) |

## Build

```sh
cmake -S . -B build
cmake --build build
```

Produces both static and shared libraries:

```
build/
  libccthread.a          # static library (PIC)
  libccthread.so         # shared library (Linux)
  libccthread.dylib      # shared library (macOS)
  ccthread.dll / .lib    # shared library (Windows)
 ```

With examples enabled (`-DBUILD_EXAMPLES=ON`): `ccthread_basic`, `ccthread_detach`, `ccthread_naming`,
`ccsem_producer_consumer`, `ccsem_timeout`, `ccmutex_basic`, `ccspinlock_basic`, `ccrwlock_basic`.

| CMake option | Default | Description |
|--------------|---------|-------------|
| `BUILD_EXAMPLES` | `OFF` | Build example programs |
| `BUILD_TESTING` | `OFF` | Register examples with CTest |

```sh
# With examples enabled:
cmake -B build -DBUILD_EXAMPLES=ON -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build        # run all examples as tests
cmake --install build --prefix /usr/local
```

### Cross-architecture builds

Compiled and tested via QEMU user-mode for ARM32 (HF), AArch64, PowerPC64 (LE),
MIPS64 (BE), and LoongArch64.  See `.github/workflows/cross-build.yml`.

## Quick start

```c
// Threads
#include "ccthread.h"
void* worker(void* arg) { return arg; }
int main(void) {
    ccthread_t* th = ccthread_create(worker, NULL);
    void* ret = NULL;
    ccthread_join(th, &ret);
}

// Semaphore
#include "ccsem.h"
void demo(void) {
    ccsem_t* sem = ccsem_create(0);
    ccsem_post(sem);
    ccsem_wait(sem);
    ccsem_destroy(sem);
}

// Recursive mutex
#include "ccmutex.h"
void demo2(void) {
    ccmutex_t* mtx = ccmutex_create(CCRECURSION_RECURSIVE);
    ccmutex_lock(mtx);
    ccmutex_lock(mtx);   // same thread — safe
    ccmutex_unlock(mtx);
    ccmutex_unlock(mtx);
    ccmutex_destroy(mtx);
}

// Spinlock
#include "ccmutex.h"
void demo3(void) {
    ccspinlock_t* spin = ccspinlock_create(CCRECURSION_PLAIN);
    ccspinlock_lock(spin);
    // critical section (very short)
    ccspinlock_unlock(spin);
    ccspinlock_destroy(spin);
}

// Read-write lock
#include "ccmutex.h"
void demo4(void) {
    ccrwlock_t* rw = ccrwlock_create();
    ccrwlock_rdlock(rw);   // multiple readers allowed
    ccrwlock_unlock(rw);
    ccrwlock_wrlock(rw);   // exclusive
    ccrwlock_unlock(rw);
    ccrwlock_destroy(rw);
}
```

## Examples

| Example | Shows |
|---------|-------|
| [`ccthread_basic.c`](https://github.com/CandyMi/ccthread/blob/master/examples/ccthread_basic.c) | create / join / return values / self / equal |
| [`ccthread_detach.c`](https://github.com/CandyMi/ccthread/blob/master/examples/ccthread_detach.c) | fire-and-forget detached threads |
| [`ccthread_naming.c`](https://github.com/CandyMi/ccthread/blob/master/examples/ccthread_naming.c) | debug thread names |
| [`ccsem_producer_consumer.c`](https://github.com/CandyMi/ccthread/blob/master/examples/ccsem_producer_consumer.c) | bounded-buffer with wait / post |
| [`ccsem_timeout.c`](https://github.com/CandyMi/ccthread/blob/master/examples/ccsem_timeout.c) | trywait polling, timedwait deadline |
| [`ccmutex_basic.c`](https://github.com/CandyMi/ccthread/blob/master/examples/ccmutex_basic.c) | plain vs recursive mode |
| [`ccspinlock_basic.c`](https://github.com/CandyMi/ccthread/blob/master/examples/ccspinlock_basic.c) | shared counter with spinlock |
| [`ccrwlock_basic.c`](https://github.com/CandyMi/ccthread/blob/master/examples/ccrwlock_basic.c) | concurrent readers + exclusive writers |

## Documentation

- **docs/README.md** — architecture overview with mermaid diagrams
- **AGENTS.md** — conventions, recipes, gotchas for contributors

## License

[MIT LICENSE](https://github.com/CandyMi/ccthread/blob/master/LICENSE).
