# Module 2 — Process & Thread Management: pre-emptive priority scheduling

## Concept applied

An OS's short-term scheduler must pick which ready task runs next. For STMS the choice is simple to
state but hard to get right: **the Ambulance must be served *immediately*, before any routine task,
and must never be delayed by a longer routine job.** This is exactly the pre-emptive priority case —
the scheduler always runs the highest-priority *ready* task, and a newly-arrived higher-priority task
pre-empts whatever is running.

### The Module-2 policy (and its weakness)

- The **top queue** is reserved for emergency-vehicle tasks, run under **pre-emptive priority
  scheduling** (`lower number = higher urgency`).
- The **lower queue** runs routine detection and logging under **Round Robin**, so no task waits
  *forever*.
- Inside a process, separate threads read sensors, compute timings and write logs (one-to-one model):
  a blocked sensor read does not stall signal changes.

## Implementation (`src/stms_sim.c`, `priority_sched()`)

Each junction controller is a **process with a PCB**: `state`, `priority`, `remain`, `arrive`,
`done`. The scheduler:

1. Scans for the ready task with the **smallest priority number** (`pick` below).
2. If nothing is ready, it idles one tick.
3. Otherwise it executes one tick of the winner; when `rem == 0` the task is marked done.

```c
/* pick the highest-priority ready task */
for (i = 0; i < n; i++)
    if (!t[i].done && t[i].arrive <= time &&
        (pick < 0 || t[i].prio < t[pick].prio))
        pick = i;
```

When the Ambulance arrives (at `t=2`), the loop sees `arrive <= time` and `prio < current`, so the
Ambulance **pre-empts** Normal-A and wins the CPU immediately.

## Sample state and the documented expected output

The PBL report's sample state (4 controllers, priorities 2/2/0/3):

```
Normal-A (arrive 0, burst 3,  prio 2)
Normal-B (arrive 1, burst 2,  prio 2)
Ambulance (arrive 2, burst 2, prio 0)
Logger    (arrive 0, burst 2, prio 3)
```

Running `priority_sched()` on this state yields exactly the report's expected trace:

```
t= 0 Normal-A     t= 1 Normal-A     t= 2 Ambulance
t= 3 Ambulance    t= 4 Normal-A     t= 5 Normal-B
t= 6 Normal-B     t= 7 Logger       t= 8 Logger
```

This matches the report's documented output order:

> `Normal-A, Normal-A, Ambulance, Ambulance, Normal-A, Normal-B, Normal-B, Logger, Logger`

**Key finding:** the Ambulance pre-empts Normal-A at `t=2` and finishes at `t=3` — a bounded,
predictable emergency response. But the low-priority **Logger runs only at the very end**, which is
the starvation problem Module 4's *ageing* solves.

## What was verified

The same algorithm is re-implemented in pure Python (`python/stms_os.py`) and run by
`python/simulate.py`; its trace reproduces the C output above and the report's expected task order
byte-for-byte.
