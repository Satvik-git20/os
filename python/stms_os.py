"""Reference implementation of the Smart Traffic Management System (STMS) --
Operating-System modules for the PBL report.

This module mirrors the relevant parts of src/stms_sim.c, src/stms_boundedbuffer.c,
src/stms_scheduler.c and src/stms_lru.c so the algorithms can be *run* and *tested*
without a C compiler.  The same functions live in the C sources; use this module
(plus tests/run_monitor_tests.sh for the bash monitor) as the verified reference.

Data model: every task is a dict with keys
    name, arrive, burst, prio, rem, done
where a lower `prio` number means a higher-urgency task, and `rem` starts equal
to `burst`.  The structures are kept as dicts rather than the C struct so the
Python reference stays dependency-free and readable side by side with the C code.

Modules covered
---------------
* Module 2: priority_sched()                   -- pre-emptive priority scheduling
* Module 3: banker_is_safe()                   -- deadlock avoidance (Banker's)
* Module 3: BoundedBuffer                      -- semaphore producer-consumer
* Module 4: aging_sched()                      -- priority ageing (starvation-free)
* Module 4: lru_page_faults / fifo_page_faults -- page-replacement benchmark
"""

from __future__ import annotations

import threading


# --------------------------------------------------------------------------- #
# Module 2 -- pre-emptive priority scheduling
# --------------------------------------------------------------------------- #
def priority_sched(tasks):
    """Return the (time, task_name) trace of a pre-emptive priority schedule.

    Tasks are dicts with keys ``name, arrive, burst, prio`` (``prio`` lower is
    more urgent).  Runs until every task is done and yields every tick on which
    a task is executed.  Ties on priority are broken by input order (deterministic
    and matches the C implementation in src/stms_sim.c).
    """
    work = []
    for t in tasks:
        item = {
            "name": t["name"],
            "arrive": t["arrive"],
            "burst": t["burst"],
            "prio": t["prio"],
            "rem": t["burst"],
            "done": False,
        }
        work.append(item)

    time = 0
    finished = 0
    n = len(work)

    while finished < n:
        pick = None
        for i, t in enumerate(work):
            if not t["done"] and t["arrive"] <= time:
                if pick is None or t["prio"] < work[pick]["prio"]:
                    pick = i
        if pick is None:
            time += 1
            continue

        yield time, work[pick]["name"]

        work[pick]["rem"] -= 1
        if work[pick]["rem"] == 0:
            work[pick]["done"] = True
            finished += 1
        time += 1


# --------------------------------------------------------------------------- #
# Module 3 -- deadlock avoidance (Banker's algorithm)
# --------------------------------------------------------------------------- #
def banker_is_safe(avail, max_demand, alloc):
    """Return ``(safe, sequence)`` using the Banker's safety check.

    ``avail``    : list of available units per resource type
    ``max_demand``: N x R matrix of maximum demand per controller
    ``alloc``     : N x R matrix of currently allocated units

    Prints each controller that can finish as `` Jx can finish`` (matching the C
    implementation) and returns ``(True, [indices])`` when a safe sequence exists,
    otherwise ``(False, [])``.
    """
    R = len(avail)
    N = len(max_demand)
    work = list(avail)
    finished = [False] * N
    count = 0
    sequence = []

    while count < N:
        found = False
        for i in range(N):
            if finished[i]:
                continue
            need = [max_demand[i][j] - alloc[i][j] for j in range(R)]
            if all(need[j] <= work[j] for j in range(R)):
                for j in range(R):
                    work[j] += alloc[i][j]
                finished[i] = True
                count += 1
                found = True
                sequence.append(i)
                print(f" J{i} can finish")
        if not found:
            return False, []
    return True, sequence


# --------------------------------------------------------------------------- #
# Module 3 -- semaphore-protected bounded buffer (producer-consumer)
# --------------------------------------------------------------------------- #
class BoundedBuffer:
    """A semaphore-guarded circular buffer.  ``put`` is the producer and
    ``get`` is the consumer.  The buffer capacity is fixed at construction."""

    def __init__(self, capacity: int = 8):
        self.capacity = capacity
        self._items = []
        self._empty = threading.Semaphore(capacity)   # free slots
        self._full = threading.Semaphore(0)            # occupied slots
        self._mutex = threading.Semaphore(1)           # buffer lock

    def put(self, item):
        self._empty.acquire()      # wait for a free slot
        self._mutex.acquire()      # lock the buffer
        self._items.append(item)
        self._mutex.release()      # unlock
        self._full.release()       # announce a filled slot

    def get(self):
        self._full.acquire()       # wait for content
        self._mutex.acquire()      # lock the buffer
        item = self._items.pop(0)
        self._mutex.release()      # unlock
        self._empty.release()      # announce a free slot
        return item


# --------------------------------------------------------------------------- #
# Module 4 -- priority ageing (starvation-free scheduling)
# --------------------------------------------------------------------------- #
def aging_sched(tasks, shade=1):
    """Pre-emptive priority schedule where waiting tasks gain ``shade`` priority
    per tick.  Returns a trace of ``(time, task_name, urgency)`` tuples.

    Urgency is the effective priority number *after* ageing for this tick, so a
    low-priority task that waits long enough will see its urgency drop below a
    higher-priority task's and eventually run (no starvation).
    """
    work = []
    for t in tasks:
        item = {
            "name": t["name"],
            "arrive": t["arrive"],
            "burst": t["burst"],
            "prio": t["prio"],
            "rem": t["burst"],
            "done": False,
            "start": -1,            # tick the task last began running
            "urgency": t["prio"],   # effective urgency (lower = more urgent)
        }
        work.append(item)

    time = 0
    finished = 0
    n = len(work)
    running = -1
    trace = []

    while finished < n:
        # 1) Age the waiting tasks (never the one executing this tick).
        for i, t in enumerate(work):
            if not t["done"] and t["rem"] > 0 and t["start"] >= 0 and i != running:
                t["urgency"] = t["prio"] - shade * (time - t["start"])

        # 2) Pre-empt: a more-urgent task that just arrived yields the CPU.
        if running >= 0:
            for i, t in enumerate(work):
                if (not t["done"] and i != running and t["arrive"] <= time
                        and t.get("urgency", t["prio"]) < work[running].get("urgency", work[running]["prio"])):
                    running = -1
                    break

        # 3) Pick the most-urgent arrived task.
        running = -1
        for i, t in enumerate(work):
            if not t["done"] and t["arrive"] <= time:
                eff = t.get("urgency", t["prio"])
                if running < 0 or eff < work[running].get("urgency", work[running]["prio"]):
                    running = i

        if running < 0:
            time += 1
            continue

        t = work[running]
        trace.append((time, t["name"], t.get("urgency", t["prio"])))
        t["rem"] -= 1
        if t["rem"] == 0:
            t["done"] = True
            finished += 1
        t["start"] = time
        time += 1

    return trace


# --------------------------------------------------------------------------- #
# Module 4 -- page-replacement benchmarks (LRU vs FIFO)
# --------------------------------------------------------------------------- #
def lru_page_faults(reference, capacity):
    """Return the number of page faults for `reference` under an LRU cache of
    the given ``capacity``.  Also returns the hit count via the `hits` attribute
    of the list returned (a single-element list for bc-style compatibility)."""
    capacity = max(1, capacity)
    cache = []
    faults = 0
    hits = 0
    for page in reference:
        if page in cache:
            # refresh to MRU
            cache.remove(page)
            cache.insert(0, page)
            hits += 1
        else:
            faults += 1
            if len(cache) == capacity:
                cache.pop()          # drop LRU (last)
            cache.insert(0, page)
    return {"faults": faults, "hits": hits}


def fifo_page_faults(reference, capacity):
    """Return the number of page faults under FIFO with the given capacity."""
    capacity = max(1, capacity)
    cache = []
    faults = 0
    for page in reference:
        if page in cache:
            continue
        faults += 1
        if len(cache) == capacity:
            cache.pop(0)             # drop oldest (FIFO)
        cache.append(page)
    return faults
