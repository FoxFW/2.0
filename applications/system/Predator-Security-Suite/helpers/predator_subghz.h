/**
 * @file predator_subghz.h
 * @brief SubGHz Unified Interface - Facade for all SubGHz modules
 * 
 * This header provides a unified interface to all SubGHz functionality.
 * Internally, it delegates to specialized modules:
 * - predator_subghz_core: Hardware management
 * - predator_subghz_car: Car-specific attacks
 * - predator_subghz_rolling: Rolling code attacks
 * - predator_subghz_jamming: Jamming operations
 * 
 * @note v2.0 Refactored: Modular architecture for better maintainability
 */

#pragma once

#include "../predator_i.h"

// Include modular SubGHz headers
#include "subghz/predator_subghz_core.h"
#include "subghz/predator_subghz_car.h"
#include "subghz/predator_subghz_rolling.h"
#include "subghz/predator_subghz_jamming.h"

// Re-export types from car module for backward compatibility
typedef CarModel CarModel;
typedef CarCommand CarCommand;

// All functions are now provided by the modular headers above
// This file serves as a convenience facade for existing code
