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
- Opaque struct exposed as `typedef struct {…} name_t` (fields are readable, modify through API)
- Consistent error codes: `_SUCCESS` (0), `_TIMEOUT` (-2), `_ERROR` (-1)

## Platforms

| Platform | ccthread backend | ccsem backend |
|----------|-----------------|---------------|
| Windows (Vista+) | Win32 `CreateThread` / `WaitForSingleObject` | Win32 `CreateSemaphore` |
| macOS | POSIX `pthread` | Grand Central Dispatch `dispatch_semaphore` |
| Linux / BSD | POSIX `pthread` | `pthread_mutex` + `pthread_cond` |

---

# ccthread — thread library

## Quick start

```c
#include "ccthread.h"

void* worker(void* arg) {
    int* n = (int*)arg;
    (*n)++;
    return n;
}

int main(void) {
    int val = 0;
    ccthread_t* th = ccthread_create(worker, &val);
    void* ret = NULL;
    ccthread_join(th, &ret);   // blocks, auto-destroys th
    // val == 1, ret == &val
}
```

Compile:

```sh
cc -o demo demo.c ccthread.c -lpthread     # POSIX
cl demo.c ccthread.c                        # MSVC static
cl /D CCTHREAD_BUILD_DLL ccthread.c /LD     # MSVC DLL
```

## API reference

### Thread lifecycle

| Function | Description |
|----------|-------------|
| `ccthread_create(func, arg)` | Create and start a thread. Returns `ccthread_t*` or NULL. |
| `ccthread_join(thread, &result)` | Wait for thread + auto-destroy handle. |
| `ccthread_detach(thread)` | Detach; OS reclaims on exit. Auto-destroys handle. |
| `ccthread_destroy(thread)` | Manual destroy (only for `ccthread_self()` handles). |
| `ccthread_exit(result)` | Exit the calling thread immediately. |
| `ccthread_yield()` | Yield CPU. |
| `ccthread_sleep(ms)` | Suspend calling thread for ≥ `ms` milliseconds. |

### Identification & naming

| Function | Description |
|----------|-------------|
| `ccthread_self()` | Get current-thread handle (must `destroy`). |
| `ccthread_equal(a, b)` | Compare two handles for equality. |
| `ccthread_set_name(thread, name)` | Assign debug name. `thread=NULL` = current thread. |

### Ownership rules

```
ccthread_create()  →  join()   or  detach()    (auto-destroy)
ccthread_self()    →  destroy()                (manual)
```

### Macros

| Macro | Value | Notes |
|-------|-------|-------|
| `CCTHREAD_SUCCESS` | `0` | |
| `CCTHREAD_ERROR` | `-1` | |
| `CCTHREAD_NAME_MAX` | `16` | Max name length incl. NUL |
| `CCTHREAD_API` | platform export | Define `CCTHREAD_BUILD_DLL` or `CCTHREAD_USE_DLL` |

---

# ccsem — semaphore library

## Quick start

```c
#include "ccsem.h"

ccsem_t* sem = ccsem_create(0);   // start locked
ccsem_post(sem);                  // V: count 0 → 1, wakes a waiter
ccsem_wait(sem);                  // P: count 1 → 0, returns immediately
ccsem_destroy(sem);
```

```sh
cc -o demo demo.c ccsem.c -lpthread       # POSIX
cl demo.c ccsem.c                          # MSVC static
```

## API reference

| Function | Description |
|----------|-------------|
| `ccsem_create(initial)` | Create with initial count. NULL on allocation failure. |
| `ccsem_wait(sem)` | P / decrement. Blocks if count == 0. |
| `ccsem_trywait(sem)` | Non-blocking P. Returns `CCSEM_TIMEOUT` if count == 0. |
| `ccsem_timedwait(sem, ms)` | P with timeout in milliseconds. Returns `CCSEM_TIMEOUT` on expiry. `ms=0` = trywait. |
| `ccsem_post(sem)` | V / increment. Wakes exactly one waiter. |
| `ccsem_destroy(sem)` | Free resources. NULL-safe. |

### Macros

| Macro | Value | Notes |
|-------|-------|-------|
| `CCSEM_SUCCESS` | `0` | |
| `CCSEM_TIMEOUT` | `-2` | trywait would block / timedwait expired |
| `CCSEM_ERROR` | `-1` | Invalid argument (e.g. NULL) |
| `CCSEM_API` | platform export | Define `CCSEM_BUILD_DLL` or `CCSEM_USE_DLL` |

---

# Examples

All examples compile with zero warnings under `-Wall -Wextra -pedantic`.
See [`examples/`](examples/) for the full source.

### ccthread

| Example | Demonstrates |
|---------|-------------|
| [`ccthread_basic.c`](examples/ccthread_basic.c) | create / join / return values / self / equal |
| [`ccthread_detach.c`](examples/ccthread_detach.c) | fire-and-forget with detached threads |
| [`ccthread_naming.c`](examples/ccthread_naming.c) | debug names via `set_name` |

### ccsem

| Example | Demonstrates |
|---------|-------------|
| [`ccsem_producer_consumer.c`](examples/ccsem_producer_consumer.c) | bounded-buffer with `wait` / `post` |
| [`ccsem_timeout.c`](examples/ccsem_timeout.c) | `trywait` polling, `timedwait` with deadline, periodic check |

Run an example:

```sh
cc -I. -o example examples/ccthread_basic.c ccthread.c -lpthread
./example
```

---

## Shared library / DLL

Define `_BUILD_DLL` when building, `_USE_DLL` when consuming:

```sh
# POSIX shared library
cc -DCCTHREAD_BUILD_DLL -fvisibility=hidden -shared -fPIC \
   -o libccthread.so ccthread.c -lpthread
cc -DCCTHREAD_USE_DLL -o demo demo.c -L. -lccthread
```

```bat
:: Windows DLL
cl /D CCTHREAD_BUILD_DLL ccthread.c /LD
cl /D CCTHREAD_USE_DLL demo.c ccthread.lib
```

Static linking (no `_DLL` macros) works everywhere with zero flags.

## License

MIT — see [LICENSE](LICENSE).
