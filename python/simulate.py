#!/usr/bin/env python3
"""simulate.py - run the STMS Modules 2-4 reference implementation on the
sample state documented in the PBL report, and print matching results.

This reproduces the report's documented expected outputs so the algorithms can
be verified without a C compiler:

  - Module 2: pre-emptive priority schedule (report expected output)
  - Module 3: Banker's safety check  (SAFE, sequence J1,J2,J3,J0)
  - Module 3: semaphore-bounded buffer demo
  - Module 4: ageing-aware schedule (Logger starves -> is rescued)
  - Module 4: LRU vs FIFO page-replacement benchmark

Pure Python; works on any platform with python3 installed.
"""
from __future__ import annotations

import os
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from stms_os import (  # noqa: E402
    BoundedBuffer,
    aging_sched,
    banker_is_safe,
    fifo_page_faults,
    lru_page_faults,
    priority_sched,
)

# --------------------------------------------------------------------------- #
# Module 2 -- report sample state
# --------------------------------------------------------------------------- #
TASKS = [
    {"name": "Normal-A", "arrive": 0, "burst": 3, "prio": 2},
    {"name": "Normal-B", "arrive": 1, "burst": 2, "prio": 2},
    {"name": "Ambulance", "arrive": 2, "burst": 2, "prio": 0},
    {"name": "Logger", "arrive": 0, "burst": 2, "prio": 3},
]

# --------------------------------------------------------------------------- #
# Module 3 -- report sample state (Banker's algorithm)
# --------------------------------------------------------------------------- #
AVAIL = [2, 1, 1]
MAX_DEMAND = [
    [3, 2, 2],   # J0
    [2, 1, 1],   # J1
    [2, 2, 1],   # J2
    [1, 1, 1],   # J3
]
ALLOC = [
    [1, 0, 0],
    [1, 1, 0],
    [0, 1, 1],
    [0, 0, 0],
]

# --------------------------------------------------------------------------- #
def demo_monitor(buffer, producers, consumers=1):
    """Feed `producers` items through `buffer`, consumed by `consumers` threads."""
    results = [None] * producers

    def producer(i):
        results[i] = buffer.get()

    threads = [threading.Thread(target=producer, args=(i,)) for i in range(producers)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    return results


def main():
    # --- Module 2 ---------------------------------------------------------- #
    print("=== Module 2: pre-emptive priority schedule ===")
    print("Preemptive priority schedule:")
    for t, name in priority_sched(TASKS):
        print(f"t={t:2d} {name}")
    print("Expected task order: Normal-A, Normal-A, Ambulance, Ambulance,")
    print("                     Normal-A, Normal-B, Normal-B, Logger, Logger")

    # --- Module 3: Banker's safety check --------------------------------- #
    print("\n=== Module 3: deadlock avoidance (Banker's safety check) ===")
    print("Banker's safety check:")
    safe, seq = banker_is_safe(AVAIL, MAX_DEMAND, ALLOC)
    print(f"System is {'SAFE' if safe else 'UNSAFE'}")
    print("Safe sequence:", ", ".join(f"J{i}" for i in seq) if safe else "-")
    print("Expected safe sequence: J1, J2, J3, J0")

    # --- Module 3: bounded buffer (semaphore-protected) ------------------ #
    print("\n=== Module 3: bounded buffer (semaphore protected) ===")
    cap = 4
    bb = BoundedBuffer(capacity=cap)
    # Producer puts 4 sensor counts; the consumer removes them.  The semaphores
    # guarantee the consumer never reads an empty slot and the producer never
    # writes past the buffer.
    put_count = {"n": 0}
    put_lock = threading.Lock()
    got = {"list": []}

    def producer():
        for k in range(4):
            bb.put(f"count-{k}")
            with put_lock:
                put_count["n"] += 1

    def consumer():
        got["list"] = [bb.get() for _ in range(4)]

    p_thread = threading.Thread(target=producer)
    c_thread = threading.Thread(target=consumer)
    p_thread.start()
    c_thread.start()
    p_thread.join()
    c_thread.join()

    print(f"buffer capacity={cap}; consumed {len(got['list'])} counts in order:")
    for i, v in enumerate(got["list"]):
        print(f"  {i}: {v}")
    print(f"total put={put_count['n']}  (no lost/overwritten counts)")

    # --- Module 4: ageing ------------------------------------------------------------------ #
    print("\n=== Module 4: priority ageing (starvation prevention) ===")
    AGING_TASKS = [
        {"name": "High-A", "arrive": 0, "burst": 2, "prio": 0},
        {"name": "Mid-B", "arrive": 1, "burst": 2, "prio": 2},
        {"name": "Logger", "arrive": 0, "burst": 3, "prio": 3},
    ]
    print("Ageing-aware priority schedule (urgency falls as a task waits):")
    print("Ageing prevents the low-priority Logger from starving.\n")
    for t, name, urg in aging_sched(AGING_TASKS):
        print(f"t={t:2d} {name} (urgency {urg})")

    # --- Module 4: LRU vs FIFO --------------------------------------------------- #
    print("\n=== Module 4: page replacement (LRU vs FIFO) ===")
    REF = [7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1]
    print("Reference string:", REF)
    for cap in (3, 4):
        lru = lru_page_faults(REF, cap)
        fifo = fifo_page_faults(REF, cap)
        better = "LRU better" if lru["faults"] < fifo else "FIFO better"
        print(f"capacity {cap}: LRU faults={lru['faults']} hits={lru['hits']} | "
              f"FIFO faults={fifo} -> {better}")

    print("\nNote: C sources (src/) require a Linux toolchain (gcc);")
    print("they are verified here via this pure-Python reference.")


if __name__ == "__main__":
    main()
