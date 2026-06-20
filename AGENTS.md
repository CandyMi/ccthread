# AGENTS.md

`ccthread` — cross-platform C/C++ concurrency primitives. Single library target, multiple header/source pairs.

## Build commands

```sh
cmake -B build -DBUILD_EXAMPLES=ON -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build               # run all examples as tests
```

Produces `libccthread.a` (static) + `libccthread.dylib`/`.so`/`.dll` (shared).

## Project structure

| Include | Source | Provides |
|---------|--------|----------|
| `ccthread.h` | `ccthread.c` | Thread lifecycle, naming, sleep/yield |
| `ccsem.h` | `ccsem.c` | Counting semaphore (wait/post/trywait/timedwait) |
| `ccmutex.h` | `ccmutex.c` | Recursive/non-recursive mutex |
| `ccmutex.h` | `ccspinlock.c` | Busy-wait spinlock (recursive/non-recursive) |
| `ccmutex.h` | `ccrwlock.c` | Read-write lock (writer-preferring) |
| `ccatomic.h` | _(header-only)_ | Cross-platform atomic macros |
| `ccmutex.h` | `cccondvar.c` | Condition variable (wait/timedwait/signal/broadcast) |

All symbols compile into a single `libccthread`. Each header is standalone — include only what you need.

## Code style

- **C99** (`-std=c99 -pedantic -Wall -Wextra -Werror`). No C11/GNU features in the public API surface; the platform adaptation layer (`ccatomic.h`, TLS selector) uses guard-conditioned `_Thread_local`, `__attribute__`, `__asm__`, `__builtin_*`, and compiler pragmas where the platform requires them.
- **4-space indentation.** No tabs.
- **Open brace on same line** for functions, control flow, structs.
- **Single space after `if`/`for`/`while` before `(`.**
- **`{ … }` scope blocks** inside `#ifdef` branches — even for single statements.
- **`/* */` comments only, no `//`.** Section separators: `/* ==== */`.
- **Explicit `(type*)calloc(...)` casts** — required for C++ embedding.
- **No `typedef` for enums or primitive types** — only structs get a `_t` typedef.

## Naming rules (mandatory)

| Category | Pattern | Examples |
|----------|---------|----------|
| Public function | `prefix_verb_noun` (snake_case) | `ccthread_create`, `ccmutex_lock` |
| Public type | `prefix_t` = `typedef struct prefix_impl` | `ccthread_t`, `ccspinlock_t` |
| Return code | `PREFIX_UPPER_SNAKE` | `CCMUTEX_SUCCESS`, `CCMUTEX_TIMEOUT` |
| Platform macro | `PREFIX_PLATFORM_OS` | `CCTHREAD_PLATFORM_WINDOWS` |
| DLL macro | `PREFIX_BUILD_DLL` / `PREFIX_USE_DLL` | `CCTHREAD_BUILD_DLL` |
| Struct fields | lowercase snake_case | `handle`, `owner`, `recursion` |

Every public symbol MUST start with its prefix. No unprefixed exports.
Return codes: `0` = success, `-1` = error, `-2` = timeout.
Shared enums/typedefs must use `#ifndef X_DEFINED` / `#define X_DEFINED` guards.

## Testing instructions

- Every example in `examples/` is registered as a CTest test.
- All must compile with zero warnings (`-Werror`).
- After changing files, run `cmake --build build && ctest --test-dir build`.
- Fix all test failures before committing.

## Platform abstraction pattern

Every function uses this branching structure:

```c
if (!arg) return PREFIX_ERROR;                  // 1. NULL guard

#ifdef CCTHREAD_PLATFORM_WINDOWS                // 2. Windows
    { … }
#elif defined(__APPLE__)                         // 3. macOS (only if differs)
    { … }
#else                                            // 4. POSIX fallback
    { … }
#endif
```

- `{ }` scope blocks keep platform-local variables contained.
- macOS gets a separate branch only when it needs a different API (e.g. GCD for semaphores). `pthread_mutex` works on macOS — ccmutex/ccrwlock use `#else` directly.

## Critical constraints (gotchas)

- **macOS GCD:** `dispatch_release()` hangs in pure C. `ccsem_destroy` on macOS skips it — the semaphore is reclaimed on process exit. Do NOT add `dispatch_release` back.
- **macOS `sem_init`:** deprecated, returns `ENOSYS` in some SDKs. ccsem uses GCD on macOS, never `sem_t`.
- **Thread naming:** Linux symmetric, macOS calling-thread-only, Windows 10+ symmetric (dynamically loaded). See `ccthread_set_name` for platform split.
- **RWLock Windows:** `SRWLOCK` requires caller to know shared-vs-exclusive. `ccrwlock_unlock` auto-detects via `ccthread_self/equal` — keep this logic.
- **Spinlock atomics:** abstracted in `ccatomic.h`. GCC/Clang use `__atomic`, MSVC uses `_InterlockedExchange`, AIX XL C / Solaris Studio use `__sync`. Never inline raw atomics in other files.
- **C++ embedding:** all `.c` files compile as C++ (`clang++ -std=c++11` verified). Headers have `extern "C"` guards. Keep explicit casts on `calloc`.
- **`ccthread_self()`** returns a heap-allocated handle. It's TLS-cached and auto-reclaimed — no `destroy` needed. Marked `is_self=1` so `join`/`detach` reject it.

## Commit / PR

- Title: `prefix: short description` (e.g. `ccmutex: fix Windows trylock race`).
- Run `cmake --build build && ctest --test-dir build` before committing.
- If adding a new primitive: create `.h` + `.c`, add to `CMakeLists.txt` source list, add example, register in CTest.
