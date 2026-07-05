#include "predator_test_framework.h"
#include <string.h>

bool test_run_suite(TestSuite* suite) {
    if (!suite || !suite->test_cases || suite->test_count == 0) {
        FURI_LOG_E("TEST", "Invalid test suite");
        return false;
    }

    TestStats stats = {0};
    stats.total = suite->test_count;

    FURI_LOG_I("TEST", "=== Running Test Suite: %s ===", suite->name);
    
    // Call setup once for the whole suite
    if (suite->setup) {
        suite->setup(suite->context);
    }

    // Run each test
    for (size_t i = 0; i < suite->test_count; i++) {
        if (!suite->test_cases[i].enabled) {
            FURI_LOG_D("TEST", "Skipping disabled test: %s", suite->test_cases[i].name);
            stats.skipped++;
            test_print_result(suite->test_cases[i].name, TestResultSkip);
            continue;
        }

        FURI_LOG_D("TEST", "Running test: %s", suite->test_cases[i].name);
        TestResult result = suite->test_cases[i].test_func(suite->context);
        test_print_result(suite->test_cases[i].name, result);

        // Update stats
        switch (result) {
            case TestResultPass:
                stats.passed++;
                break;
            case TestResultFail:
                stats.failed++;
                break;
            case TestResultSkip:
                stats.skipped++;
                break;
        }
    }

    // Call teardown once for the whole suite
    if (suite->teardown) {
        suite->teardown(suite->context);
    }

    // Print summary
    FURI_LOG_I(
        "TEST",
        "=== Test Suite %s: %d tests, %d passed, %d failed, %d skipped ===",
        suite->name,
        stats.total,
        stats.passed,
        stats.failed,
        stats.skipped);

    return stats.failed == 0;
}

void test_print_result(const char* name, TestResult result) {
    const char* result_str;
    const char* tag;

    switch (result) {
        case TestResultPass:
            result_str = "PASS";
            tag = "TEST";
            break;
        case TestResultFail:
            result_str = "FAIL";
            tag = "TEST_ERR";
            break;
        case TestResultSkip:
            result_str = "SKIP";
            tag = "TEST_WARN";
            break;
        default:
            result_str = "UNKNOWN";
            tag = "TEST_ERR";
            break;
    }

    FURI_LOG_I(tag, "[%s] %s", result_str, name);
}
