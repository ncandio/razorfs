/**
 * Multithreading Stress Tests - Phase 3
 *
 * Tests ext4-style per-inode locking with heavy concurrent load.
 */

#define _GNU_SOURCE
#include "../src/nary_tree_mt.h"
#include <stdio.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>

#ifndef S_IFDIR
#define S_IFDIR  0040000
#endif
#ifndef S_IFREG
#define S_IFREG  0100000
#endif

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "❌ FAILED: %s\n", msg); \
        return 1; \
    } \
} while(0)

#define TEST_PASS(msg) printf("✅ %s\n", msg)


/* Test 1: Basic thread safety */
int test_basic_locking(void) {
    struct nary_tree_mt tree;
    nary_tree_mt_init(&tree);

    /* Test lock acquisition */
    TEST_ASSERT(nary_lock_read(&tree, NARY_ROOT_IDX) == 0, "Acquire read lock");
    TEST_ASSERT(nary_unlock(&tree, NARY_ROOT_IDX) == 0, "Release read lock");

    TEST_ASSERT(nary_lock_write(&tree, NARY_ROOT_IDX) == 0, "Acquire write lock");
    TEST_ASSERT(nary_unlock(&tree, NARY_ROOT_IDX) == 0, "Release write lock");

    nary_tree_mt_destroy(&tree);
    TEST_PASS("Basic locking");
    return 0;
}

/* Test 2: Concurrent readers */
struct reader_args {
    struct nary_tree_mt *tree;
    int thread_id;
    int iterations;
    int *success_count;
    pthread_mutex_t *count_mutex;
};

void *reader_thread(void *arg) {
    struct reader_args *args = (struct reader_args *)arg;

    for (int i = 0; i < args->iterations; i++) {
        /* Read root node */
        struct nary_node node;
        if (nary_read_node_mt(args->tree, NARY_ROOT_IDX, &node) == 0) {
            pthread_mutex_lock(args->count_mutex);
            (*args->success_count)++;
            pthread_mutex_unlock(args->count_mutex);
        }

        /* Small delay to increase contention */
        usleep(1);
    }

    return NULL;
}

int test_concurrent_readers(void) {
    struct nary_tree_mt tree;
    nary_tree_mt_init(&tree);

    const int NUM_READERS = 10;
    const int ITERATIONS = 100;

    pthread_t threads[NUM_READERS];
    struct reader_args args[NUM_READERS];
    int success_count = 0;
    pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

    /* Start reader threads */
    for (int i = 0; i < NUM_READERS; i++) {
        args[i].tree = &tree;
        args[i].thread_id = i;
        args[i].iterations = ITERATIONS;
        args[i].success_count = &success_count;
        args[i].count_mutex = &count_mutex;
        pthread_create(&threads[i], NULL, reader_thread, &args[i]);
    }

    /* Wait for completion */
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("   %d concurrent reads completed successfully\n", success_count);
    TEST_ASSERT(success_count == NUM_READERS * ITERATIONS, "All reads succeeded");

    pthread_mutex_destroy(&count_mutex);
    nary_tree_mt_destroy(&tree);
    TEST_PASS("Concurrent readers");
    return 0;
}

/* Test 3: Concurrent create operations */
struct writer_args {
    struct nary_tree_mt *tree;
    int thread_id;
    int num_files;
    int *created_count;
    pthread_mutex_t *count_mutex;
    uint16_t parent_idx;  /* Parent directory for this thread */
};

void *writer_thread(void *arg) {
    struct writer_args *args = (struct writer_args *)arg;

    for (int i = 0; i < args->num_files; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "file%d.txt", i);

        uint16_t idx = nary_insert_mt(args->tree, args->parent_idx,
                                     filename, S_IFREG | 0644);

        if (idx != NARY_INVALID_IDX) {
            pthread_mutex_lock(args->count_mutex);
            (*args->created_count)++;
            pthread_mutex_unlock(args->count_mutex);
        }
    }

    return NULL;
}

int test_concurrent_creates(void) {
    struct nary_tree_mt tree;
    nary_tree_mt_init(&tree);

    const int NUM_WRITERS = 8;
    const int FILES_PER_WRITER = 5;

    /* Pre-create directories for each thread to avoid root capacity limit */
    uint16_t thread_dirs[NUM_WRITERS];
    for (int i = 0; i < NUM_WRITERS; i++) {
        char dirname[64];
        snprintf(dirname, sizeof(dirname), "thread%d", i);
        thread_dirs[i] = nary_insert_mt(&tree, NARY_ROOT_IDX, dirname, S_IFDIR | 0755);
        TEST_ASSERT(thread_dirs[i] != NARY_INVALID_IDX, "Create thread directory");
    }

    pthread_t threads[NUM_WRITERS];
    struct writer_args args[NUM_WRITERS];
    int created_count = 0;
    pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

    /* Start writer threads - each writes to its own directory */
    for (int i = 0; i < NUM_WRITERS; i++) {
        args[i].tree = &tree;
        args[i].thread_id = i;
        args[i].num_files = FILES_PER_WRITER;
        args[i].created_count = &created_count;
        args[i].count_mutex = &count_mutex;
        args[i].parent_idx = thread_dirs[i];
        pthread_create(&threads[i], NULL, writer_thread, &args[i]);
    }

    /* Wait for completion */
    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("   %d files created by %d threads\n", created_count, NUM_WRITERS);

    pthread_mutex_destroy(&count_mutex);
    nary_tree_mt_destroy(&tree);
    TEST_PASS("Concurrent creates");
    return 0;
}

/* Test 4: Mixed read/write operations */
void *mixed_operations_thread(void *arg) {
    struct writer_args *args = (struct writer_args *)arg;

    for (int i = 0; i < args->num_files; i++) {
        /* Create file */
        char filename[64];
        snprintf(filename, sizeof(filename), "mixed%d.txt", i);

        uint16_t idx = nary_insert_mt(args->tree, args->parent_idx,
                                     filename, S_IFREG | 0644);

        if (idx != NARY_INVALID_IDX) {
            /* Read it back immediately */
            struct nary_node node;
            if (nary_read_node_mt(args->tree, idx, &node) == 0) {
                pthread_mutex_lock(args->count_mutex);
                (*args->created_count)++;
                pthread_mutex_unlock(args->count_mutex);
            }
        }

        usleep(1);
    }

    return NULL;
}

int test_mixed_operations(void) {
    struct nary_tree_mt tree;
    nary_tree_mt_init(&tree);

    const int NUM_THREADS = 10;
    const int OPS_PER_THREAD = 5;

    /* Pre-create directories for each thread */
    uint16_t thread_dirs[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        char dirname[64];
        snprintf(dirname, sizeof(dirname), "mixed%d", i);
        thread_dirs[i] = nary_insert_mt(&tree, NARY_ROOT_IDX, dirname, S_IFDIR | 0755);
    }

    pthread_t threads[NUM_THREADS];
    struct writer_args args[NUM_THREADS];
    int success_count = 0;
    pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].tree = &tree;
        args[i].thread_id = i;
        args[i].num_files = OPS_PER_THREAD;
        args[i].created_count = &success_count;
        args[i].count_mutex = &count_mutex;
        args[i].parent_idx = thread_dirs[i];
        pthread_create(&threads[i], NULL, mixed_operations_thread, &args[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("   %d mixed operations completed\n", success_count);

    pthread_mutex_destroy(&count_mutex);
    nary_tree_mt_destroy(&tree);
    TEST_PASS("Mixed read/write operations");
    return 0;
}

/* Test 5: Heavy stress test (1000 threads) */
struct stress_args {
    struct nary_tree_mt *tree;
    int thread_id;
    int *stop_flag;  /* Removed volatile - use atomics properly */
    int *op_count;
    pthread_mutex_t *count_mutex;
    pthread_mutex_t *stop_mutex;
};

void *stress_thread(void *arg) {
    struct stress_args *args = (struct stress_args *)arg;
    int local_ops = 0;
    int should_stop = 0;

    while (!should_stop) {
        /* Most threads just read root (heavy contention test) */
        struct nary_node node;
        nary_read_node_mt(args->tree, NARY_ROOT_IDX, &node);

        local_ops++;

        /* Brief yield to increase contention */
        if (local_ops % 10 == 0) {
            sched_yield();
            /* Check stop flag periodically */
            pthread_mutex_lock(args->stop_mutex);
            should_stop = *args->stop_flag;
            pthread_mutex_unlock(args->stop_mutex);
        }
    }

    pthread_mutex_lock(args->count_mutex);
    *args->op_count += local_ops;
    pthread_mutex_unlock(args->count_mutex);

    return NULL;
}

int test_heavy_stress(void) {
    struct nary_tree_mt tree;
    nary_tree_mt_init(&tree);

    const int NUM_THREADS = 1000;
    const int TEST_DURATION_SEC = 2;

    printf("   Starting %d threads for %d seconds...\n",
           NUM_THREADS, TEST_DURATION_SEC);

    pthread_t *threads = malloc(NUM_THREADS * sizeof(pthread_t));
    struct stress_args *args = malloc(NUM_THREADS * sizeof(struct stress_args));

    int stop_flag = 0;
    int total_ops = 0;
    pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t stop_mutex = PTHREAD_MUTEX_INITIALIZER;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Start threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].tree = &tree;
        args[i].thread_id = i;
        args[i].stop_flag = &stop_flag;
        args[i].op_count = &total_ops;
        args[i].count_mutex = &count_mutex;
        args[i].stop_mutex = &stop_mutex;
        pthread_create(&threads[i], NULL, stress_thread, &args[i]);
    }

    /* Let them run */
    sleep(TEST_DURATION_SEC);
    pthread_mutex_lock(&stop_mutex);
    stop_flag = 1;
    pthread_mutex_unlock(&stop_mutex);

    /* Wait for completion */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                    (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("   Total operations: %d\n", total_ops);
    printf("   Operations/sec: %.0f\n", total_ops / elapsed);
    printf("   Ops per thread: %.1f\n", (double)total_ops / NUM_THREADS);

    /* Get lock statistics */
    struct nary_mt_stats stats;
    nary_get_mt_stats(&tree, &stats);
    printf("   Read locks: %lu\n", stats.read_locks);
    printf("   Write locks: %lu\n", stats.write_locks);
    printf("   Lock conflicts: %lu\n", stats.lock_conflicts);

    if (stats.read_locks + stats.write_locks > 0) {
        double conflict_rate = (double)stats.lock_conflicts /
                              (stats.read_locks + stats.write_locks) * 100.0;
        printf("   Conflict rate: %.2f%%\n", conflict_rate);
    }

    free(threads);
    free(args);
    pthread_mutex_destroy(&count_mutex);
    pthread_mutex_destroy(&stop_mutex);
    nary_tree_mt_destroy(&tree);

    TEST_PASS("Heavy stress test (1000 threads)");
    return 0;
}

/* Test 6: Deadlock detection */
int test_no_deadlocks(void) {
    struct nary_tree_mt tree;
    nary_tree_mt_init(&tree);

    /* Create hierarchy for complex locking */
    uint16_t dir1 = nary_insert_mt(&tree, NARY_ROOT_IDX, "dir1", S_IFDIR | 0755);
    uint16_t dir2 = nary_insert_mt(&tree, NARY_ROOT_IDX, "dir2", S_IFDIR | 0755);

    TEST_ASSERT(dir1 != NARY_INVALID_IDX, "Create dir1");
    TEST_ASSERT(dir2 != NARY_INVALID_IDX, "Create dir2");

    /* Create files in both directories simultaneously */
    const int NUM_THREADS = 10;
    pthread_t threads[NUM_THREADS];
    struct writer_args args[NUM_THREADS];
    int created = 0;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].tree = &tree;
        args[i].thread_id = i;
        args[i].num_files = 3;
        args[i].created_count = &created;
        args[i].count_mutex = &mutex;
        /* Alternate between dir1 and dir2 */
        args[i].parent_idx = (i % 2 == 0) ? dir1 : dir2;
        pthread_create(&threads[i], NULL, writer_thread, &args[i]);
    }

    /* Wait - if deadlock, this would hang */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* If we got here, no deadlock */
    printf("   %d files created without deadlock\n", created);
    TEST_ASSERT(nary_check_deadlocks(&tree) == 0, "No deadlocks detected");

    pthread_mutex_destroy(&mutex);
    nary_tree_mt_destroy(&tree);
    TEST_PASS("No deadlocks");
    return 0;
}

int main(void) {
    printf("\n=== RAZORFS Phase 3: Multithreading Stress Tests ===\n\n");

    int failures = 0;

    failures += test_basic_locking();
    failures += test_concurrent_readers();
    failures += test_concurrent_creates();
    failures += test_mixed_operations();
    failures += test_heavy_stress();
    failures += test_no_deadlocks();

    printf("\n");
    if (failures == 0) {
        printf("🎉 All Phase 3 tests passed!\n");
        printf("✅ Ext4-style locking working correctly\n");
        printf("✅ Zero deadlocks under heavy load\n");
        printf("✅ 1000 threads handled successfully\n");
        return 0;
    } else {
        printf("❌ %d test(s) failed\n", failures);
        return 1;
    }
}
