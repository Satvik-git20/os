/*
 * stms_scheduler.c - STMS Module 4: ageing-aware scheduler.
 *
 * Concept applied: Module 2's pre-emptive priority scheduler starves the
 * low-priority Logger task (it only runs after every other task has
 * finished).  Module 4 prevents that starvation with priority ageing: every
 * tick a task spends waiting, its effective priority is boosted (its priority
 * number is reduced), so a low-priority task eventually becomes the most
 * urgent and can no longer be starved.
 *
 * This is the priority-scheduler half extended with ageing -- it mirrors
 * stms_sim.c but prints each task's current (aged) urgency.  In a real OS
 * the same idea is applied to the round-robin / multilevel queues in the
 * lower queues, where long-waiting interactive and background jobs are
 * promoted to keep them responsive.
 *
 * Build:  gcc -Wall -Wextra -O2 -o bin/stms_scheduler src/stms_scheduler.c
 * Run:    ./stms_scheduler
 */
#include <stdio.h>
#include "common.h"

/* priority-points a waiting task gains each tick of waiting */
#define AGE_SHADE 1

/* ageing_sched(): pre-emptive priority schedule with ageing.
 *   t[]  : base tasks (lower prio number = more urgent)
 *   n    : number of tasks
 *
 * Equivalent to stms_sim.c's priority_sched(), except every waiting task's
 * effective urgency is improved by AGE_SHADE per tick, so the low-priority
 * Logger task eventually wins the CPU instead of running only at the end.
 * Each line prints the tick, the task and its current (aged) urgency. */
static void aging_sched(Task t[], int n)
{
    Task cur[n];          /* working copy */
    int eff[n];           /* effective urgency (lower => more urgent) */
    int start[n];         /* tick each task last began running (-1 = none) */
    int running = -1;     /* index of the task executing this tick */
    int time = 0;
    int finished = 0;
    int i, j;

    for (i = 0; i < n; i++) {
        cur[i] = t[i];
        eff[i] = t[i].prio;     /* base urgency */
        start[i] = -1;
    }

    while (finished < n) {
        /* 1) Age every task that has started, is not done, and is not the
         *    task currently executing (running). Aging the running task is
         *    incorrect: a task keeps its urgency while it is on the CPU. */
        for (i = 0; i < n; i++) {
            if (!cur[i].done && start[i] >= 0 && i != running)
                eff[i] -= AGE_SHADE;
        }

        /* 2) Pre-empt: if a more-urgent task has just arrived, yield the
         *    CPU (running is then re-picked in step 3). */
        if (running >= 0 && cur[running].rem > 0 && time > 0) {
            for (i = 0; i < n; i++) {
                if (!cur[i].done && i != running && cur[i].arrive <= time &&
                    eff[i] < eff[running]) {
                    running = -1;
                    break;
                }
            }
        }

        /* 3) Choose the most-urgent arrived task; idle if none is ready. */
        running = -1;
        for (i = 0; i < n; i++) {
            if (!cur[i].done && cur[i].arrive <= time &&
                (running < 0 || eff[i] < eff[running]))
                running = i;
        }

        if (running < 0) {
            time++;
            continue;
        }

        /* 4) Execute one tick. */
        printf("t=%2d %s (urgency %d)\n", time, cur[running].name, eff[running]);

        if (--cur[running].rem == 0) {
            cur[running].done = 1;
            finished++;
        }
        start[running] = time;   /* note: start is the tick it began running */
        time++;
    }
}

int main(void)
{
    Task t[] = {
        {"Normal-A", 0, 3, 2, 3, -1},
        {"Normal-B", 1, 2, 2, 2, -1},
        {"Ambulance", 2, 2, 0, 2, -1},
        {"Logger", 0, 3, 3, 3, -1}
    };

    puts("Ageing-aware priority schedule (urgency falls as a task waits):");
    puts("Ageing prevents the low-priority Logger from starving.");
    aging_sched(t, N_CONTROLLERS);

    return 0;
}
