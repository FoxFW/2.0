#pragma once

#include "../predator_i.h"

/**
 * @brief Initialize watchdog system
 * @param app Predator application context
 * @return true if successful
 */
bool predator_watchdog_init(PredatorApp* app);

/**
 * @brief Start watchdog monitoring
 * @param app Predator application context
 * @param timeout_ms Timeout in milliseconds before watchdog triggers
 * @return true if started successfully
 */
bool predator_watchdog_start(PredatorApp* app, uint32_t timeout_ms);

/**
 * @brief Pet/kick the watchdog to prevent timeout
 * @param app Predator application context
 */
void predator_watchdog_kick(PredatorApp* app);

/**
 * @brief Stop watchdog monitoring
 * @param app Predator application context
 */
void predator_watchdog_stop(PredatorApp* app);

/**
 * @brief Process watchdog tick
 * Should be called regularly from main loop
 * @param app Predator application context
 */
void predator_watchdog_tick(PredatorApp* app);
