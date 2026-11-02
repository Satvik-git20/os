#ifndef STMS_COMMON_H
#define STMS_COMMON_H

/* Shared types for the Smart Traffic Management System (STMS) modules.
 *
 * This header is included by the Module 2-4 C sources so the whole project
 * shares one definition of Task / resource shapes. Nothing here is a
 * service; it is pure documentation + tool types. */

/* A junction controller / vehicle-detection task. */
typedef struct {
    char name[16];   /* e.g. "Normal-A", "Ambulance", "Logger" */
    int  arrive;     /* arrival time (t) */
    int  burst;      /* total CPU bursts (working set) */
    int  prio;       /* lower value = higher priority (pre-emptive) */
    int  rem;        /* remaining burst to run */
    int  done;       /* finished flag */
} Task;

/* Resource type at a four-way junction (Module 3). */
typedef struct {
    char name[16];
    int  total;   /* number of units of this resource type */
    int  avail;   /* units currently free (updated by the safety check) */
} Resource;

#define N_CONTROLLERS 4   /* vehicle-detection tasks / junction controllers */
#define N_RESOURCES   3   /* signal slot, camera, comm-link */

/* Small fixed sizes used by the bounded-buffer and LRU caches. */
#define BB_SIZE       8   /* bounded buffer slots */
#define LRU_CAPACITY  4   /* page-cache capacity */

#endif /* STMS_COMMON_H */
