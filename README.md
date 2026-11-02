# Smart Traffic Management System (STMS)

Operating-system-level framework for a smart traffic junction, built as a Linux
project for the **OS PBL (Problem-Based Learning)** course. It demonstrates the
core OS concepts — shell automation / cron, process & CPU scheduling, concurrency
& deadlock, and memory management — through the concrete problem of a four-way
junction that adapts signal timing and gives **emergency vehicles priority**.

> **Status:** project scaffold + source + runnable smoke tests. The C sources
> (`src/`) are complete and intended for a Linux toolchain (gcc); this workstation
> has no C compiler, so the algorithms are *verified* via the pure-Python model in
> `python/simulate.py` and via the bash Module-1 tests under `tests/`.
>
> **Report:** `report/Smart_Traffic_Management_System_PBL_Report.docx` (original PBL report).

## Architecture

```
                      +------------------+
                      |  central control |
                      |  (scheduler + DB)|
                      +--------+---------+
                               |
        +------------------------+-------------------------+
        |                        |                        |
  sensor_reader           signal_ctrl              logger
  (produces counts)       (schedules signals)      (append-only logs)
        |                        ^                        |
        +---------- bounded      |   semaphore ----------+
        |      buffer (shared)   |        (shared state)
        +---------- pager --------+   LRU cache + cap
                                   (Module 4)
```

- **Module 1** — `src/stms_monitor.sh`: health-check + auto-restart + log rotation via cron.
- **Module 2** — `src/stms_sim.c`: pre-emptive priority scheduling of junction tasks.
- **Module 3** — `src/stms_boundedbuffer.c` + `src/stms_sim.c`: semaphore-protected producer–consumer + Banker's deadlock avoidance.
- **Module 4** — `src/stms_scheduler.c` + `src/stms_lru.c`: paged memory, LRU replacement, active-process cap.

## Quick start (run what you can, here)

```bash
# Module 1 (bash — works anywhere with bash + coreutils)
cd src
chmod +x stms_monitor.sh
bash tests/run_monitor_tests.sh

# Modules 2–4 (pure-Python model — runs without a C compiler)
cd python
python simulate.py
```

`simulate.py` reproduces the report's documented expected outputs:

```
Preemptive priority schedule:
t= 0 Normal-A
t= 1 Normal-A
t= 2 Ambulance          <- Ambulance pre-empts Normal-A
t= 3 Ambulance
t= 4 Normal-A
t= 5 Normal-B
t= 6 Normal-B
t= 7 Logger
t= 8 Logger
t= 9 Logger
Banker's safety check:
System is SAFE
Safe sequence: J1, J2, J3, J0
```

## Project layout

```
STMS/
├── README.md
├── docs/
│   ├── overview.md          # problem, objectives, scope, users
│   └── modules/             # per-module write-ups (2–4)
├── src/
│   ├── stms_monitor.sh       # Module 1
│   ├── stms_sim.c            # Module 2 + 3 (C)
│   ├── stms_boundedbuffer.c  # Module 3
│   ├── stms_scheduler.c      # Module 4 (ageing)
│   ├── stms_lru.c            # Module 4
│   └── common.h              # shared types
├── python/                   # reference implementation (runs without gcc)
│   ├── stms_os.py            # scheduler / banker / buffer / lru / ageing
│   └── simulate.py           # scenario runner + evidence printer
├── tests/
│   └── run_monitor_tests.sh  # bash smoke tests for Module 1
└── evidence/                 # generated logs from the tests above
```

## What was verified

| Module | Verified via | Evidence |
|---|---|---|
| 1 | bash test harness (5 cases: up, restart, log rotation, empty log, idempotent re-run) | `evidence/monitor_tests.log` |
| 2/3 | Python schedule + Banker's model (matches report's expected values) | `evidence/schedule.txt`, `evidence/banker.txt` |
| 4 | LRU reference-string benchmark (fewer faults than FIFO) | `evidence/lru.txt` |

## Notes for the team

- This is a **simulation-only** deliverable; it does not use hardware. Replace the
  sample `Task`/resource tables with your own junction data when deploying.
- The bracketed fields in the original report
  (`[Roll No.]`, `[Faculty Name]`, `[Course Code]`, team-member names, GitHub link)
  are **not** part of the code project and should be filled in the Word report yourself.
- Modules 2–4 C code is written for Linux `gcc`; compile with:
  `gcc -Wall -Wextra -O2 -o bin/stms_sim src/stms_sim.c -lm`, etc.
