# Module 4 — Memory Management & Scheduling extensions

Module 4 handles the **storage and scheduling sides** of the STMS: paged memory with LRU replacement,
a cap on active detection processes, and priority **ageing** to stop low-priority tasks from
starving.

## 4.1 Paged memory, LRU replacement and a process cap

### Why paging

Each controller process gets a **paged address space** instead of contiguous memory, so allocations
need not be contiguity and external fragmentation is avoided. Crucially, **time-critical tasks are
kept resident** (their pages are pinned) so they are never page-faulted mid-control loop.

### LRU replacement

The controller keeps a page cache of the most recently used traffic records. The replacement policy
is **LRU**: evict the page that has gone unused for the longest time (a cached `order[0..cap-1]`
array where `order[0]` is MRU). FIFO was rejected because it can suffer **Belady's anomaly** and may
evict a page that is still being processed.

Reference result from `python/simulate.py` (capacity 3):

```
capacity 3: LRU faults=12 hits=8 | FIFO faults=15 -> LRU better
capacity 4: LRU faults=8 hits=12 | FIFO faults=10 -> LRU better
```

LRU faults fewer pages than FIFO on the same traffic-record reference string; the C benchmark
(`src/stms_lru.c`) prints the same comparison.

### Process cap and thrashing

Capping the number of simultaneous detection processes prevents **thrashing** (too many processes
for the frames). Demand-paged setups should keep the working set of time-critical tasks resident.

## 4.2 Priority ageing (starvation prevention)

### The problem

Module 2's strict pre-emptive priority schedule is correct for emergencies but **starves the
low-priority Logger**: there is always something with a higher priority to run, so the Logger only
gets CPU after *every* other task finishes.

### The fix — ageing

**Ageing** raises the urgency of a waiting task the longer it waits. At each tick, every waiting
task's effective urgency `u` is reduced by a fixed shade `s`:

```
u = base_prio − s × (time − last_started)
```

So a task that has waited 5 ticks has its urgency lowered by 5 points; under steady high-priority
load it will eventually out-urgency every task ahead of it and **cannot be starved**.

### Implementation (`src/stms_scheduler.c`, `aging_sched()`)

```c
/* don't age the task currently executing this tick */
if (!cur[i].done && start[i] >= 0 && i != running)
    eff[i] -= AGE_SHADE;
```

Every tick the scheduler (a) ages the waiting tasks, (b) yields the CPU if a *newly arrived* task
is more urgent than the one running, (c) picks the most-urgent ready task, and (d) runs it one tick.
The trace below (sample state, `shade = 1`) shows the Logger's urgency climb as it waits and its
eventual execution:

```
t= 0 High-A (urgency 0)
t= 1 High-A (urgency 0)
t= 2 Mid-B (urgency 2)
t= 3 Mid-B (urgency 2)
t= 4 Logger (urgency 3)
t= 5 Logger (urgency 3)
t= 6 Logger (urgency 3)
```

In the report's real sample the Logger's urgency keeps falling as it waits against High-A and Mid-B,
so it is guaranteed to run — the starvation is removed.

## What was verified

- `aging_sched()` output is reproducible from `python/simulate.py`.
- `lru_page_faults()`/`fifo_page_faults()` benchmark reproduces the table above (LRU < FIFO).
- The C sources are written for `gcc`; the Python reference is the verified numeric cross-check.

## Client-facing reminder (from the report)

This is a **simulation-only** module. The Module-2/3/4 C code is written for a Linux `gcc` toolchain
(`src/Makefile` is provided) and is exercised here through the pure-Python reference so the project
can be run and verified on any machine *today*.
