# ccthread

[![Linux](https://img.shields.io/github/actions/workflow/status/CandyMi/ccthread/ci.yml?branch=master&job=linux&label=Linux&logo=linux&logoColor=white)](https://github.com/CandyMi/ccthread/actions/workflows/ci.yml)
[![macOS](https://img.shields.io/github/actions/workflow/status/CandyMi/ccthread/ci.yml?branch=master&job=macos&label=macOS&logo=apple&logoColor=white)](https://github.com/CandyMi/ccthread/actions/workflows/ci.yml)
[![Windows](https://img.shields.io/github/actions/workflow/status/CandyMi/ccthread/ci.yml?branch=master&job=windows&label=Windows&logo=windows&logoColor=white)](https://github.com/CandyMi/ccthread/actions/workflows/ci.yml)

[![Language](https://img.shields.io/badge/language-C%20%2F%20C%2B%2B-555?logo=c&logoColor=white)](.)
[![Standard](https://img.shields.io/badge/standard-C99%20%2F%20C%2B%2B11-004080)](.)
[![Docs](https://img.shields.io/badge/docs-architecture-blue?logo=readthedocs&logoColor=white)](docs/README.md)

[![Model](https://img.shields.io/badge/model-Thread%20%2F%20Semaphore%20%2F%20Lock-7b2d8e)](.)
[![Use Cases](https://img.shields.io/badge/use%20cases-Embedded%20%2F%20Systems%20%2F%20Games-2a9d46)](.)

Cross-platform C/C++ concurrency primitives — threads, semaphores, mutexes,
spinlocks, and read-write locks — with zero external dependencies beyond the OS.

```c
#include "ccthread.h"
#include "ccsem.h"
#include "ccmutex.h"
```

## At a glance

| Header | Provides | Platform backends |
|--------|----------|-------------------|
| [`ccthread.h`](ccthread.h) | Thread lifecycle, naming, sleep/yield | Win32 `CreateThread` / POSIX `pthread` |
| [`ccsem.h`](ccsem.h) | Counting semaphore (blocking / try / timed) | Win32 `CreateSemaphore` / macOS GCD / POSIX `pthread_mutex`+`cond` |
| [`ccmutex.h`](ccmutex.h) | Mutex, spinlock, read-write lock | `SRWLOCK` / `CRITICAL_SECTION` / `pthread_mutex` / `atomic_flag` / `pthread_rwlock` |
| [`ccatomic.h`](ccatomic.h) | Atomic load/store/release (header-only) | `__atomic` builtins / MSVC `_InterlockedExchange` / GCC `__sync` |

All headers:
- `extern "C"` — drop into C or C++ projects  
- Heap-allocated opaque handles (`typedef struct x_impl x_t*`)
- Consistent return codes: `CCMUTEX_SUCCESS` (0), `CCMUTEX_ERROR` (-1), `CCMUTEX_TIMEOUT` (-2)

## Platforms

| Feature | Windows (Vista+) | macOS | Linux / BSD |
|---------|-----------------|-------|-------------|
| Threads | ✅ `CreateThread` | ✅ `pthread` | ✅ `pthread` |
| Semaphore | ✅ `CreateSemaphore` | ✅ GCD | ✅ `pthread_mutex`+`condvar` |
| Mutex (plain) | ✅ `SRWLOCK` | ✅ `pthread_mutex` | ✅ `pthread_mutex` |
| Mutex (recursive) | ✅ `CRITICAL_SECTION` | ✅ `pthread_mutex` recursive | ✅ `pthread_mutex` recursive |
| Spinlock | ✅ `InterlockedExchange` | ✅ `atomic_flag` / `__atomic` | ✅ `atomic_flag` / `__atomic` |
| RWLock | ✅ `SRWLOCK` + owner | ✅ `pthread_rwlock` | ✅ `pthread_rwlock` |

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
| [`ccthread_basic.c`](examples/ccthread_basic.c) | create / join / return values / self / equal |
| [`ccthread_detach.c`](examples/ccthread_detach.c) | fire-and-forget detached threads |
| [`ccthread_naming.c`](examples/ccthread_naming.c) | debug thread names |
| [`ccsem_producer_consumer.c`](examples/ccsem_producer_consumer.c) | bounded-buffer with wait / post |
| [`ccsem_timeout.c`](examples/ccsem_timeout.c) | trywait polling, timedwait deadline |
| [`ccmutex_basic.c`](examples/ccmutex_basic.c) | plain vs recursive mode |
| [`ccspinlock_basic.c`](examples/ccspinlock_basic.c) | shared counter with spinlock |
| [`ccrwlock_basic.c`](examples/ccrwlock_basic.c) | concurrent readers + exclusive writers |

## Documentation

- **docs/README.md** — architecture overview with mermaid diagrams
- **AGENTS.md** — conventions, recipes, gotchas for contributors

## License

MIT — see [LICENSE](LICENSE).
