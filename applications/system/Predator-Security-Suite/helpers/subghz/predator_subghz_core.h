/**
 * @file predator_subghz_core.h
 * @brief SubGHz Hardware Core - Initialization and Hardware Management
 * 
 * This module handles SubGHz hardware initialization, deinitialization,
 * and low-level hardware acquisition for all supported boards.
 */

#pragma once

#include "../../predator_i.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize SubGHz hardware for attack operations
 * @param app Application context
 * @note Automatically handles board-specific configuration
 * @note Suppresses charging during SubGHz operations
 */
void predator_subghz_init(PredatorApp* app);

/**
 * @brief Deinitialize SubGHz hardware and restore power state
 * @param app Application context
 */
void predator_subghz_deinit(PredatorApp* app);

/**
 * @brief Stop any running SubGHz attack
 * @param app Application context
 * @return true if stopped successfully, false otherwise
 */
bool predator_subghz_stop_attack(PredatorApp* app);

/**
 * @brief Send raw encrypted packet via SubGHz
 * @param app Application context
 * @param packet Packet data to transmit
 * @param len Packet length in bytes
 * @return true if transmitted successfully, false otherwise
 * @note Used by crypto engine for Keeloq/Hitag2 packets
 */
bool predator_subghz_send_raw_packet(PredatorApp* app, uint8_t* packet, size_t len);
