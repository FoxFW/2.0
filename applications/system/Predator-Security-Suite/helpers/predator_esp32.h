#pragma once

#include <furi.h>

typedef struct PredatorEsp32 PredatorEsp32;
typedef struct PredatorApp PredatorApp;

// ESP32 callback for received data
void predator_esp32_rx_callback(uint8_t* buf, size_t len, void* context);

// ESP32 management functions
void predator_esp32_init(PredatorApp* app);
void predator_esp32_deinit(PredatorApp* app);
bool predator_esp32_send_command(PredatorApp* app, const char* command);
bool predator_esp32_is_connected(PredatorApp* app);

// WiFi attack functions
bool predator_esp32_wifi_scan(PredatorApp* app);
bool predator_esp32_wifi_deauth(PredatorApp* app, int channel);
bool predator_esp32_wifi_evil_twin(PredatorApp* app);

// Bluetooth attack functions
bool predator_esp32_ble_scan(PredatorApp* app);
bool predator_esp32_ble_spam(PredatorApp* app, uint8_t mode);

// Wardriving functions
bool predator_esp32_wardrive(PredatorApp* app);
bool predator_esp32_start_wardriving(PredatorApp* app);

// Status and control
bool predator_esp32_stop_attack(PredatorApp* app);
bool predator_esp32_get_status(PredatorApp* app);
