#pragma once

#include "../predator_i.h"

/**
 * @brief Initialize error handling system
 * @param app Predator application context
 */
void predator_error_init(PredatorApp* app);

/**
 * @brief Handle and report an error
 * @param app Predator application context
 * @param error_type Type of error that occurred
 * @param message Error message to display
 * @param show_notification Whether to show a visual notification
 * @return true if error was handled, false otherwise
 */
bool predator_error_handle(
    PredatorApp* app, 
    PredatorErrorType error_type, 
    const char* message, 
    bool show_notification);

/**
 * @brief Show error popup with user-friendly message
 * @param app Predator application context
 * @param title Error title
 * @param message Error message
 */
void predator_error_show_popup(PredatorApp* app, const char* title, const char* message);

/**
 * @brief Clear current error state
 * @param app Predator application context
 */
void predator_error_clear(PredatorApp* app);

/**
 * @brief Check if app is currently in error state
 * @param app Predator application context
 * @return true if in error state
 */
bool predator_error_is_active(PredatorApp* app);

/**
 * @brief Get last error message
 * @param app Predator application context
 * @return Error message string
 */
const char* predator_error_get_message(PredatorApp* app);
