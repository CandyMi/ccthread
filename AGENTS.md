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

## MSVC architecture support

| MSVC 宏 | 架构 | 字长 | 首个 MSVC 版本 | `_MSC_VER` | 最低 Windows | ccthread 状态 |
|---------|------|------|---------------|-----------|-------------|:----------:|
| `_M_IX86` | x86 (IA-32) | 32-bit | MSVC 1.0 (1993) | 800 | **Vista**¹ | ✅ `_mm_pause()` + `_InterlockedExchange` 全支持 |
| `_M_AMD64` | x64 (AMD64) | 64-bit | **MSVC 2005** (VC8) | 1400 | **Vista**¹ | ✅ 同上 |
| `_M_ARM` | ARM32 (AArch32) | 32-bit | **VS 2012–2019** (VC11–VC16) | 1700–1929 | Vista / WinRT | ✅ `__yield()` 已补全², VS 2022+ 已移除 |
| `_M_ARM64` | ARM64 (AArch64) | 64-bit | **VS 2017 15.9** | 1916 | Windows 10 on ARM64 | ✅ `__yield()` |
| `_M_ARM64EC` | ARM64EC³ | 64-bit | **VS 2022 17.2** | 1932 | Windows 11 on ARM64 | ✅ 继承 ARM64 |

| 特性 | x86 | x64 | ARM32 | ARM64 |
|------|:---:|:---:|:----:|:----:|
| `_InterlockedExchange` | ✅ VC8+ | ✅ VC8+ | ✅ VS 2012+ | ✅ VS 2017+ |
| `_InterlockedCompareExchange` | ✅ VC8+ | ✅ VC8+ | ✅ VS 2012+ | ✅ VS 2017+ |
| `_mm_pause()` | ✅ VC8+ (SSE2) | ✅ VC8+ (SSE2) | ❌ N/A | ❌ N/A |
| `__yield()` | ❌ N/A | ❌ N/A | ✅ VS 2012+ | ✅ VS 2017 15.9+ |
| `__declspec(thread)` | ✅ VC6+ | ✅ VC6+ | ✅ VS 2012+ | ✅ VS 2017+ |
| SRWLOCK / CONDITION_VARIABLE | ✅ Vista SDK | ✅ Vista SDK | ✅ WinRT SDK | ✅ Win10 SDK |

> ¹ ccthread 内联定义了 `_WIN32_WINNT=0x0600`（Vista），所有架构的最低 Windows API 版本均为 Vista+。
> ² `ccatomic_pause()` 的 MSVC ARM32 路径于 2025-06 补全，见 `ccatomic.h:150`。VS 2022+ 已移除 ARM32 交叉编译工具链，仅 VS 2012–2019 可用；ARM32 编译验证由 `cross-build.yml`（arm-linux-gnueabihf-gcc + QEMU）覆盖。
> ³ ARM64EC 是 x86 兼容的 ARM64 ABI，允许 ARM64 原生进程中运行 x64 二进制。

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
