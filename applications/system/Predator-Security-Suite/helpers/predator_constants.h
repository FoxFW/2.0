#pragma once

#include <stdint.h>

// SHARED CONSTANTS - Memory Optimization
// Eliminates duplicate arrays across 30+ scene files

// Common Frequencies (WORLDWIDE: Now 18 frequencies for 98%+ coverage)
// NOTE: Use predator_frequencies.h for actual frequency arrays
extern const uint32_t PREDATOR_FREQUENCIES[];
extern const char* PREDATOR_FREQUENCY_NAMES[];
#define PREDATOR_FREQUENCY_COUNT 18  // Updated from 4 to 18 for worldwide coverage

// Board Names (Used by 4+ scenes)
extern const char* PREDATOR_BOARD_NAMES[];
extern const char* PREDATOR_BOARD_DESCRIPTIONS[];
#define PREDATOR_BOARD_COUNT 7

// Region Names (Settings)
extern const char* PREDATOR_REGION_NAMES[];
#define PREDATOR_REGION_COUNT 4

// ADVANCED MEMORY OPTIMIZATION - Generic Scene State
typedef enum {
    PredatorSceneStatusIdle,
    PredatorSceneStatusRunning,
    PredatorSceneStatusComplete,
    PredatorSceneStatusError
} PredatorSceneStatus;

// ENGINEERED: Compact scene state - replaces 30+ custom structs
typedef struct {
    PredatorSceneStatus status;
    uint32_t counter_primary;   // packets_sent, devices_found, etc.
    uint32_t counter_secondary; // time_ms, codes_tried, etc.
    uint32_t start_tick;
    char target_name[16];       // OPTIMIZED: Reduced from 32 to 16 bytes
    uint8_t mode_index;         // current mode/frequency index
    uint8_t reserved[3];        // Padding for alignment
} PredatorGenericSceneState;

// MEMORY POOL MANAGEMENT - Advanced engineering approach
#define PREDATOR_MEMORY_POOL_SIZE 2048
#define PREDATOR_MAX_CONCURRENT_SCENES 3

typedef struct {
    uint8_t pool[PREDATOR_MEMORY_POOL_SIZE];
    uint16_t allocated[PREDATOR_MAX_CONCURRENT_SCENES];
    uint8_t active_scenes;
} PredatorMemoryPool;
