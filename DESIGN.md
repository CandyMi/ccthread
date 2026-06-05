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

### One-line summary

> mutex+condvar is ~50 ns slower on the uncontended fast path, but any scenario involving thread scheduling drowns that gap in microsecond-scale context-switch noise. It was chosen because **one code path = one correctness story = one place to fix bugs**.

---

