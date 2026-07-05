#include "predator_error.h"
#include <furi.h>
#include <notification/notification_messages.h>

// Implementation of popup callback function
static void popup_callback_ok(void* context) {
    furi_assert(context);
    PredatorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PredatorCustomEventPopupBack);
}

#define ERROR_NOTIFY_TIMEOUT 2000 // 2 seconds

void predator_error_init(PredatorApp* app) {
    app->has_error = false;
    app->last_error = PredatorErrorNone;
    app->error_timestamp = 0;
    memset(app->error_message, 0, sizeof(app->error_message));
}

bool predator_error_handle(
    PredatorApp* app, 
    PredatorErrorType error_type, 
    const char* message, 
    bool show_notification) {
    
    // Don't handle if app is NULL
    if(!app) return false;
    
    // Record error state
    app->has_error = true;
    app->last_error = error_type;
    app->error_timestamp = furi_get_tick();
    
    // Copy message with bounds checking
    strlcpy(app->error_message, message, sizeof(app->error_message));
    
    // Log error
    FURI_LOG_E("PredatorError", "Error %d: %s", error_type, message);
    
    // Show visual notification if requested
    if(show_notification && app->notifications) {
        notification_message(app->notifications, &sequence_error);
    }
    
    // Signal error event
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        PredatorCustomEventError);
    
    return true;
}

void predator_error_show_popup(PredatorApp* app, const char* title, const char* message) {
    // Skip if popup not available
    if(!app || !app->popup) return;
    
    // Setup error popup with friendly message
    popup_set_header(app->popup, title, 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, message, 64, 32, AlignCenter, AlignTop);
    popup_set_context(app->popup, app);
    
    // Add OK button for acknowledgement
    popup_set_callback(app->popup, popup_callback_ok);
    popup_set_timeout(app->popup, 0);
    popup_disable_timeout(app->popup);
    
    // Show popup
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewPopup);
}

void predator_error_clear(PredatorApp* app) {
    if(!app) return;
    
    app->has_error = false;
    app->last_error = PredatorErrorNone;
    memset(app->error_message, 0, sizeof(app->error_message));
}

bool predator_error_is_active(PredatorApp* app) {
    if(!app) return false;
    return app->has_error;
}

const char* predator_error_get_message(PredatorApp* app) {
    if(!app || !app->has_error) return "No error";
    return app->error_message;
}

void predator_log_error(PredatorApp* app, const char* tag, const char* message) {
    if(!app || !tag || !message) {
        FURI_LOG_E("PredatorError", "Invalid parameters for logging error");
        return;
    }
    
    FURI_LOG_E(tag, "%s", message);
    
    // Store the error in a log buffer if implemented - Comment out to avoid build errors
    // if(app->error_log) {
    //     // Assuming a simple circular buffer or similar structure for error logging
    //     // This is a placeholder for actual implementation
    //     // predator_add_to_log(app->error_log, tag, message);
    // }
}

void predator_log_warning(PredatorApp* app, const char* tag, const char* message) {
    if(!app || !tag || !message) {
        FURI_LOG_W("PredatorWarning", "Invalid parameters for logging warning");
        return;
    }
    
    FURI_LOG_W(tag, "%s", message);
}

void predator_log_info(PredatorApp* app, const char* tag, const char* message) {
    if(!app || !tag || !message) {
        FURI_LOG_I("PredatorInfo", "Invalid parameters for logging info");
        return;
    }
    
    FURI_LOG_I(tag, "%s", message);
}

bool predator_handle_error(PredatorApp* app, bool condition, const char* tag, const char* error_message) {
    if(!condition) {
        predator_log_error(app, tag, error_message);
        // Additional error handling logic can be added here
        return false;
    }
    return true;
}
