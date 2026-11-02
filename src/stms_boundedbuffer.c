/*
 * stms_boundedbuffer.c - STMS Module 3: producer-consumer with a semaphore-
 *            protected bounded buffer.
 *
 * Concept applied: the three STMS user-space services share a junction-state
 * buffer so that sensor_reader (producer) and signal_ctrl (consumer) can never
 * update or read a stale or partially-updated count.  Two counting semaphores
 * (empty / full) guard the slot counts and one binary mutex protects the
 * array itself, eliminating the race condition where two threads would update
 * the same head/tail counter at once.
 *
 *   sensor_reader (producer)  appends vehicle counts
 *   signal_ctrl  (consumer)   removes them for signal computation / logging
 *
 * Build:  gcc -Wall -Wextra -O2 -o bin/stms_boundedbuffer src/stms_boundedbuffer.c -lpthread
 * Run:    ./stms_boundedbuffer
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"

/* Bounded buffer: an array of vehicle counts between the producer and the
 * controller.  The semaphores below guard it and must be initialised by
 * bb_init() before any thread is created. */
typedef struct {
    int buf[BB_SIZE];       /* slots */
    int head;               /* consumer reads here */
    int tail;               /* producer writes here */
    int count;              /* number of valid slots */
    sem_t empty;            /* slots currently free */
    sem_t full;             /* slots currently occupied */
    sem_t mutex;            /* binary lock for the array */
} BoundedBuffer;

static BoundedBuffer g_bb;

/* Initialise the buffer and its three semaphores. */
static void bb_init(void)
{
    int i;
    g_bb.head = g_bb.tail = g_bb.count = 0;
    sem_init(&g_bb.empty, 0, BB_SIZE);   /* all slots free */
    sem_init(&g_bb.full, 0, 0);        /* nothing occupied */
    sem_init(&g_bb.mutex, 0, 1);       /* unlocked */
}

/* Producer: append one vehicle count (staggering the two producers so we can
 * see interleaving, i.e. a live concurrency demonstration). */
static void *producer(void *arg)
{
    int idx = (int)(long)arg;
    int value;
    usleep((useconds_t)(idx * 250000));  /* run producer 0 first */

    value = 10 + (idx * 3);

    sem_wait(&g_bb.empty);    /* wait for a free slot */
    sem_wait(&g_bb.mutex);    /* take the buffer lock */
    g_bb.buf[g_bb.tail] = value;
    g_bb.tail = (g_bb.tail + 1) % BB_SIZE;
    g_bb.count++;
    sem_post(&g_bb.mutex);    /* release the lock */
    sem_post(&g_bb.full);     /* announce a slot is filled */

    printf("[producer %d] appended %d (buffer count now %d)\n",
           idx, value, g_bb.count);
    return NULL;
}

/* Consumer: remove and aggregate counts (simulates the controller's work). */
static void *consumer(void *arg)
{
    (void)arg;
    int total = 0;
    int got = 0;

    while (got < BB_SIZE * 2) {
        sem_wait(&g_bb.full);     /* wait for a filled slot */
        sem_wait(&g_bb.mutex);    /* take the buffer lock */
        int v = g_bb.buf[g_bb.head];
        g_bb.head = (g_bb.head + 1) % BB_SIZE;
        g_bb.count--;
        sem_post(&g_bb.mutex);    /* release the lock */
        sem_post(&g_bb.full);     /* announce a slot is free */

        total += v;
        got++;
        printf("[consumer] removed %d  (running total = %d)\n", v, total);
        usleep(150000);           /* simulate controller processing time */
    }

    printf("[consumer] total aggregated count = %d\n", total);
    return NULL;
}

int main(void)
{
    pthread_t p[2];
    pthread_t c;
    int i;

    bb_init();
    printf("Bounded buffer: %d slots, guarded by semaphores (empty / full / mutex)\n",
           BB_SIZE);

    /* Spawn the producer threads (sensors). */
    for (i = 0; i < 2; i++)
        pthread_create(&p[i], NULL, producer, (void *)(long)i);

    /* Spawn the consumer thread (signal controller). */
    pthread_create(&c, NULL, consumer, NULL);

    for (i = 0; i < 2; i++)
        pthread_join(p[i], NULL);
    pthread_join(c, NULL);

    printf("Buffer state: head=%d tail=%d count=%d\n",
           g_bb.head, g_bb.tail, g_bb.count);
    return 0;
}
