/*
 * stms_lru.c - STMS Module 4: LRU (Least Recently Used) page-replacement
 *            cache for junction traffic records.
 *
 * Concept applied: instead of paging in / swapping out the whole journal of
 * traffic records, each junction controller keeps a small, fast LRU cache of
 * the most recently used entries in memory.  Recent traffic data is far more
 * likely to be reused (a long queue of vehicles at one sensor is still
 * relevant a few seconds later), so LRU typically gives fewer page faults
 * than FIFO, which can evict a page that is still being processed (Belady's
 * anomaly).  The production code would map these onto virtual memory /
 * page tables; here we implement the replacement policy as a reference
 * cache so it can be benchmarked on traffic-record reference strings.
 *
 * Build:  gcc -Wall -Wextra -O2 -o bin/stms_lru src/stms_lru.c
 * Run:    ./stms_lru
 */
#include <stdio.h>
#include "common.h"

/* ------------------------------------------------------------------ */
/* LRU reference cache: one fixed-size array holds the resident pages in */
/* use order.  order[0] is the most-recently-used page and order[cap-1]  */
/* is the least-recently-used page.  `size` is the number of valid pages.*/
/* ------------------------------------------------------------------ */
typedef struct {
    int order[LRU_CAPACITY];
    int size;
    int faults;
    int hits;
} LruState;

/* Return 1 if `page` was not in the cache (a fault), 0 if it was a hit.
 * After the call the cache holds `page` as the MRU entry. */
static int lru_refresh(LruState *st, int page)
{
    int i;

    for (i = 0; i < st->size; i++) {
        if (st->order[i] == page) {              /* hit */
            if (i > 0) {                         /* bubble to MRU */
                int tmp = st->order[i];
                for (; i > 0; i--)
                    st->order[i] = st->order[i - 1];
                st->order[0] = tmp;
            }
            st->hits++;
            return 0;
        }
    }

    /* miss: evict the LRU entry (or make room) and insert page at MRU */
    st->faults++;
    if (st->size == LRU_CAPACITY)
        st->order[LRU_CAPACITY - 1] = 0;         /* drop the LRU placeholder */
    else
        st->size++;
    st->order[0] = page;
    return 1;
}

/* Process a reference string and report page faults (and hits) under LRU. */
static void lru_run(const int *ref, int len, int cap)
{
    LruState st;
    int i;

    for (i = 0; i < LRU_CAPACITY; i++)
        st.order[i] = 0;
    st.size = 0;
    st.faults = 0;
    st.hits = 0;

    for (i = 0; i < len; i++)
        lru_refresh(&st, ref[i]);

    printf("LRU cache (capacity %d) on reference string:", cap);
    for (i = 0; i < len; i++)
        printf(" %d", ref[i]);
    printf("\n  faults=%d hits=%d success=%s\n",
           st.faults, st.hits, st.faults == 0 ? "n/a" : "");
}

/* FIFO page-replacement benchmark, for direct comparison. */
static int fifo_faults(const int *ref, int len, int cap)
{
    int order[LRU_CAPACITY];
    int size = 0;
    int faults = 0;
    int i, j;

    for (i = 0; i < cap; i++)
        order[i] = 0;

    for (i = 0; i < len; i++) {
        int present = 0;
        for (j = 0; j < size; j++) {
            if (order[j] == ref[i]) {
                present = 1;
                break;
            }
        }
        if (!present) {
            faults++;
            if (size == cap) {               /* drop the oldest */
                for (j = 0; j < cap - 1; j++)
                    order[j] = order[j + 1];
                size--;
            }
            order[size++] = ref[i];
        }
    }
    return faults;
}

static int lru_faults(const int *ref, int len, int cap)
{
    LruState st;
    int i;

    for (i = 0; i < cap; i++)
        st.order[i] = 0;
    st.size = 0;
    st.faults = 0;
    st.hits = 0;
    for (i = 0; i < len; i++)
        lru_refresh(&st, ref[i]);
    return st.faults;
}

int main(void)
{
    static const int ref[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    int len = sizeof(ref) / sizeof(ref[0]);
    int cap3_lru = 0, cap4_lru = 0;
    int cap3_fifo = 0, cap4_fifo = 0;

    lru_run(ref, len, 3);
    lru_run(ref, len, 4);

    cap3_lru = lru_faults(ref, len, 3);
    cap4_lru = lru_faults(ref, len, 4);
    cap3_fifo = fifo_faults(ref, len, 3);
    cap4_fifo = fifo_faults(ref, len, 4);

    puts("\nBenchmark (capacity 3): LRU vs FIFO");
    printf("  capacity 3: LRU=%d faults, FIFO=%d faults -> %s\n",
           cap3_lru, cap3_fifo, cap3_lru < cap3_fifo ? "LRU better" : "FIFO better");

    puts("Benchmark (capacity 4): LRU vs FIFO");
    printf("  capacity 4: LRU=%d faults, FIFO=%d faults -> %s\n",
           cap4_lru, cap4_fifo, cap4_lru < cap4_fifo ? "LRU better" : "FIFO better");

    return 0;
}
