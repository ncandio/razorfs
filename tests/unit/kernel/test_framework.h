/*
 * RazorFS Kernel Test Framework
 * Simple unit testing framework for kernel-style C code
 */

#ifndef RAZORFS_TEST_FRAMEWORK_H
#define RAZORFS_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>

/* Test result tracking */
struct test_results {
    int total_tests;
    int passed_tests;
    int failed_tests;
    char current_test[256];
};

/* Global test results */
extern struct test_results g_test_results;

/* Color output macros */
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_RESET   "\x1b[0m"

/* Test framework macros */
#define TEST_INIT() \
    do { \
        g_test_results.total_tests = 0; \
        g_test_results.passed_tests = 0; \
        g_test_results.failed_tests = 0; \
        printf(COLOR_BLUE "=== RazorFS Kernel Unit Tests ===" COLOR_RESET "\n"); \
    } while(0)

#define TEST_START(name) \
    do { \
        strncpy(g_test_results.current_test, name, sizeof(g_test_results.current_test) - 1); \
        g_test_results.current_test[sizeof(g_test_results.current_test) - 1] = '\0'; \
        g_test_results.total_tests++; \
        printf(COLOR_YELLOW "Running: %s" COLOR_RESET "\n", name); \
    } while(0)

#define TEST_PASS() \
    do { \
        g_test_results.passed_tests++; \
        printf(COLOR_GREEN "PASS: %s" COLOR_RESET "\n", g_test_results.current_test); \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        g_test_results.failed_tests++; \
        printf(COLOR_RED "FAIL: %s - %s" COLOR_RESET "\n", g_test_results.current_test, msg); \
    } while(0)

#define TEST_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            TEST_FAIL(msg); \
            return -1; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(expected, actual, msg) \
    do { \
        if ((expected) != (actual)) { \
            char fail_msg[512]; \
            snprintf(fail_msg, sizeof(fail_msg), "%s (expected: %ld, actual: %ld)", \
                    msg, (long)(expected), (long)(actual)); \
            TEST_FAIL(fail_msg); \
            return -1; \
        } \
    } while(0)

#define TEST_ASSERT_NEQ(not_expected, actual, msg) \
    do { \
        if ((not_expected) == (actual)) { \
            char fail_msg[512]; \
            snprintf(fail_msg, sizeof(fail_msg), "%s (should not equal: %ld)", \
                    msg, (long)(not_expected)); \
            TEST_FAIL(fail_msg); \
            return -1; \
        } \
    } while(0)

#define TEST_ASSERT_NULL(ptr, msg) \
    do { \
        if ((ptr) != NULL) { \
            TEST_FAIL(msg " (pointer should be NULL)"); \
            return -1; \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr, msg) \
    do { \
        if ((ptr) == NULL) { \
            TEST_FAIL(msg " (pointer should not be NULL)"); \
            return -1; \
        } \
    } while(0)

#define TEST_ASSERT_STR_EQ(expected, actual, msg) \
    do { \
        if (strcmp(expected, actual) != 0) { \
            char fail_msg[512]; \
            snprintf(fail_msg, sizeof(fail_msg), "%s (expected: '%s', actual: '%s')", \
                    msg, expected, actual); \
            TEST_FAIL(fail_msg); \
            return -1; \
        } \
    } while(0)

#define TEST_SUMMARY() \
    do { \
        printf(COLOR_BLUE "\n=== Test Summary ===" COLOR_RESET "\n"); \
        printf("Total Tests: %d\n", g_test_results.total_tests); \
        printf(COLOR_GREEN "Passed: %d" COLOR_RESET "\n", g_test_results.passed_tests); \
        printf(COLOR_RED "Failed: %d" COLOR_RESET "\n", g_test_results.failed_tests); \
        if (g_test_results.failed_tests == 0) { \
            printf(COLOR_GREEN "ALL TESTS PASSED!" COLOR_RESET "\n"); \
        } else { \
            printf(COLOR_RED "SOME TESTS FAILED!" COLOR_RESET "\n"); \
        } \
        return (g_test_results.failed_tests == 0) ? 0 : 1; \
    } while(0)

/* Memory safety helpers */
#define SAFE_FREE(ptr) \
    do { \
        if (ptr) { \
            free(ptr); \
            ptr = NULL; \
        } \
    } while(0)

/* Test runner function pointer type */
typedef int (*test_function_t)(void);

/* Test case structure */
struct test_case {
    const char *name;
    test_function_t func;
};

/* Test suite runner */
int run_test_suite(const char *suite_name, struct test_case *tests, int num_tests);

/* Memory testing helpers */
void* test_malloc(size_t size);
void test_free(void *ptr);
int get_allocation_count(void);
void reset_allocation_tracking(void);

/* Timer helpers for performance testing */
uint64_t get_timestamp_us(void);
void test_sleep_ms(int milliseconds);

#endif /* RAZORFS_TEST_FRAMEWORK_H */