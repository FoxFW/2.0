#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "predator_boards.h"

// Forward declaration
struct PredatorApp;

// Real Attack Types (NO SIMULATION)
typedef enum {
    RealAttackTypeRollingCode,
    RealAttackTypeFixedCode,
    RealAttackTypeSmartKey,
    RealAttackTypeJamming
} RealAttackType;

// Real WiFi Attack Types
typedef enum {
    RealWiFiAttackScan,
    RealWiFiAttackDeauth,
    RealWiFiAttackEvilTwin
} RealWiFiAttackType;

// Real Bluetooth Attack Types
typedef enum {
    RealBluetoothAttackScan,
    RealBluetoothAttackSpam
} RealBluetoothAttackType;

// Real Car Attack Configuration
typedef struct {
    RealAttackType attack_type;
    uint32_t frequency;
    char target_make[32];
    char target_model[32];
    char attack_type_str[32];
    uint8_t power_level;
    uint32_t duration_ms;
} RealCarAttackConfig;

// Real WiFi Attack Configuration
typedef struct {
    RealWiFiAttackType attack_type;
    char target_ssid[32];
    uint8_t channel;
    char attack_type_str[32];
    uint32_t duration_ms;
} RealWiFiAttackConfig;

// Real Bluetooth Attack Configuration
typedef struct {
    RealBluetoothAttackType attack_type;
    char target_device[32];
    uint8_t spam_mode;
    char attack_type_str[32];
    uint32_t duration_ms;
} RealBluetoothAttackConfig;

// Walking Mode Configuration (Elon's Paris Demo)
typedef struct {
    bool exclude_tesla;
    uint32_t walking_steps;
    uint32_t step_delay_ms;
    bool auto_attack_enabled;
    uint8_t attack_intensity;
} WalkingModeConfig;

// Real Attack Engine State
typedef struct {
    bool hardware_validated;
    bool subghz_ready;
    bool esp32_ready;
    bool gps_ready;
    bool has_external_rf;
    uint8_t max_power_dbm;
    PredatorBoardConfig board_config;
    
    // VIP Mode Settings
    bool vip_mode;
    bool unlimited_power;
    bool all_frequencies_enabled;
    
    // Attack Statistics
    uint32_t total_attacks;
    uint32_t successful_attacks;
    uint32_t failed_attacks;
    uint32_t real_transmissions;
} RealAttackEngine;

// Real Attack Statistics
typedef struct {
    uint32_t total_attacks;
    uint32_t successful_attacks;
    uint32_t failed_attacks;
    uint32_t real_transmissions;
    float success_rate;
    bool hardware_validated;
    bool vip_mode_active;
} RealAttackStats;

// Real Attack Engine Functions
bool predator_real_attack_init(struct PredatorApp* app);

bool predator_real_attack_validate_hardware(struct PredatorApp* app);

bool predator_real_attack_execute_car(struct PredatorApp* app, RealCarAttackConfig* config);

bool predator_real_attack_execute_wifi(struct PredatorApp* app, RealWiFiAttackConfig* config);

bool predator_real_attack_execute_bluetooth(struct PredatorApp* app, RealBluetoothAttackConfig* config);

bool predator_real_attack_walking_mode(struct PredatorApp* app, WalkingModeConfig* config);

RealAttackStats predator_real_attack_get_stats(void);

void predator_real_attack_shutdown(struct PredatorApp* app);

// Internal attack implementations - remove static declarations to avoid unused function errors
