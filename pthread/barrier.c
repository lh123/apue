#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>

#define NTHR 8
#define NUMNUM 8000000L
#define TNUM (NUMNUM/NTHR)

long nums[NUMNUM];
long snums[NUMNUM];

pthread_barrier_t b;

static void sink(void *arr,int k, size_t count, size_t ele_size, int (*comp)(const void *a, const void *b));
static void heapsort(void *arr, size_t count, size_t ele_size, int (*comp)(const void *a, const void *b)) {
    for (int i = count / 2; i >= 0; i--) {
        sink(arr, i, count, ele_size, comp);
    }
    void *temp = malloc(ele_size);
    if (temp == NULL) {
        exit(-1);
    }
    for (int i = 0; i < count; i++) {
        memcpy(temp, arr, ele_size);
        memcpy(arr, arr + (count - i - 1) * ele_size, ele_size);
        memcpy(arr + (count - i - 1) * ele_size, temp, ele_size);
        sink(arr, 0, count - i - 1, ele_size, comp);
    }
    free(temp);
}


static void sink(void *arr,int k, size_t count, size_t ele_size, int (*comp)(const void *a, const void *b)) {
    void *temp = malloc(ele_size);
    if (temp == NULL) {
        exit(-1);
    }
    int parent = k;
    int child = 2 * parent + 1;
    while (child < count) {
        if (child + 1 < count && comp(arr + ele_size * child, arr + ele_size * (child + 1)) < 0) {
            child++;
        }
        if (comp(arr + ele_size * parent, arr + ele_size * child) > 0) {
            break;
        }
        memcpy(temp, arr + ele_size * parent, ele_size);
        memcpy(arr + ele_size * parent, arr + ele_size * child, ele_size);
        memcpy(arr + ele_size * child, temp, ele_size);
        parent = child;
        child = 2 * parent + 1;
    }
    free(temp);
}

int complong(const void *arg1, const void *arg2) {
    long l1 = *(long *)arg1;
    long l2 = *(long *)arg2;

    if (l1 == l2) {
        return 0;
    } else if (l1 < l2) {
        return -1;
    } else {
        return 1;
    }
}

void *thr_fn(void *arg) {
    long idx = (long)arg;
    heapsort(&nums[idx], TNUM, sizeof(long), complong);
    pthread_barrier_wait(&b);

    return NULL;
}

void merge(void) {
    long idx[NTHR];
    for (int i = 0; i < NTHR; i++) {
        idx[i] = i * TNUM;
    }
    for (long sidx = 0; sidx < NUMNUM; sidx++) {
        long num = LONG_MAX;
        long minidx;
        for (int i = 0; i < NTHR; i++) {
            if ((idx[i] < (i + 1) * TNUM) && (nums[idx[i]] < num)) {
                num = nums[idx[i]];
                minidx = i;
            }
        }
        snums[sidx] = nums[idx[minidx]];
        idx[minidx]++;
    }
}

int is_sorted(void *arr, size_t count, size_t ele_size, int (*comp)(const void *a, const void *b));

int main(void) {
    srand(time(NULL));
    for (int i = 0; i < NUMNUM; i++) {
        nums[i] = rand();
    }

    struct timespec start;
    clock_gettime(CLOCK_REALTIME, &start);
    pthread_barrier_init(&b, NULL, NTHR + 1);

    for (int i = 0; i < NTHR; i++) {
        pthread_t pid;
        int err = pthread_create(&pid, NULL, thr_fn, (void *)(i * TNUM));
        if (err != 0) {
            printf("create thread error: %s\n", strerror(err));
            exit(-1);
        }
    }
    pthread_barrier_wait(&b);
    merge();
    struct timespec end;
    clock_gettime(CLOCK_REALTIME, &end);

    long long startusec, endusec;
    startusec = start.tv_sec * 1000000 + start.tv_nsec / 1000;
    endusec = end.tv_sec * 1000000 + end.tv_nsec / 1000;
    
    double elapsed = (endusec - startusec) / 1000000.0;
    printf("sort took %.4f seconds\n", elapsed);

    if (!is_sorted(snums, NUMNUM, sizeof(long), complong)) {
        printf("internal error\n");
    }
    return 0;
}

int is_sorted(void *arr, size_t count, size_t ele_size, int (*comp)(const void *a, const void *b)) {
    for (int i = 1; i < count; i++) {
        if (comp(arr + (i - 1) * ele_size, arr + i * ele_size) > 0) {
            return 0;
        }
    }
    return 1;
}
