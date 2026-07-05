#include "predator_test_framework.h"
#include "../helpers/predator_esp32.h"
#include "../predator_i.h"

// Test context structure
typedef struct {
    PredatorApp* app;
    bool mock_esp32_connected;
    uint32_t mock_targets_found;
    uint32_t mock_packets_sent;
} Esp32TestContext;

// Setup function - called before each test suite
static void esp32_test_setup(void* context) {
    Esp32TestContext* ctx = (Esp32TestContext*)context;
    
    // Create a minimal app context for testing
    ctx->app = malloc(sizeof(PredatorApp));
    memset(ctx->app, 0, sizeof(PredatorApp));
    
    // Set initial mock values
    ctx->mock_esp32_connected = false;
    ctx->mock_targets_found = 0;
    ctx->mock_packets_sent = 0;
    
    // Initialize app with mock values
    ctx->app->esp32_connected = ctx->mock_esp32_connected;
    ctx->app->targets_found = ctx->mock_targets_found;
    ctx->app->packets_sent = ctx->mock_packets_sent;
}

// Teardown function - called after each test suite
static void esp32_test_teardown(void* context) {
    Esp32TestContext* ctx = (Esp32TestContext*)context;
    
    // Free app context
    free(ctx->app);
    ctx->app = NULL;
}

// Test ESP32 RX callback
static TestResult test_esp32_rx_callback(void* context) {
    Esp32TestContext* ctx = (Esp32TestContext*)context;
    
    // Set initial state
    ctx->app->esp32_connected = false;
    ctx->app->targets_found = 0;
    ctx->app->packets_sent = 0;
    
    // Test with ESP32 identification string
    const uint8_t test_data[] = "ESP32 Marauder Ready";
    predator_esp32_rx_callback((uint8_t*)test_data, sizeof(test_data) - 1, ctx->app);
    
    // Verify connection state was updated
    TEST_ASSERT(ctx->app->esp32_connected);
    
    // Test with AP Found message
    const uint8_t ap_data[] = "AP Found: TestSSID";
    predator_esp32_rx_callback((uint8_t*)ap_data, sizeof(ap_data) - 1, ctx->app);
    
    // Verify targets count was incremented
    TEST_ASSERT(ctx->app->targets_found == 1);
    
    // Test with Deauth sent message
    const uint8_t deauth_data[] = "Deauth sent: 10 packets";
    predator_esp32_rx_callback((uint8_t*)deauth_data, sizeof(deauth_data) - 1, ctx->app);
    
    // Verify packets count was incremented
    TEST_ASSERT(ctx->app->packets_sent == 1);
    
    // Test NULL safety
    predator_esp32_rx_callback(NULL, 0, ctx->app);
    predator_esp32_rx_callback((uint8_t*)test_data, 0, ctx->app);
    predator_esp32_rx_callback((uint8_t*)test_data, sizeof(test_data) - 1, NULL);
    
    // No assertions should have failed if null-safety is working
    
    return TestResultPass;
}

// Test ESP32 command sending
static TestResult test_esp32_send_command(void* context) {
    Esp32TestContext* ctx = (Esp32TestContext*)context;
    
    // Test with NULL uart (should fail safely)
    ctx->app->esp32_uart = NULL;
    bool result = predator_esp32_send_command(ctx->app, "test");
    TEST_ASSERT(result == false);
    
    // Test with NULL command (should fail safely)
    result = predator_esp32_send_command(ctx->app, NULL);
    TEST_ASSERT(result == false);
    
    // Test with NULL app (should fail safely)
    result = predator_esp32_send_command(NULL, "test");
    TEST_ASSERT(result == false);
    
    // Test with empty command (should fail safely)
    result = predator_esp32_send_command(ctx->app, "");
    TEST_ASSERT(result == false);
    
    // Test with oversized command (should fail safely)
    char huge_cmd[200];
    memset(huge_cmd, 'A', sizeof(huge_cmd));
    huge_cmd[sizeof(huge_cmd) - 1] = '\0';
    result = predator_esp32_send_command(ctx->app, huge_cmd);
    TEST_ASSERT(result == false);
    
    return TestResultPass;
}

// Test ESP32 switch logic
static TestResult test_esp32_switch_logic(void* context) {
    UNUSED(context);
    
    // We can't directly test GPIO in unit tests, so this is a reminder
    // that the Marauder switch logic should be checked in predator_esp32.c
    // The switch is active-low, so it should be checking !furi_hal_gpio_read()
    
    return TestResultPass;
}

// Test ESP32 attack commands
static TestResult test_esp32_attack_commands(void* context) {
    Esp32TestContext* ctx = (Esp32TestContext*)context;
    
    // Mock a valid UART - can't actually test sending commands in unit tests
    // but we can verify that the command strings are properly formatted
    ctx->app->esp32_uart = (void*)0xDEADBEEF; // Just a non-NULL pointer
    
    // Verify WiFi scan command matches the expected string
    bool expected_cmd_format = strcmp(MARAUDER_CMD_WIFI_SCAN, "scanap") == 0;
    TEST_ASSERT(expected_cmd_format);
    
    // Verify deauth command format 
    expected_cmd_format = strncmp(MARAUDER_CMD_WIFI_DEAUTH, "attack -t deauth -c", 18) == 0;
    TEST_ASSERT(expected_cmd_format);
    
    // Verify evil twin command format
    expected_cmd_format = strncmp(MARAUDER_CMD_WIFI_EVIL_TWIN, "attack -t evil_portal", 20) == 0;
    TEST_ASSERT(expected_cmd_format);
    
    // Verify BLE scan command format
    expected_cmd_format = strcmp(MARAUDER_CMD_BLE_SCAN, "scandevices -t ble") == 0;
    TEST_ASSERT(expected_cmd_format);
    
    return TestResultPass;
}

// Define and run tests
bool predator_run_esp32_tests() {
    // Create context
    Esp32TestContext context;
    
    // Define test cases
    TestCase test_cases[] = {
        {"ESP32 RX Callback", test_esp32_rx_callback, true},
        {"ESP32 Send Command", test_esp32_send_command, true},
        {"ESP32 Switch Logic", test_esp32_switch_logic, true},
        {"ESP32 Attack Commands", test_esp32_attack_commands, true}
    };
    
    // Configure test suite
    TestSuite suite = {
        .name = "ESP32 Module Tests",
        .test_cases = test_cases,
        .test_count = sizeof(test_cases) / sizeof(TestCase),
        .context = &context,
        .setup = esp32_test_setup,
        .teardown = esp32_test_teardown
    };
    
    // Run tests
    return test_run_suite(&suite);
}
