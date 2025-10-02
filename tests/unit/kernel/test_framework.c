/*
 * RazorFS Kernel Test Framework Implementation
 */

#include "test_framework.h"
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

/* Global test results */
struct test_results g_test_results = {0, 0, 0, ""};

/* Memory tracking for leak detection */
static int allocation_count = 0;

/* Test suite runner implementation */
int run_test_suite(const char *suite_name, struct test_case *tests, int num_tests) {
    printf(COLOR_BLUE "\n=== Running Test Suite: %s ===" COLOR_RESET "\n", suite_name);
    
    TEST_INIT();
    
    for (int i = 0; i < num_tests; i++) {
        TEST_START(tests[i].name);
        
        /* Reset memory tracking for each test */
        reset_allocation_tracking();
        
        /* Run the test */
        int result = tests[i].func();
        
        if (result == 0) {
            /* Check for memory leaks */
            if (get_allocation_count() != 0) {
                char leak_msg[256];
                snprintf(leak_msg, sizeof(leak_msg), 
                        "Memory leak detected: %d unfreed allocations", 
                        get_allocation_count());
                TEST_FAIL(leak_msg);
            } else {
                TEST_PASS();
            }
        }
        /* If result != 0, test already called TEST_FAIL */
    }
    
    TEST_SUMMARY();
}

/* Memory testing helpers */
void* test_malloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr) {
        allocation_count++;
    }
    return ptr;
}

void test_free(void *ptr) {
    if (ptr) {
        free(ptr);
        allocation_count--;
    }
}

int get_allocation_count(void) {
    return allocation_count;
}

void reset_allocation_tracking(void) {
    allocation_count = 0;
}

/* Timer helpers */
uint64_t get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

void test_sleep_ms(int milliseconds) {
    usleep(milliseconds * 1000);
}