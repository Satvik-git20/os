# Project overview

**Smart Traffic Management System (STMS)** — a software design and simulation of a single four-way
junction, built as cooperating user-space services on Linux. The project is a **PBL (Problem-Based
Learning)** deliverable for an Operating Systems course: it shows how the four conceptual pillars of
an operating system map onto a real-world, concurrent, multi-service problem.

> **Problem statement (report, abridged).** Design an operating-system-level framework for a Smart
> Traffic Management System that adapts signal timing to live traffic and gives emergency vehicles
> priority, while remaining reliable and deadlock-free.

## Objectives

1. Host STMS on Linux with automated administration (`stms_monitor.sh`).
2. Schedule detection and signal tasks by **pre-emptive priority** so emergency vehicles are served
   immediately.
3. Protect shared junction data from **races and deadlock** (semaphores + the Banker's algorithm).
4. Manage memory and storage for continuous traffic data (LRU page replacement, paging, ageing).

## Scope

- **Simulation only.** No physical hardware, sensors, cameras or embedded controllers. The sample
  `Task` and resource tables in the code represent one four-way junction and are meant to be swapped
  for real junction data before deployment.
- **Software design and simulation.** This is an OS-concept demonstration: scheduling, synchronisation,
  deadlock handling and memory management are shown through correct, runnable implementations.

## Users

| User | Need | How STMS meets it |
|---|---|---|
| Commuters | Shorter waits, fairer signals | Dynamic signal timing from live counts |
| Emergency services | Bounded, pre-emptive response | Pre-emptive priority on the top queue |
| Traffic police / administrators | Less manual supervision | Auto-restart + log rotation via cron |
| Society / environment | Less idling → less fuel/emissions (SDG 13) | |

## Sustainable Development Goals

**SDG 11** – Sustainable Cities and Communities (smarter, dependable urban traffic).
**SDG 9** – Industry, Innovation and Infrastructure (resilient urban infrastructure).
**SDG 13** – Climate Action (less idling reduces fuel use and emissions).

## Architecture

```
                     +------------------+
                     |  central control |
                     |  (scheduler + DB)|
                     +--------+---------+
                              |
       +---------------------+---------------------+
       |                   |                   |
 sensor_reader      signal_ctrl            logger
 (producer)         (scheduler)            (append-only log)
       |                   ^                   |
       +---------- bounded buffer (semaphore) --+
                  LRU page cache + paging (Module 4)
```

- **Module 1** — `src/stms_monitor.sh`: health-check, auto-restart, log rotation (cron).
- **Module 2** — `src/stms_sim.c`: pre-emptive priority scheduling of junction tasks.
- **Module 3** — `src/stms_sim.c` (Banker's safety check) + `src/stms_boundedbuffer.c` (semaphore
  producer-consumer) + deadlock avoidance.
- **Module 4** — `src/stms_scheduler.c` (ageing) + `src/stms_lru.c` (page replacement) +
  paged virtual memory.

## What is verified

| Module | Verified via | Evidence |
|---|---|---|
| 1 | bash smoke tests (9/9 pass) | `evidence/monitor_tests.log` |
| 2+3+4 | pure-Python reference (`python/stms_os.py`, `simulate.py`) | reports SAFE / LRU-faults-smaller-than-FIFO |
| 2–4 C sources | written for `gcc`; verified numerically against the Python model | `evidence/` + the run above |

## Project layout

```
STMS/
├── README.md
├── docs/
│   ├── overview.md
│   └── modules/          # module2.md, module3.md, module4.md
├── src/
│   ├── common.h
│   ├── stms_monitor.sh       # Module 1
│   ├── stms_sim.c            # Module 2 + 3 (C)
│   ├── stms_boundedbuffer.c  # Module 3
│   ├── stms_scheduler.c      # Module 4
│   ├── stms_lru.c            # Module 4
│   └── Makefile
├── python/
│   ├── stms_os.py            # reference implementation (no C compiler needed)
│   └── simulate.py           # scenario runner (reproduces the report's outputs)
├── tests/
│   └── run_monitor_tests.sh  # Module 1 smoke tests (9/9 passing)
└── evidence/
    └── monitor_tests.log
```
