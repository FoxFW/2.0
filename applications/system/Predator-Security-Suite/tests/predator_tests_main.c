#include <furi.h>
#include <furi_hal.h>
#include <stdio.h>
#include "predator_test_framework.h"

// Forward declarations for test suites
bool predator_run_gps_tests(void);
bool predator_run_esp32_tests(void);
bool predator_run_crypto_tests(void);
bool predator_run_subghz_tests(void);

// Main test entry point
int32_t predator_run_tests(void* p) {
    UNUSED(p);
    
    FURI_LOG_I("TEST", "==== Predator Unit Tests ====");
    FURI_LOG_I("TEST", "Running comprehensive test suite...");
    
    bool all_passed = true;
    int suites_passed = 0;
    int suites_total = 4;
    
    // Run GPS tests
    FURI_LOG_I("TEST", "");
    FURI_LOG_I("TEST", "Running GPS module tests...");
    if(predator_run_gps_tests()) {
        suites_passed++;
    } else {
        all_passed = false;
    }
    
    // Run ESP32 tests
    FURI_LOG_I("TEST", "");
    FURI_LOG_I("TEST", "Running ESP32 module tests...");
    if(predator_run_esp32_tests()) {
        suites_passed++;
    } else {
        all_passed = false;
    }
    
    // Run Crypto Engine tests
    FURI_LOG_I("TEST", "");
    FURI_LOG_I("TEST", "Running Crypto Engine tests...");
    if(predator_run_crypto_tests()) {
        suites_passed++;
    } else {
        all_passed = false;
    }
    
    // Run SubGHz tests
    FURI_LOG_I("TEST", "");
    FURI_LOG_I("TEST", "Running SubGHz module tests...");
    if(predator_run_subghz_tests()) {
        suites_passed++;
    } else {
        all_passed = false;
    }
    
    // Report final status
    FURI_LOG_I("TEST", "");
    FURI_LOG_I("TEST", "====================================");
    if(all_passed) {
        FURI_LOG_I("TEST", "==== All %d/%d test suites PASSED! ====", suites_passed, suites_total);
    } else {
        FURI_LOG_E("TEST", "==== %d/%d test suites PASSED, %d FAILED ====", 
                   suites_passed, suites_total, suites_total - suites_passed);
    }
    FURI_LOG_I("TEST", "====================================");
    
    return all_passed ? 0 : 1;
}
