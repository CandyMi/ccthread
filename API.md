# API Reference

## ccthread — thread library

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

## ccsem — semaphore library

### Semaphore API

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

### Thread safety

All three backends are safe for concurrent `wait` / `trywait` / `timedwait`
/ `post` calls from any number of threads.

| Backend | Mechanism | `wait` / `post` safe |
|---------|-----------|---------------------|
| Windows | Kernel object — `WaitForSingleObject` / `ReleaseSemaphore` serialised by the NT scheduler | ✅ |
| macOS | GCD `dispatch_semaphore` — uses OSAtomic lock-free decrement internally | ✅ |
| Linux / BSD | `pthread_mutex` + `pthread_cond` — all state changes under the mutex; `while` loop guards against spurious wakeups | ✅ |

> ⚠️ `ccsem_destroy` is **not** safe while threads are waiting on the semaphore.
> This is a universal constraint across all semaphore APIs (POSIX `sem_destroy`,
> Win32 `CloseHandle`, GCD `dispatch_release`).
