/**
 * @file predator_scene_car_core.h
 * @brief Car Core Navigation - Continent and Model Selection
 * 
 * This module handles the core navigation flow for car attacks:
 * 1. Continent selection (Europe/Asia/America)
 * 2. Model selection (filtered by continent)
 * 
 * These scenes are combined as they're tightly coupled navigation flows.
 * 
 * @note v2.0 Refactored: Modular car scenes architecture
 */

#pragma once

#include "predator_scene_car_shared.h"

// ============================================================================
// CONTINENT SELECTION SCENE
// ============================================================================

/**
 * @brief Enter continent selection scene
 * @param context Application context
 */
void predator_scene_car_continent_ui_on_enter(void* context);

/**
 * @brief Handle continent selection events
 * @param context Application context
 * @param event Scene manager event
 * @return true if event was consumed
 */
bool predator_scene_car_continent_ui_on_event(void* context, SceneManagerEvent event);

/**
 * @brief Exit continent selection scene
 * @param context Application context
 */
void predator_scene_car_continent_ui_on_exit(void* context);

// ============================================================================
// CAR MODELS SELECTION SCENE
// ============================================================================

/**
 * @brief Enter car models selection scene
 * @param context Application context
 * @note Models are filtered by previously selected continent
 */
void predator_scene_car_models_ui_on_enter(void* context);

/**
 * @brief Handle car models selection events
 * @param context Application context
 * @param event Scene manager event
 * @return true if event was consumed
 */
bool predator_scene_car_models_ui_on_event(void* context, SceneManagerEvent event);

/**
 * @brief Exit car models selection scene
 * @param context Application context
 */
void predator_scene_car_models_ui_on_exit(void* context);
