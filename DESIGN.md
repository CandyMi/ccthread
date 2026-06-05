# Design notes — ccsem POSIX backend selection & performance

## 1. Why not `sem_t`

### The starting point

In an ideal POSIX world there is one path:

```
all POSIX → sem_t → kernel futex
```

macOS breaks this assumption.

### Decision tree

```
              POSIX semaphore selection
                         │
              ┌──────────┼──────────┐
              ↓          ↓          ↓
           sem_t      sem_t       pthread
          (ideal     +  GCD     mutex+condvar
         but dead)  (hybrid)     (chosen)
```

| Approach | macOS | Linux/BSD | Maintenance cost |
|----------|-------|-----------|-----------------|
| **Pure `sem_t`** | ❌ Dead — `sem_init` deprecated since macOS 10.12; returns `ENOSYS` in some SDKs | ✅ | — |
| **`sem_t` + GCD hybrid** | GCD `dispatch_semaphore` | `sem_t` | 3 code paths; `sem_timedwait` (fixed `CLOCK_REALTIME`) vs `pthread_cond_timedwait` (can use `CLOCK_MONOTONIC`) have different timeout semantics → same class of bugs fixed twice |
| **mutex+condvar** ← current | GCD `dispatch_semaphore` | mutex+condvar | Also 3 paths, but Linux/BSD/all-other-POSIX share a single `#else` branch |

**Conclusion**: since macOS forced a second path (GCD), there is no reason for Linux/BSD to take a third (`sem_t`). Unified mutex+condvar keeps all non-macOS POSIX on one code path.

---

## 2. Performance analysis

### Model

Two critical paths in a semaphore:

- **fast path** — count > 0, `wait` returns immediately
- **slow path** — count == 0, `wait` traps into the kernel; a `post` wakes it

### Fast path comparison (count > 0)

```
   sem_t                     mutex + condvar
   ─────                     ───────────────

wait:                         wait:
   atomic_fetch_sub(count)      pthread_mutex_lock      → atomic CAS
   futex check (noop)           count--
                                pthread_mutex_unlock    → atomic store
                                ─────────────────
                                ~2 extra atomic ops

post:                         post:
   atomic_fetch_add(count)      pthread_mutex_lock      → atomic CAS
   futex check (noop)           count++
                                pthread_cond_signal     → futex(FUTEX_WAKE)
                                                          kernel noop when no waiter
                                pthread_mutex_unlock    → atomic store
                                ─────────────────
                                ~3 extra atomic ops + 1 futex check
```

### Latency quantified

| Operation | sem_t | mutex+condvar | Δ |
|-----------|-------|---------------|----|
| `wait` fast path | ~1 atomic op | ~3 atomic ops | ≈ 20–40 ns |
| `post` fast path | ~1 atomic op | ~4 atomic ops + futex check | ≈ 40–60 ns |

One atomic op ≈ 10–20 ns. Fast-path overhead of mutex+condvar: 20–60 ns.

### Slow path — dwarfed by scheduling noise

```
                        timeline →
    ┌─────────────────────────────────────────────┐
    │                                             │
    │  fast-path gap: ~50ns  ▏                    │
    │                                             │
    │  slow path:                                 │
    │    context switch    ─────────────── 2–20μs │
    │    scheduler latency ─────────────── 5–50μs │
    │                                             │
    │  sem_t vs mutex+condvar on the slow path    │
    │  both hit the same futex; gap ≈ 0%          │
    └─────────────────────────────────────────────┘
```

| Scenario | Bottleneck | sem_t advantage |
|----------|-----------|----------------|
| Uncontended throughput | atomic op count | ~50 ns (visible in micro-benchmarks) |
| Thread blocks then wakes | kernel context switch | none (same `futex` path) |
| Any real workload with I/O or logic | application logic | unmeasurable |

### Why we rejected a Linux-only `sem_t` fast path

```c
#elif defined(__linux__)
    // sem_t (futex, fastest)
#elif defined(__APPLE__)
    // GCD
#else
    // mutex+condvar
```

**Rejected because**: `sem_timedwait` is hardwired to `CLOCK_REALTIME` — an NTP step or leap second breaks the timeout contract. `pthread_cond_timedwait` can use `CLOCK_MONOTONIC`. Splitting Linux and BSD means maintaining timeout-correctness guarantees across two paths. The marginal performance win (~50 ns fast-path) is not worth the dual maintenance burden.

> **Applied fix** (see `ccsem.c`): the POSIX condvar is now initialised with
> `pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)`, and `ccsem_timedwait`
> calls `clock_gettime(CLOCK_MONOTONIC, …)`.  The absolute deadline is computed
> once before the `while` loop; each spurious-wakeup re-entry reuses the same
> monotonic deadline, so elapsed time is never "reset".  Timed waits are now
> immune to wall-clock jumps and spurious-wakeup drift.

### One-line summary

> mutex+condvar is ~50 ns slower on the uncontended fast path, but any scenario involving thread scheduling drowns that gap in microsecond-scale context-switch noise. It was chosen because **one code path = one correctness story = one place to fix bugs**.

---

## 3. Timer precision

### Where the jitter comes from

When `ccsem_timedwait(sem, 5ms)` overruns, the library code is not the cause:

```
actual elapsed = requested timeout
               + pthread_mutex_lock/unlock overhead  (~100 ns, negligible)
               + OS timer granularity                (50 μs – 10 ms, main source)
               + scheduler latency                   (10 – 500 μs, load-dependent)
```

The two dominating factors are **timer slack** (Linux) and **timer coalescing** (macOS) — the kernel deliberately delays timer expiry to batch wake-ups and save power.

### Clock resolution vs. timer precision

| Layer | Linux (typical) | macOS (measured) |
|-------|----------------|------------------|
| `clock_gettime(CLOCK_MONOTONIC)` resolution | 1 ns (vDSO) | 1 μs |
| Timer slack / coalescing | `PR_GET_TIMERSLACK` → default **50 μs**; `prctl(SET_TIMERSLACK, 1)` tightens to ~50 ns | Not queryable; Mach timer coalescing → **1–10 ms** granularity |
| Lib code overhead (`mutex_lock` + `gettime` + `mutex_unlock`) | ~100 ns | ~100 ns |

### Empirical measurement

`ccsem_timedwait(sem, 5ms)` on a locked semaphore, 50 rounds on macOS:

```
target =   5000 μs
min    =   5028 μs  (+28 μs)      ← best case:  one coalescing tick
max    =   6390 μs  (+1390 μs)    ← worst: tick + scheduler delay
avg    =   5785 μs  (+785 μs)     ← typical macOS timer coalescing penalty
```

On Linux the same test would expect ~+50–200 μs deviation — roughly an order of
magnitude tighter — because the default timer slack is 50 μs and the scheduler
tick is typically 250–1000 Hz (1–4 ms).  Linux users who need tighter bounds
can call `prctl(PR_SET_TIMERSLACK, 1)` in their thread before waiting.

### Why this is acceptable

- `ccsem_timedwait` is a **blocking** operation — its caller expects to sleep on the order of milliseconds.  A ~1 ms jitter on a 100 ms timeout is < 1% error, far below application-level timing tolerances.
- For sub-millisecond deadlines, a spinning `ccsem_trywait` loop (busy-wait) avoids the kernel timer path entirely.
- For hard real-time constraints (`PREEMPT_RT`, DPDK-style polling), the whole pthread model is the wrong tool — `ccsem` targets general-purpose systems.

### One-line summary

> The library adds ~100 ns of overhead; the remaining jitter is OS timer granularity (50 μs – 10 ms depending on platform and configuration).  For general-purpose use this is well within tolerance.

---

