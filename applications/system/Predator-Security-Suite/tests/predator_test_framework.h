#pragma once

#include <furi.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/**
 * @brief Simple test framework for Predator app components
 * 
 * This framework provides a lightweight testing infrastructure for
 * unit testing Predator Security Suite components. It supports:
 * - Test suites with setup/teardown hooks
 * - Multiple assertion macros
 * - Test result tracking and reporting
 * - Skip functionality for conditional tests
 */

// Test result states
typedef enum {
    TestResultPass,
    TestResultFail,
    TestResultSkip
} TestResult;

// Test case structure
typedef struct {
    const char* name;
    TestResult (*test_func)(void* context);
    bool enabled;
} TestCase;

// Test suite structure
typedef struct {
    const char* name;
    TestCase* test_cases;
    size_t test_count;
    void* context;
    void (*setup)(void* context);
    void (*teardown)(void* context);
} TestSuite;

// Test framework stats
typedef struct {
    int total;
    int passed;
    int failed;
    int skipped;
} TestStats;

/**
 * @brief Run a test suite and print results
 * @param suite Test suite to run
 * @return true if all tests passed
 */
bool test_run_suite(TestSuite* suite);

/**
 * @brief Print test result to console
 * @param name Test name
 * @param result Test result
 */
void test_print_result(const char* name, TestResult result);

/**
 * @brief Assert that condition is true
 * @param condition Condition to test
 * @return TestResultPass if true, TestResultFail if false
 */
#define TEST_ASSERT(condition) \
    do { \
        if(!(condition)) { \
            FURI_LOG_E("TEST", "Assertion failed: %s", #condition); \
            return TestResultFail; \
        } \
    } while(0)

/**
 * @brief Assert that condition is true with custom message
 * @param condition Condition to test
 * @param msg Custom failure message
 * @return TestResultPass if true, TestResultFail if false
 */
#define TEST_ASSERT_MSG(condition, msg) \
    do { \
        if(!(condition)) { \
            FURI_LOG_E("TEST", "Assertion failed: %s - %s", #condition, msg); \
            return TestResultFail; \
        } \
    } while(0)

/**
 * @brief Assert that two integers are equal
 * @param expected Expected value
 * @param actual Actual value
 * @return TestResultPass if equal, TestResultFail if not
 */
#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    do { \
        if((expected) != (actual)) { \
            FURI_LOG_E("TEST", "Expected %d, got %d", (expected), (actual)); \
            return TestResultFail; \
        } \
    } while(0)

/**
 * @brief Assert that two unsigned integers are equal
 * @param expected Expected value
 * @param actual Actual value
 * @return TestResultPass if equal, TestResultFail if not
 */
#define TEST_ASSERT_EQUAL_UINT(expected, actual) \
    do { \
        if((expected) != (actual)) { \
            FURI_LOG_E("TEST", "Expected %u, got %u", (unsigned)(expected), (unsigned)(actual)); \
            return TestResultFail; \
        } \
    } while(0)

/**
 * @brief Assert that two strings are equal
 * @param expected Expected string
 * @param actual Actual string
 * @return TestResultPass if equal, TestResultFail if not
 */
#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    do { \
        if(strcmp((expected), (actual)) != 0) { \
            FURI_LOG_E("TEST", "Expected \"%s\", got \"%s\"", (expected), (actual)); \
            return TestResultFail; \
        } \
    } while(0)

/**
 * @brief Assert that pointer is not NULL
 * @param ptr Pointer to check
 * @return TestResultPass if not NULL, TestResultFail if NULL
 */
#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if((ptr) == NULL) { \
            FURI_LOG_E("TEST", "Expected non-NULL pointer"); \
            return TestResultFail; \
        } \
    } while(0)

/**
 * @brief Assert that pointer is NULL
 * @param ptr Pointer to check
 * @return TestResultPass if NULL, TestResultFail if not NULL
 */
#define TEST_ASSERT_NULL(ptr) \
    do { \
        if((ptr) != NULL) { \
            FURI_LOG_E("TEST", "Expected NULL pointer"); \
            return TestResultFail; \
        } \
    } while(0)

/**
 * @brief Assert that two memory blocks are equal
 * @param expected Expected memory
 * @param actual Actual memory
 * @param size Size in bytes to compare
 * @return TestResultPass if equal, TestResultFail if not
 */
#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, size) \
    do { \
        if(memcmp((expected), (actual), (size)) != 0) { \
            FURI_LOG_E("TEST", "Memory comparison failed (%zu bytes)", (size_t)(size)); \
            return TestResultFail; \
        } \
    } while(0)

/**
 * @brief Assert that a value is true (non-zero)
 * @param value Value to check
 * @return TestResultPass if true, TestResultFail if false
 */
#define TEST_ASSERT_TRUE(value) \
    do { \
        if(!(value)) { \
            FURI_LOG_E("TEST", "Expected true, got false"); \
            return TestResultFail; \
        } \
    } while(0)

/**
 * @brief Assert that a value is false (zero)
 * @param value Value to check
 * @return TestResultPass if false, TestResultFail if true
 */
#define TEST_ASSERT_FALSE(value) \
    do { \
        if((value)) { \
            FURI_LOG_E("TEST", "Expected false, got true"); \
            return TestResultFail; \
        } \
    } while(0)

/**
 * @brief Assert that a value is within a range (inclusive)
 * @param value Value to check
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @return TestResultPass if in range, TestResultFail if not
 */
#define TEST_ASSERT_IN_RANGE(value, min, max) \
    do { \
        if((value) < (min) || (value) > (max)) { \
            FURI_LOG_E("TEST", "Value %d not in range [%d, %d]", (int)(value), (int)(min), (int)(max)); \
            return TestResultFail; \
        } \
    } while(0)

/**
 * @brief Assert that two floating point values are approximately equal
 * @param expected Expected value
 * @param actual Actual value
 * @param epsilon Maximum allowed difference
 * @return TestResultPass if approximately equal, TestResultFail if not
 */
#define TEST_ASSERT_FLOAT_EQUAL(expected, actual, epsilon) \
    do { \
        float _diff = (expected) - (actual); \
        if(_diff < 0) _diff = -_diff; \
        if(_diff > (epsilon)) { \
            FURI_LOG_E("TEST", "Expected %f, got %f (epsilon: %f)", \
                      (double)(expected), (double)(actual), (double)(epsilon)); \
            return TestResultFail; \
        } \
    } while(0)

/**
 * @brief Fail the test unconditionally with a message
 * @param msg Failure message
 */
#define TEST_FAIL(msg) \
    do { \
        FURI_LOG_E("TEST", "Test failed: %s", msg); \
        return TestResultFail; \
    } while(0)

/**
 * @brief Skip the current test
 * @return TestResultSkip
 */
#define TEST_SKIP() \
    do { \
        return TestResultSkip; \
    } while(0)

/**
 * @brief Skip the current test with a message
 * @param msg Skip message
 * @return TestResultSkip
 */
#define TEST_SKIP_MSG(msg) \
    do { \
        FURI_LOG_W("TEST", "Test skipped: %s", msg); \
        return TestResultSkip; \
    } while(0)
