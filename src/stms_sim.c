/*
 * stms_sim.c - STMS Modules 2 and 3: pre-emptive priority scheduling +
 *             deadlock avoidance (Banker's algorithm).
 *
 * Module 2 - each junction controller is a process with a PCB, and the
 *            short-term scheduler uses a multilevel queue: the top queue
 *            runs emergency-vehicle tasks under pre-emptive priority
 *            scheduling (a lower priority number means a higher urgency),
 *            and the lower queue runs routine detection/logging under
 *            Round Robin. This simulation implements the pre-emptive
 *            priority half of that policy, which is what the Ambulance
 *            task relies on.
 *
 * Module 3 - the Banker's algorithm tests a state of controllers against
 *            shared junction resources (signal slot, camera, comm-link) and
 *            admits a new request only if the resulting state is safe.
 *
 * Sample state (as documented in the PBL report):
 *   controllers : Normal-A, Normal-B, Ambulance, Logger
 *   resources   : signal slot, camera, comm-link
 *
 * Build:  gcc -Wall -Wextra -O2 -o bin/stms_sim src/stms_sim.c -lm
 * Run:    ./stms_sim
 */
#include <stdio.h>
#include "common.h"

/* Module 2: pre-emptive priority schedule (lower prio number = higher
 * priority). The caller owns the Task array; this routine only reads it. */
static void priority_sched(Task t[], int n)
{
    int time = 0;          /* current simulation time */
    int finished = 0;      /* tasks completed */

    while (finished < n) {
        int pick = -1;     /* index of task to run next */
        int i;

        /* Pick the highest-priority ready task. Ties are broken by the
         * lowest index, which keeps the output deterministic. */
        for (i = 0; i < n; i++) {
            if (!t[i].done && t[i].arrive <= time &&
                (pick < 0 || t[i].prio < t[pick].prio)) {
                pick = i;
            }
        }

        if (pick < 0) {    /* nothing ready yet: idle for one tick */
            time++;
            continue;
        }

        printf("t=%2d %s\n", time, t[pick].name);

        if (--t[pick].rem == 0) {   /* task completes */
            t[pick].done = 1;
            finished++;
        }
        time++;
    }
}

/* Module 3: Banker's safety check.
 *   avail[R]   : available units of each resource type
 *   max[N][R]  : maximum demand each controller may ever make
 *   alloc[N][R]: currently allocated units
 *
 * Prints every controller that can finish (" Jx can finish"), then returns
 * 1 (SAFE) if every controller can finish, 0 (UNSAFE) otherwise. */
static int is_safe(int avail[R], int max[N][R], int alloc[N][R])
{
    int work[N_RESOURCES];
    int  fin[N_CONTROLLERS];
    int  i, count = 0;

    for (i = 0; i < N_RESOURCES; i++)
        work[i] = avail[i];

    while (count < N_CONTROLLERS) {
        int found = 0;

        for (i = 0; i < N_CONTROLLERS; i++) {
            if (fin[i])
                continue;

            int ok = 1;
            int j;
            for (j = 0; j < N_RESOURCES; j++) {
                if (max[i][j] - alloc[i][j] > work[j]) {
                    ok = 0;
                    break;
                }
            }

            if (ok) {
                for (j = 0; j < N_RESOURCES; j++)
                    work[j] += alloc[i][j];
                fin[i] = 1;
                count++;
                found = 1;
                printf(" J%d can finish\n", i);
            }
        }

        if (!found)
            return 0;   /* no remaining controller can be satisfied -> unsafe */
    }

    return 1;           /* a safe sequence exists */
}

int main(void)
{
    Task t[] = {
        {"Normal-A", 0, 3, 2, 3, 0},
        {"Normal-B", 1, 2, 2, 2, 0},
        {"Ambulance", 2, 2, 0, 2, 0},
        {"Logger",    0, 2, 3, 2, 0}
    };

    puts("Preemptive priority schedule:");
    priority_sched(t, N_CONTROLLERS);

    /* sample state: 4 controllers, 3 resource types */
    int avail[N_RESOURCES] = {2, 1, 1};
    int max[N_CONTROLLERS][N_RESOURCES] = {
        {3, 2, 2},
        {2, 1, 1},
        {2, 2, 1},
        {1, 1, 1}
    };
    int alloc[N_CONTROLLERS][N_RESOURCES] = {
        {1, 0, 0},
        {1, 1, 0},
        {0, 1, 1},
        {0, 0, 0}
    };

    puts("Banker's safety check:");
    printf("System is %s\n", is_safe(avail, max, alloc) ? "SAFE" : "UNSAFE");

    return 0;
}
