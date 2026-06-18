# AGENTS.md — ccthread project reference

## Project overview

`ccthread` is a **pair of single-file cross-platform C/C++ concurrency libraries**
with zero external dependencies:

| Library | Files | Purpose | Backends |
|---------|-------|---------|----------|
| **ccthread** | `ccthread.h` + `ccthread.c` | Thread lifecycle + naming | Win32 / pthread |
| **ccsem** | `ccsem.h` + `ccsem.c` | Counting semaphore (blocking / try / timed) | Win32 / macOS GCD / pthread mutex+condvar |

Every API and macro is namespaced with the library prefix (`ccthread_*` / `CCTHREAD_*`, `ccsem_*` / `CCSEM_*`).  Both headers have `extern "C"` guards — drop into C or C++.

### Supporting documentation

| File | Purpose |
|------|--------|
| `README.md` | Entry point — at-a-glance, platforms, build, examples, thread safety |
| `API.md` | Complete API reference (function tables, macros, ownership rules) |
| `DESIGN.md` | POSIX backend selection rationale, fast/slow path performance analysis, timer precision |
| `AGENTS.md` | This file — conventions, recipes, gotchas for contributors |

---

## Design principles

1. **Single .h + single .c per library.**  No internal headers, no subdirectories.  The struct definition lives in the **`.c` file** (opaque to consumers) — platform-specific headers are hidden from API consumers.

2. **Heap-allocated `typedef struct {…} name_t`.**  Handles are explicit pointers: `ccthread_t*`, `ccsem_t*`.  Allocated with `calloc` (zero-init), returned to the caller.  No stack allocation — the struct layout differs per platform.

3. **Platform branching at the outermost level.**  Three layers: (a) platform `#define` at header top, (b) conditional `#include` of OS headers, (c) `#ifdef` / `#elif` / `#else` in every function body.  No `#ifdef` scattered inside logic — each platform gets its own block.

4. **Ownership is explicit.**  `create()` → caller owns the handle.  `join()` / `detach()` auto-destroy.  `self()` → caller must `destroy()`.  `destroy(NULL)` is a safe no-op.

5. **Return codes, not errno.**  Every function returns `_SUCCESS` (0), `_ERROR` (-1), or `_TIMEOUT` (-2, semaphore only).  No global error state.

6. **DLL export is opt-in via macros.**  `CCTHREAD_API` / `CCSEM_API` expand to `__declspec(dllexport/dllimport)` on MSVC or `__attribute__((visibility("default")))` on GCC, controlled by `_BUILD_DLL` / `_USE_DLL` defines.  CMake sets these automatically via generator expressions; manual builds must pass them explicitly.

---

## Naming conventions (mandatory for all additions)

| Category | Pattern | Examples |
|----------|---------|----------|
| Public function | `prefix_verb_noun` (snake_case) | `ccthread_create`, `ccsem_timedwait` |
| Public type | `prefix_t` = `typedef struct prefix_impl` | `ccthread_t`, `ccsem_t` |
| Function-pointer typedef | `prefix_func_t` | `ccthread_func_t` |
| Return code macro | `PREFIX_UPPER_SNAKE` | `CCTHREAD_SUCCESS`, `CCSEM_TIMEOUT` |
| Other macro | `PREFIX_UPPER_SNAKE` | `CCTHREAD_NAME_MAX`, `CCTHREAD_API` |
| Platform-detection macro | `PREFIX_PLATFORM_OS` (value `1`) | `CCTHREAD_PLATFORM_WINDOWS`, `CCSEM_PLATFORM_POSIX` |
| DLL control macro | `PREFIX_BUILD_DLL` / `PREFIX_USE_DLL` | `CCTHREAD_BUILD_DLL`, `CCSEM_USE_DLL` |
| Struct fields | lowercase snake_case | `handle`, `tid`, `is_self`, `detached` |
| Internal static function | `prefix_descriptive_name` | `ccthread_wrapper` |
| Internal local variable | short lowercase | `rc`, `buf`, `hr` |

**Hard rules:**
- Every public symbol MUST start with the library prefix (`ccthread_` or `ccsem_`).  No unprefixed exports.
- Every macro MUST be `UPPER_SNAKE` and start with the library prefix.
- Return codes: `0` = success, negative = failure.  `-2` is reserved for timeout.
- New libraries replicate the prefix convention: `ccmutex_` / `CCMUTEX_`, `ccrwlock_` / `CCRWLOCK_`, etc.

---

## Platform abstraction recipe

### Adding a new platform to an existing function

Follow the existing structure — every function body already has platform blocks:

```c
int ccsem_wait(ccsem_t* sem) {
    if (!sem) return CCSEM_ERROR;              // 1. guard (shared across platforms)

#ifdef CCSEM_PLATFORM_WINDOWS                  // 2. Windows block
    { … }
#elif defined(__APPLE__)                       // 3. macOS block (if differs from POSIX)
    { … }
#else                                          // 4. POSIX fallback (Linux/BSD)
    { … }
#endif
}
```

- Keep each platform block self-contained (no cross-platform `#ifdef` inside one block).
- If macOS needs special treatment, add `#elif defined(__APPLE__)` before `#else`.
- New POSIX variants (FreeBSD, NetBSD) go as additional `#elif` branches or piggyback on `#else` if they match the Linux behavior.

### Adding a completely new library (e.g. ccmutex)

1. Create `ccmutex.h` — copy the structure from `ccsem.h`:
   - Platform detection (`CCMUTEX_PLATFORM_WINDOWS` / `CCMUTEX_PLATFORM_POSIX`)
   - Conditional `#include` block
   - `CCMUTEX_API` export macro (same pattern, different prefix)
   - Return codes: `CCMUTEX_SUCCESS` (0), `CCMUTEX_ERROR` (-1)
   - `typedef struct ccmutex_impl { … } ccmutex_t;`
   - Function declarations with `CCMUTEX_API` and Doxygen comments
   - `extern "C"` guard

2. Create `ccmutex.c` — copy the structure from `ccsem.c`:
   - Re-include platform headers (include guards prevent duplication)
   - Include `<stdlib.h>` for `calloc`/`free`
   - Implement every function with `#ifdef` / `#elif` / `#else` blocks

3. Update `CMakeLists.txt`:
   ```cmake
   add_library(ccmutex ccmutex.c ccmutex.h)
   target_include_directories(ccmutex PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
   target_compile_definitions(ccmutex PRIVATE
       $<$<BOOL:${BUILD_SHARED_LIBS}>:CCMUTEX_BUILD_DLL>)
   target_compile_definitions(ccmutex INTERFACE
       $<$<BOOL:${BUILD_SHARED_LIBS}>:CCMUTEX_USE_DLL>)
   # add example targets as needed
   ```

4. Update `README.md` and `API.md` — follow the existing dual-library structure:
   - Add a `# ccmutex` H1 section
   - Add API reference table
   - Add example to the Examples section

---

## Adding a new function to an existing library

### In the header

```c
/**
 * One-line summary.
 *
 * @param x  description
 * @return   CCTHREAD_SUCCESS, or CCTHREAD_ERROR on …
 */
CCTHREAD_API int ccthread_new_fn(ccthread_t* thread, …);
```

### In the .c file

```c
/* ================================================================== */
/*  ccthread_new_fn                                                     */
/* ================================================================== */

int ccthread_new_fn(ccthread_t* thread, …) {
    if (!thread) return CCTHREAD_ERROR;        // NULL guard first

#ifdef CCTHREAD_PLATFORM_WINDOWS
    { … }
#else
    { … }
#endif
}
```

- Use `{ … }` scope blocks inside each `#ifdef` branch to keep variables local.
- NULL-pointer parameters MUST return the library's `_ERROR` code.
- Document platform-specific behavior differences in the header's Doxygen comment.
- Update `API.md` with the new function in the correct subsection.
- If the addition affects a design decision, update `DESIGN.md` as well.

---

## CMake conventions

- **Generator expressions** for DLL defines, not raw `add_definitions`:
  ```cmake
  target_compile_definitions(ccthread PRIVATE
      $<$<BOOL:${BUILD_SHARED_LIBS}>:CCTHREAD_BUILD_DLL>)
  target_compile_definitions(ccthread INTERFACE
      $<$<BOOL:${BUILD_SHARED_LIBS}>:CCTHREAD_USE_DLL>)
  ```
  This ensures `_BUILD_DLL` is set only when compiling the library itself,
  and `_USE_DLL` is inherited by consumers.

- **pthread linkage rules:**
  - `ccthread` → `if(UNIX)` (all POSIX including macOS)
  - `ccsem` → `if(UNIX AND NOT APPLE)` (macOS uses GCD, no pthread needed)

- **Example targets** depend on the library target (`PRIVATE ccthread`), not raw `.c` files.

- Always add `install()` rules for new library targets.

---

## Gotchas & non-obvious decisions

### Struct is opaque — no platform headers in public headers
The `ccthread_impl` and `ccsem_impl` struct definitions live in their respective `.c`
files, not in the headers.  The headers contain only `typedef struct x_impl x_t;`
forward declarations.

This is intentional: `<windows.h>`, `<pthread.h>`, and `<dispatch/dispatch.h>`
are **internal** to the implementation.  Consumers of the header file never see
platform-specific types or macros, so they don't need any platform SDK installed
beyond what they already have for their own code.

The platform-detection `#define`s (`CCTHREAD_PLATFORM_WINDOWS` / `_POSIX`,
`CCSEM_PLATFORM_WINDOWS` / `_POSIX`) remain in the headers as informational
constants — consumers can use them for conditional compilation of their own code
without pulling in any OS headers.

### macOS `dispatch_release` hangs in pure C
`dispatch_release()` blocks indefinitely when called from pure C on modern macOS.  `ccsem_destroy` on macOS intentionally skips it — the GCD semaphore is reclaimed on process exit.  Do NOT add `dispatch_release` back without verifying against macOS ≥ 12 in a pure-C binary.

### macOS GCD vs POSIX semaphores
`sem_init` / `sem_destroy` are deprecated on macOS and return `ENOSYS` in some SDKs.  This is why `ccsem` uses GCD on macOS and `pthread_mutex`+`pthread_cond` on Linux/BSD — two different implementations for two different POSIX flavors, selected at compile time.

### `ccthread_set_name` is platform-asymmetric
- Linux: can name ANY thread from ANY thread.
- macOS: can only name the CALLING thread (`pthread_setname_np` takes no thread argument).
- Windows 10+: fully symmetric (can name any thread), but the API is dynamically loaded so older Windows silently returns `CCTHREAD_ERROR`.
- The API accepts `thread=NULL` to mean "current thread" for convenience.

### `ccsem` POSIX backend: `CLOCK_MONOTONIC` is mandatory

The condvar MUST be initialised with `pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)` and all timing calls MUST use `clock_gettime(CLOCK_MONOTONIC, …)`.  Using `CLOCK_REALTIME` would make `timedwait` vulnerable to NTP adjustments and manual clock changes (a 12-second backward leap turns a 5-second timeout into 17 seconds).  The monotonic clock also guarantees the absolute deadline stays valid across spurious-wakeup re-entries — no elapsed-time "reset" can occur because the clock never jumps backward.

### Timer precision is OS-limited, not code-limited
The library overhead for `timedwait` is ~100 ns (two mutex ops + one `clock_gettime`).  All remaining jitter comes from the OS: timer slack (~50 µs on Linux, configurable via `prctl(PR_SET_TIMERSLACK, 1)`), timer coalescing (~1–10 ms on macOS, not configurable), and scheduler wake-up latency (~10–500 µs).  For sub-millisecond deadlines use a spinning `trywait` loop.  See `DESIGN.md` §3 for empirical measurements.

### Detached-thread cleanup uses acquire-release atomics
`ccthread_detach` and the thread wrapper both set & check `finished` / `detached`
flags.  The flags are accessed via `CCTHREAD_ATOMIC_STORE` (release) and
`CCTHREAD_ATOMIC_LOAD` (acquire), defined in `ccthread.c` as a compiler-agnostic
wrapper:

| Compiler | Implementation | Platform |
|----------|---------------|----------|
| GCC / Clang | `__atomic_store_n(p, v, __ATOMIC_RELEASE)` / `__atomic_load_n(p, __ATOMIC_ACQUIRE)` | All |
| MSVC | `InterlockedExchange` / `InterlockedCompareExchange` (full barrier — correct, slightly heavier) | Windows |
| Fallback | `volatile` store / load | TinyCC etc. |

The acquire-release pairs form a happens-before chain across the two threads,
closing the theoretical leak window on weakly-ordered CPUs (ARM, PowerPC).
On x86 the atomic macros emit zero extra instructions — x86 loads/stores are
already acquire/release in hardware.  The struct fields are plain `int` (no
`volatile` qualifier) because all access goes through the macros.

### `ccthread_self()` returns an owned handle
Unlike `pthread_self()` which returns a value, `ccthread_self()` heap-allocates a new `ccthread_t` that MUST be `destroy()`'d.  The handle is marked `is_self=1` so `join`/`detach` will reject it.

### Windows thread handles are closed by `detach` / `join`
On Windows, closing the `HANDLE` with `CloseHandle` does NOT terminate the thread — it only releases the creator's reference.  The thread continues running.  This is different from POSIX where `pthread_detach` is an explicit operation.

### Return-code asymmetry
`ccthread` has only `SUCCESS` / `ERROR`.  `ccsem` adds `TIMEOUT` (-2).  New libraries should decide whether they need a timeout code based on whether any API can "time out" or "would block" — if yes, add `_TIMEOUT`; if no, keep it at two codes.

---

## Style rules

- **C99** (`-std=c99 -Wall -Wextra -pedantic`).  No C11 features, no GNU extensions.
- **Tabs for indentation?** No — the project uses 4-space indentation consistently.
- **Open brace on same line** for functions, control flow, and structs.
- **Single space after `if` / `for` / `while` before `(`.**
- **`{ … }` scope blocks** inside `#ifdef` branches — even for single statements — to keep platform-local variables from leaking.
- **Comments**: `/* … */` style only, no `//`.  Section separators use `/* ==== */`.
- **Cast `calloc` return**: `(ccsem_t*)calloc(1, sizeof(ccsem_t))`.  Required for C++.
- **No `typedef` for enums or primitive types.**  Only structs get a `_t` typedef.
- **Headers include only what the struct fields need.**  Extra includes (e.g. `<time.h>` for `struct timespec`) go in the `.c` file.
