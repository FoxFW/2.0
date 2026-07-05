/**
 * @file predator_scene_car_tesla.h
 * @brief Tesla-Specific Security Testing
 * 
 * Handles Tesla charge port attacks and Tesla-specific security testing.
 * This module is kept separate due to Tesla's importance (Elon priority!)
 * and unique attack characteristics.
 * 
 * @note v2.0 Refactored: Modular car scenes architecture
 */

#pragma once

#include "predator_scene_car_shared.h"

/**
 * @brief Enter Tesla attack scene
 * @param context Application context
 */
void predator_scene_car_tesla_ui_on_enter(void* context);

/**
 * @brief Handle Tesla scene events
 * @param context Application context
 * @param event Scene manager event
 * @return true if event was consumed
 */
bool predator_scene_car_tesla_ui_on_event(void* context, SceneManagerEvent event);

/**
 * @brief Exit Tesla attack scene
 * @param context Application context
 */
void predator_scene_car_tesla_ui_on_exit(void* context);
