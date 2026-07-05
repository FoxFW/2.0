/**
 * @file predator_subghz_car.h
 * @brief SubGHz Car Attacks - Generic car key operations
 * 
 * This module handles car-specific SubGHz attacks including:
 * - Key bruteforce attacks
 * - Fixed code transmission
 * - Car command sending
 */

#pragma once

#include "../../predator_i.h"
#include <stdbool.h>
#include <stdint.h>

// Car model definitions
typedef enum {
    CarModelToyota,
    CarModelHonda,
    CarModelFord,
    CarModelChevrolet,
    CarModelBMW,
    CarModelMercedes,
    CarModelAudi,
    CarModelVolkswagen,
    CarModelNissan,
    CarModelHyundai,
    CarModelKia,
    CarModelTesla,
    CarModelSubaru,
    CarModelJeep,
    CarModelChrysler,
    CarModelDodge,
    CarModelCadillac,
    CarModelLexus,
    CarModelInfiniti,
    CarModelAcura,
    CarModelMazda,
    CarModelMitsubishi,
    CarModelPorsche,
    CarModelRangeRover,
    CarModelJaguar,
    CarModelVolvo,
    CarModelFiat,
    CarModelPeugeot,
    CarModelRenault,
    CarModelSkoda,
    CarModelLamborghini,
    CarModelFerrari,
    CarModelMaserati,
    CarModelBentley,
    CarModelRollsRoyce,
    CarModelCount
} CarModel;

// Car command definitions
typedef enum {
    CarCommandUnlock,
    CarCommandLock,
    CarCommandTrunk,
    CarCommandStart,
    CarCommandPanic,
    CarCommandCount
} CarCommand;

/**
 * @brief Start car key bruteforce attack
 * @param app Application context
 * @param frequency Target frequency in Hz (315MHz, 433MHz, 868MHz)
 * @return true if started successfully, false otherwise
 */
bool predator_subghz_start_car_bruteforce(PredatorApp* app, uint32_t frequency);

/**
 * @brief Send single car key code
 * @param app Application context
 * @param key_code Key code to transmit
 */
void predator_subghz_send_car_key(PredatorApp* app, uint32_t key_code);

/**
 * @brief Send specific command to car model
 * @param app Application context
 * @param model Target car model
 * @param command Command to send (unlock, lock, etc.)
 * @return true if sent successfully, false otherwise
 */
bool predator_subghz_send_car_command(PredatorApp* app, CarModel model, CarCommand command);

/**
 * @brief Get car model name as string
 * @param model Car model enum
 * @return Const string with car model name
 */
const char* predator_subghz_get_car_model_name(CarModel model);

/**
 * @brief Get car command name as string
 * @param command Car command enum
 * @return Const string with command name
 */
const char* predator_subghz_get_car_command_name(CarCommand command);

/**
 * @brief Send Tesla charge port open command
 * @param app Application context
 */
void predator_subghz_send_tesla_charge_port(PredatorApp* app);

/**
 * @brief Send car bruteforce attack (crypto engine wrapper)
 * @param app Application context
 * @param frequency Target frequency
 */
void predator_subghz_send_car_bruteforce(PredatorApp* app, uint32_t frequency);
