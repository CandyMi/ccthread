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

# Build

**CMake is the recommended way.**  One configure step builds everything:
libraries, examples, shared or static — all platforms.

```sh
cmake -S . -B build
cmake --build build
```

That produces:

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

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | `OFF` | `ON` → `.so` / `.dylib` / `.dll` |
| `BUILD_EXAMPLES` | `ON` | `OFF` → libraries only |

```sh
# Shared libraries + examples
cmake -S . -B build -D BUILD_SHARED_LIBS=ON
cmake --build build

# Libraries only
cmake -S . -B build -D BUILD_EXAMPLES=OFF
cmake --build build
```

### Install

```sh
cmake --install build --prefix /usr/local
# → /usr/local/lib/libccthread.a  libccsem.a
# → /usr/local/include/ccthread.h  ccsem.h
```

### Manual compilation (no CMake)

If you prefer not to use CMake, just compile the `.c` files directly:

```sh
# POSIX
cc -o myapp myapp.c ccthread.c       -lpthread
cc -o myapp myapp.c ccsem.c ccthread.c -lpthread   # ccsem examples need ccthread too

# MSVC
cl myapp.c ccthread.c
```

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

> Build with `cmake -S . -B build && cmake --build build`.  See [Build](#build) above for details.

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

> Build with `cmake -S . -B build && cmake --build build`.  See [Build](#build) above for details.

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

Build & run with CMake:

```sh
cmake -S . -B build && cmake --build build
./build/ccthread_basic
./build/ccsem_producer_consumer
```

Or manually:

```sh
cc -I. examples/ccthread_basic.c ccthread.c -lpthread -o /tmp/ex && /tmp/ex
```

---

## Shared library / DLL

CMake handles everything automatically — pass `-D BUILD_SHARED_LIBS=ON` and
the `_BUILD_DLL`/`_USE_DLL` defines are set via CMake generator expressions.

Manual invocation (if not using CMake):

```sh
# POSIX
cc -DCCTHREAD_BUILD_DLL -fvisibility=hidden -shared -fPIC \
   -o libccthread.so ccthread.c -lpthread
cc -DCCTHREAD_USE_DLL -o demo demo.c -L. -lccthread
```

```bat
:: Windows
cl /D CCTHREAD_BUILD_DLL ccthread.c /LD
cl /D CCTHREAD_USE_DLL demo.c ccthread.lib
```

Static linking (no macros) works everywhere with zero flags — that's the default.

## License

MIT — see [LICENSE](LICENSE).
