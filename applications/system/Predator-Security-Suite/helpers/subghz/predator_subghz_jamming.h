/**
 * @file predator_subghz_jamming.h
 * @brief SubGHz Jamming and Interference Attacks
 * 
 * This module handles RF jamming operations including:
 * - Continuous carrier jamming
 * - Parking barrier attacks
 * - General RF interference
 */

#pragma once

#include "../../predator_i.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Start SubGHz jamming attack
 * @param app Application context
 * @param frequency Target frequency to jam in Hz
 * @return true if started successfully, false otherwise
 */
bool predator_subghz_start_jamming(PredatorApp* app, uint32_t frequency);

/**
 * @brief Start parking barrier attack
 * @param app Application context
 * @param frequency Target frequency (typically 433.92MHz or 868MHz)
 * @param barrier_type Type of barrier system (0-5)
 * @return true if started successfully, false otherwise
 */
bool predator_subghz_start_parking_attack(PredatorApp* app, uint32_t frequency, uint8_t barrier_type);

/**
 * @brief Send jamming attack (crypto engine wrapper)
 * @param app Application context
 * @param frequency Target frequency
 */
void predator_subghz_send_jamming_attack(PredatorApp* app, uint32_t frequency);
