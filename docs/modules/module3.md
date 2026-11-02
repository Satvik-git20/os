# Module 3 — Concurrency & Deadlock Management

The shared data at a junction is a *count of vehicles waiting on each of four approach segments*.
Three user-space services (the sensor producer, the signal controller, the logger) must share this
state without racing. Module 3 has two parts: **producer–consumer via semaphore-protected bounded
buffer**, and **deadlock avoidance via the Banker's safety check**.

## Part 1 — Producer–consumer (semaphore-protected bounded buffer)

### The race being prevented

Two sensor threads placing counts, and the controller removing them, would corrupt the shared
`head`/`tail`/`count` variables if they touched them without protection — the textbook **race
condition**. A **bounded buffer** is the classic fix: a fixed-size circular array plus three
synchronisation primitives.

### The three semaphores

| Semaphore | Count | Meaning |
|---|---|---|
| `empty` | `capacity` | free slots (producer waits on it) |
| `full`  | `0`         | occupied slots (consumer waits on it) |
| `mutex` | `1`         | guards the array itself |

- **Producer** (`sensor_reader`): `empty.wait()` → `mutex.wait()` → store → `full.signal()`.
- **Consumer** (`signal_ctrl`): `full.wait()` → `mutex.wait()` → read → `full.signal()`.

The `empty`/`full` semaphores keep the producer from overflowing and the consumer from reading
nothing; the `mutex` guarantees the array update is atomic. Future work (Module 4) mentions moving
to real POSIX threads (`pthread`) and mutexes for a live demonstration.

### Implementation (`src/stms_boundedbuffer.c`)

```c
sem_wait(&g_bb.empty);      /* wait for a free slot */
sem_wait(&g_bb.mutex);      /* take the buffer lock */
g_bb.buf[g_bb.tail] = value;
g_bb.tail = (g_bb.tail + 1) % BB_SIZE;
g_bb.count++;
sem_post(&g_bb.mutex);
sem_post(&g_bb.full);
```

A 9-case bash smoke test (`tests/run_monitor_tests.sh` and the Module-1 test suite) confirms both
that all three services report `OK` when running and that a stopped service is reported `DOWN` and
restarted automatically. The dedicated file test for this buffer runs a multi-threaded producer/
consumer and confirms **4 counts are consumed with none lost or overwritten**.

## Part 2 — Deadlock avoidance (Banker's algorithm)

### The traffic form of deadlock

Four controllers contending for shared resources (a signal slot, a camera, a comm link) can form a
**circular wait** — the traffic analogue of gridlock / deadlock. Avoiding it is cheaper than
recovering from it, so STMS uses the **Banker's safety check**: a request is granted **only if**
admitting it leaves the system in a *safe state* (some order exists in which every controller can
finish).

### The safety check (`is_safe()`)

- `avail[R]` — free units per resource type (signal slot, camera, comm link).
- `max[N][R]` — each controller's maximum demand.
- `alloc[N][R]` — currently allocated units.

The algorithm repeatedly finds a controller `i` whose **remaining need**
`max[i] − alloc[i]` fits in `work` (the currently free units); if it does, it is added to the safe
sequence and `alloc[i]` is returned to `work`. If no such controller exists, the state is **UNSAFE**.

```
for each unfinished controller i:
    need[i][j] = max[i][j] − alloc[i][j]
    if need[i] <= work:      # can be satisfied
        work += alloc[i]     # pretend it finished, release its alloc
        add i to the safe sequence
        if none found → UNSAFE
```

The sample state (4 controllers × 3 resources, `avail = {2,1,1}`) checks **SAFE** with the safe
sequence the report documents:

```
 J1 can finish
 J2 can finish
 J3 can finish
 J0 can finish
System is SAFE
Safe sequence: J1, J2, J3, J0
```

### Implementation (`src/stms_sim.c`, `is_safe()`)

```c
int ok = 1;
for (int j = 0; j < R; j++)
    if (max[i][j] - alloc[i][j] > work[j]) { ok = 0; break; }
if (ok) {
    for (int j = 0; j < R; j++) work[j] += alloc[i][j];
    fin[i] = 1;
    ...
}
```

## What was verified

- `priority_sched()` trace (Module 2) reproduces the report's expected task order.
- `banker_is_safe()` prints `J1, J2, J3, J0` and returns SAFE, exactly matching the report.
- The bounded-buffer demo in `python/simulate.py` consumes 4 counts with no loss.

All three are cross-checked by the pure-Python reference (`python/stms_os.py`) and printed by
`python/simulate.py`.
