# ccthread & ccsem

Cross-platform C/C++ concurrency primitives — threads and semaphores — in
two single-file libraries with zero dependencies beyond the OS.

## At a glance

| Library | File | Provides |
|---------|------|----------|
| **ccthread** | `ccthread.h` + `ccthread.c` | Thread lifecycle, naming, sleep/yield |
| **ccsem** | `ccsem.h` + `ccsem.c` | Counting semaphore with blocking / non-blocking / timed wait |

Both libraries share the same design:
- Single-header API, single-source implementation
- `extern "C"` — drop into C or C++ projects
- `CCTHREAD_API` / `CCSEM_API` for MSVC DLL export and GCC visibility
- Heap-allocated `typedef struct {…} name_t` (fields readable, modify via API)
- Consistent error codes: `_SUCCESS` (0), `_TIMEOUT` (-2), `_ERROR` (-1)

## Platforms

| Platform | ccthread backend | ccsem backend |
|----------|-----------------|---------------|
| Windows (Vista+) | Win32 `CreateThread` / `WaitForSingleObject` | Win32 `CreateSemaphore` |
| macOS | POSIX `pthread` | Grand Central Dispatch `dispatch_semaphore` |
| Linux / BSD | POSIX `pthread` | `pthread_mutex` + `pthread_cond` |

### Thread safety

All three backends are safe for concurrent `wait` / `trywait` / `timedwait`
/ `post` calls from any number of threads.

| Backend | ccsem mechanism | `wait` / `post` safe |
|---------|----------------|----------------------|
| Windows | Kernel object — NT scheduler serialises `WaitForSingleObject` / `ReleaseSemaphore` | ✅ |
| macOS | GCD `dispatch_semaphore` — internally uses OSAtomic lock-free decrement | ✅ |
| Linux / BSD | `pthread_mutex` + `pthread_cond` — all state changes under the mutex; `while` loop guards against spurious wakeups | ✅ |

> ⚠️ `destroy` must never be called while threads are waiting on the
> semaphore.  This is a universal constraint across `sem_destroy`,
> `CloseHandle`, and `dispatch_release`.  Similarly, `ccthread_destroy`
> must not be called on a running joinable thread — a joined or detached
> handle is auto-destroyed.

## Build

```sh
cmake -S . -B build
cmake --build build
```

That produces static libraries + all examples:

```
build/
  libccthread.a       (or .lib on Windows)
  libccsem.a
  ccthread_basic      (or .exe)
  ccthread_detach
  ccthread_naming
  ccsem_producer_consumer
  ccsem_timeout
```

| CMake option | Default | Description |
|--------------|---------|-------------|
| `BUILD_SHARED_LIBS` | `OFF` | `ON` → `.so` / `.dylib` / `.dll` |
| `BUILD_EXAMPLES` | `ON` | `OFF` → libraries only |

```sh
cmake --install build --prefix /usr/local
```

## Quick start

```c
// ccthread
#include "ccthread.h"
void* worker(void* arg) { return arg; }
int main(void) {
    ccthread_t* th = ccthread_create(worker, NULL);
    ccthread_join(th, NULL);
}
```

```c
// ccsem
#include "ccsem.h"
int main(void) {
    ccsem_t* sem = ccsem_create(0);
    ccsem_post(sem);
    ccsem_wait(sem);
    ccsem_destroy(sem);
}
```

## Examples

See [`examples/`](examples/) for the full source.

| Example | Demonstrates |
|---------|-------------|
| [`ccthread_basic.c`](examples/ccthread_basic.c) | create / join / return values / self / equal |
| [`ccthread_detach.c`](examples/ccthread_detach.c) | fire-and-forget with detached threads |
| [`ccthread_naming.c`](examples/ccthread_naming.c) | debug names via `set_name` |
| [`ccsem_producer_consumer.c`](examples/ccsem_producer_consumer.c) | bounded-buffer with `wait` / `post` |
| [`ccsem_timeout.c`](examples/ccsem_timeout.c) | `trywait` polling, `timedwait` deadline, periodic check |

```sh
cmake -S . -B build && cmake --build build
./build/ccthread_basic
./build/ccsem_producer_consumer
```

## Further reading

- **[API.md](API.md)** — complete API reference (function tables, macros, ownership rules, thread safety)
- **[DESIGN.md](DESIGN.md)** — why mutex+condvar instead of `sem_t` on Linux, plus performance analysis
## License

MIT — see [LICENSE](LICENSE).
