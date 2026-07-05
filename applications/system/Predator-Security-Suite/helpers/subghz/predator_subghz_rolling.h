/**
 * @file predator_subghz_rolling.h
 * @brief SubGHz Rolling Code Attacks
 * 
 * This module handles rolling code specific attacks including:
 * - Rolling code capture
 * - Rolling code prediction
 * - Rolling code replay
 */

#pragma once

#include "../../predator_i.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Start rolling code attack (capture + replay)
 * @param app Application context
 * @param frequency Target frequency in Hz
 * @return true if started successfully, false otherwise
 */
bool predator_subghz_start_rolling_code_attack(PredatorApp* app, uint32_t frequency);

/**
 * @brief Stop rolling code attack
 * @param app Application context
 */
void predator_subghz_stop_rolling_code_attack(PredatorApp* app);

/**
 * @brief Tick function for rolling code attack state machine
 * @param app Application context
 * @note Call this periodically to advance attack state
 */
void predator_subghz_rolling_code_attack_tick(PredatorApp* app);

/**
 * @brief Send rolling code attack (crypto engine wrapper)
 * @param app Application context
 * @param frequency Target frequency
 */
void predator_subghz_send_rolling_code_attack(PredatorApp* app, uint32_t frequency);

/**
 * @brief Start passive car opener mode (rolling code sniffer)
 * @param app Application context
 */
void predator_subghz_start_passive_car_opener(PredatorApp* app);

/**
 * @brief Stop passive car opener mode
 * @param app Application context
 */
void predator_subghz_stop_passive_car_opener(PredatorApp* app);

/**
 * @brief Tick function for passive car opener
 * @param app Application context
 * @note Call this periodically to check for captured signals
 */
void predator_subghz_passive_car_opener_tick(PredatorApp* app);
