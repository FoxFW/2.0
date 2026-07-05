/**
 * @file predator_scene_car_shared.h
 * @brief Shared Types and Constants for Car Security Scenes
 * 
 * This header contains common types, enums, and constants used across
 * all car security scene modules. Centralizing these prevents duplication
 * and ensures consistency.
 * 
 * @note v2.0 Refactored: Modular car scenes architecture
 */

#pragma once

#include "../../predator_i.h"
#include "../predator_scene.h"
#include "../../helpers/predator_models.h"
#include "../../helpers/predator_boards.h"
#include "../../helpers/predator_subghz.h"

// ============================================================================
// SCENE INDICES
// ============================================================================

// Scene indices for car-related scenes (from predator_scene.h)
// These are already defined in the scene manager, just documenting here

// ============================================================================
// COMMON CONSTANTS
// ============================================================================

/** @brief Standard delay between attack iterations (ms) */
#define CAR_ATTACK_DELAY_MS 100

/** @brief Delay for UI feedback (ms) */
#define CAR_UI_FEEDBACK_DELAY_MS 500

/** @brief Back button debounce time (ms) */
#define CAR_BACK_DEBOUNCE_MS 500

/** @brief Maximum number of bruteforce attempts */
#define CAR_MAX_BRUTEFORCE_ATTEMPTS 1000

/** @brief Default jamming duration (ms) */
#define CAR_DEFAULT_JAM_DURATION_MS 5000

// ============================================================================
// ATTACK TYPE ENUMS
// ============================================================================

/**
 * @brief Car attack types
 */
typedef enum {
    CarAttackTypeBruteforce,    ///< Brute force key codes
    CarAttackTypeRollingCode,   ///< Rolling code capture/replay
    CarAttackTypeFixedCode,     ///< Fixed code transmission
    CarAttackTypeJamming,       ///< RF jamming
    CarAttackTypePassive,       ///< Passive signal capture
    CarAttackTypeSmartKey,      ///< Smart key exploitation
    CarAttackTypeCount
} CarAttackType;

/**
 * @brief Protocol types for testing
 */
typedef enum {
    CarProtocolKeeloq,          ///< Keeloq rolling code
    CarProtocolHitag2,          ///< Hitag2 rolling code
    CarProtocolAES,             ///< AES encrypted
    CarProtocolFixed,           ///< Fixed code
    CarProtocolCount
} CarProtocol;

/**
 * @brief Parking barrier types
 */
typedef enum {
    BarrierTypeFAAC,            ///< FAAC (Italy)
    BarrierTypeBFT,             ///< BFT (Italy)
    BarrierTypeCAME,            ///< CAME (Italy/International)
    BarrierTypeNice,            ///< Nice (France/International)
    BarrierTypeSomfy,           ///< Somfy (France)
    BarrierTypeEnterprise,      ///< Enterprise Professional (Worldwide)
    BarrierTypeGeneric,         ///< Generic barrier
    BarrierTypeCount
} BarrierType;

// ============================================================================
// ATTACK STATE STRUCTURES
// ============================================================================

/**
 * @brief Attack state for bruteforce operations
 */
typedef struct {
    bool active;                ///< Attack is running
    uint32_t attempts;          ///< Number of attempts made
    uint32_t current_code;      ///< Current code being tested
    uint32_t frequency;         ///< Target frequency
    uint32_t start_tick;        ///< Start time
} CarBruteforceState;

/**
 * @brief Attack state for jamming operations
 */
typedef struct {
    bool active;                ///< Jamming is active
    uint32_t frequency;         ///< Jamming frequency
    uint32_t duration_ms;       ///< Jamming duration
    uint32_t start_tick;        ///< Start time
} CarJammingState;

/**
 * @brief Attack state for passive opener
 */
typedef struct {
    bool active;                ///< Passive mode active
    uint32_t signals_captured;  ///< Number of signals captured
    uint32_t last_signal_time;  ///< Timestamp of last signal
    uint32_t frequency;         ///< Listening frequency
} CarPassiveState;

/**
 * @brief Walking mode configuration
 */
typedef struct {
    bool active;                ///< Walking mode active
    bool exclude_tesla;         ///< Exclude Tesla (Elon's requirement)
    uint32_t step_count;        ///< Number of steps taken
    uint32_t cars_attacked;     ///< Number of cars attacked
    uint32_t last_attack_time;  ///< Last attack timestamp
} WalkingModeState;

// ============================================================================
// UTILITY FUNCTIONS (Inline for performance)
// ============================================================================

/**
 * @brief Get attack type name as string
 * @param type Attack type enum
 * @return Const string with attack type name
 */
static inline const char* car_attack_type_name(CarAttackType type) {
    switch(type) {
        case CarAttackTypeBruteforce: return "Bruteforce";
        case CarAttackTypeRollingCode: return "Rolling Code";
        case CarAttackTypeFixedCode: return "Fixed Code";
        case CarAttackTypeJamming: return "Jamming";
        case CarAttackTypePassive: return "Passive";
        case CarAttackTypeSmartKey: return "Smart Key";
        default: return "Unknown";
    }
}

/**
 * @brief Get protocol name as string
 * @param protocol Protocol type enum
 * @return Const string with protocol name
 */
static inline const char* car_protocol_name(CarProtocol protocol) {
    switch(protocol) {
        case CarProtocolKeeloq: return "Keeloq";
        case CarProtocolHitag2: return "Hitag2";
        case CarProtocolAES: return "AES-128";
        case CarProtocolFixed: return "Fixed Code";
        default: return "Unknown";
    }
}

/**
 * @brief Get barrier type name as string
 * @param barrier Barrier type enum
 * @return Const string with barrier name
 */
static inline const char* car_barrier_name(BarrierType barrier) {
    switch(barrier) {
        case BarrierTypeFAAC: return "FAAC";
        case BarrierTypeBFT: return "BFT";
        case BarrierTypeCAME: return "CAME";
        case BarrierTypeNice: return "Nice";
        case BarrierTypeSomfy: return "Somfy";
        case BarrierTypeEnterprise: return "Enterprise Pro";
        case BarrierTypeGeneric: return "Generic";
        default: return "Unknown";
    }
}

/**
 * @brief Check if back button debounce time has passed
 * @param last_press_time Last back button press timestamp
 * @return true if debounce time has passed
 */
static inline bool car_back_debounce_check(uint32_t last_press_time) {
    if(last_press_time == 0) return true;
    return (furi_get_tick() - last_press_time) >= CAR_BACK_DEBOUNCE_MS;
}

// ============================================================================
// LOGGING HELPERS
// ============================================================================

/** @brief Log car attack start */
#define CAR_LOG_ATTACK_START(attack, freq) \
    FURI_LOG_I("CarAttack", "[START] %s attack on %lu Hz", attack, freq)

/** @brief Log car attack stop */
#define CAR_LOG_ATTACK_STOP(attack) \
    FURI_LOG_I("CarAttack", "[STOP] %s attack", attack)

/** @brief Log car attack success */
#define CAR_LOG_ATTACK_SUCCESS(attack, details) \
    FURI_LOG_I("CarAttack", "[SUCCESS] %s: %s", attack, details)

/** @brief Log car attack failure */
#define CAR_LOG_ATTACK_FAILURE(attack, reason) \
    FURI_LOG_E("CarAttack", "[FAILED] %s: %s", attack, reason)
