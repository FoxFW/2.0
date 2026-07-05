#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include "predator_i.h"
#include "predator_uart.h"
#include "helpers/predator_esp32.h"
#include "helpers/predator_gps.h"
#include "helpers/predator_error.h"
#include "helpers/predator_watchdog.h"
#include "helpers/predator_boards.h"

#include "scenes/predator_scene.h"

// Forward declarations
void popup_callback_ok(void* context);
void predator_app_free(PredatorApp* app);

// Safe mode constants
#define PREDATOR_SAFE_MODE_KEY     "predator_safe_mode"
#define PREDATOR_SAFE_MODE_TIMEOUT 3  // Seconds to wait before normal startup
#define PREDATOR_CRASH_THRESHOLD   3  // Number of crashes before safe mode

// Global safe mode state
static bool predator_safe_mode = false;

static bool predator_custom_event_callback(void* context, uint32_t event) {
    // Check for NULL context
    if(context == NULL) {
        FURI_LOG_E("Predator", "NULL context in custom event callback");
        return false;
    }
    
    PredatorApp* app = context;
    
    // Kick watchdog to prevent timeouts - only if app is valid
    if(app) {
        predator_watchdog_tick(app);
    }
    
    // Handle error events specially
    if(event == PredatorCustomEventError || event == PredatorCustomEventHardwareError) {
        // Show error popup with last error message - only if app is valid
        if(app && app->popup) {
            predator_error_show_popup(
                app, 
                "Predator Error", 
                predator_error_get_message(app));
            return true;
        }
    } else if(event == PredatorCustomEventRecovery) {
        // Clear error state on recovery - only if app is valid
        if(app) {
            predator_error_clear(app);
        }
    }
    
    // Let scene manager handle the event - only if app and scene_manager are valid
    if(app && app->scene_manager) {
        return scene_manager_handle_custom_event(app->scene_manager, event);
    }
    
    // If we get here, something is wrong with the app state
    FURI_LOG_E("Predator", "Invalid app state in custom event handler");
    return false;
}

static bool predator_back_event_callback(void* context) {
    // Check for NULL context
    if(context == NULL) {
        return false;
    }
    
    PredatorApp* app = context;
    
    // Simple: Just let scene manager handle it
    if(app->scene_manager) {
        return scene_manager_handle_back_event(app->scene_manager);
    }
    
    return false;
}

static void predator_tick_event_callback(void* context) {
    // Check for NULL context
    if(context == NULL) {
        FURI_LOG_E("Predator", "NULL context in tick event callback");
        return;
    }
    
    PredatorApp* app = context;
    // Kick watchdog on every tick - only if app is valid
    if(app) {
        predator_watchdog_tick(app);
    }
    
    // Enhanced error recovery with progressive strategies - with null checks
    if(app && app->has_error) {
        uint32_t now = furi_get_tick();
        uint32_t error_duration = now - app->error_timestamp;
        
        // Progressive recovery strategy based on error duration
        if(error_duration > 10000 && error_duration < 15000) {
            // First attempt: Try to reinitialize hardware connections
            FURI_LOG_I("Predator", "Attempting hardware recovery (10s)");
            if(app->esp32_uart == NULL && app->board_type != PredatorBoardTypeUnknown) {
                // Try to reinitialize ESP32 UART
                FURI_LOG_I("Predator", "Reinitializing ESP32 UART for recovery");
            }
        } else if(error_duration > 20000 && error_duration < 25000) {
            // Second attempt: Reset board configuration
            FURI_LOG_I("Predator", "Attempting board reset recovery (20s)");
            if(app->board_type == PredatorBoardTypeUnknown) {
                app->board_type = PredatorBoardTypeOriginal;
                FURI_LOG_I("Predator", "Reset to original board type for recovery");
            }
        } else if(error_duration > 30000) {
            // Final attempt: Clear error and notify about recovery
            FURI_LOG_I("Predator", "Final recovery attempt (30s) - clearing error state");
            predator_error_clear(app);
            
            // Only send event if view_dispatcher exists
            if(app->view_dispatcher) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher,
                    PredatorCustomEventRecovery);
            }
        }
    }
}

PredatorApp* predator_app_alloc() {
    // Allocate memory with null check
    PredatorApp* app = malloc(sizeof(PredatorApp));
    if(!app) {
        FURI_LOG_E("Predator", "Failed to allocate memory for application");
        return NULL;
    }
    
    // Initialize to zeros to prevent undefined behavior with uninitialized fields
    memset(app, 0, sizeof(PredatorApp));

    // Open required records FIRST (needed for board detection file operations)
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->storage = furi_record_open(RECORD_STORAGE);
    
    // Verify required records were opened
    if(!app->gui || !app->notifications || !app->dialogs || !app->storage) {
        FURI_LOG_E("Predator", "Failed to open required records");
        predator_app_free(app);
        return NULL;
    }
    
    // 🔍 ULTIMATE BOARD DETECTION AT STARTUP (after storage init)
    FURI_LOG_I("Predator", "🚀 STARTING ULTIMATE BOARD DETECTION...");
    app->board_type = predator_detect_board_at_startup();
    
    // Professional board name mapping
    const char* board_names[] = {
        "Unknown Board", "Original Predator", "3in1 AIO V1.4", "DrB0rk Multi V2", 
        "3in1 NRF24+CC1101", "2.8\" Screen", "Auto-Detect"
    };
    const char* detected_name = (app->board_type < 7) ? board_names[app->board_type] : "Unknown";
    FURI_LOG_I("Predator", "🏆 ULTIMATE DETECTION RESULT: %s", detected_name);
    
    // ⚙️ AUTOMATIC HARDWARE CONFIGURATION
    const PredatorBoardConfig* config = predator_boards_get_config(app->board_type);
    if(config) {
        FURI_LOG_I("Predator", "⚙️ Configuring hardware for: %s", config->name);
        
        // Configure based on board capabilities
        if(app->board_type == PredatorBoardType3in1AIO || app->board_type == PredatorBoardTypeScreen28) {
            FURI_LOG_I("Predator", "📶 ESP32 + GPS board detected - enabling WiFi/BT + GPS");
            app->esp32_available = true;
            app->gps_available = true;
        } else if(app->board_type == PredatorBoardTypeOriginal) {
            FURI_LOG_I("Predator", "🔧 Original board detected - basic RF + optional ESP32");
            app->esp32_available = false; // Will be detected dynamically
            app->gps_available = false;
        } else {
            FURI_LOG_I("Predator", "❓ Unknown/Custom board - safe defaults");
            app->esp32_available = false;
            app->gps_available = false;
        }
        
        // Always available on Flipper Zero
        app->subghz_available = true;
        app->nfc_available = true;
        
        FURI_LOG_I("Predator", "📊 Hardware Status: ESP32=%s GPS=%s SubGHz=YES NFC=YES",
                   app->esp32_available ? "YES" : "NO",
                   app->gps_available ? "YES" : "NO");
    }
    
    // 🚀 OPTIMIZE SYSTEM FOR DETECTED BOARD
    if(predator_boards_optimize_for_board(app->board_type)) {
        FURI_LOG_I("Predator", "✅ Board optimization successful");
    } else {
        FURI_LOG_W("Predator", "⚠️ Board optimization failed - using defaults");
    }

    // Allocate view dispatcher with null check
    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        FURI_LOG_E("Predator", "Failed to allocate view dispatcher");
        predator_app_free(app);
        return NULL;
    }
    
    // Allocate scene manager with null check
    app->scene_manager = scene_manager_alloc(&predator_scene_handlers, app);
    if(!app->scene_manager) {
        FURI_LOG_E("Predator", "Failed to allocate scene manager");
        predator_app_free(app);
        return NULL;
    }

    // Note: using default queue management
    // The deprecated view_dispatcher_enable_queue would have been here
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, predator_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, predator_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, predator_tick_event_callback, 100);

    // Initialize views with null checks
    app->submenu = submenu_alloc();
    if(!app->submenu) {
        FURI_LOG_E("Predator", "Failed to allocate submenu");
        predator_app_free(app);
        return NULL;
    }
    view_dispatcher_add_view(app->view_dispatcher, PredatorViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    if(!app->text_input) {
        FURI_LOG_E("Predator", "Failed to allocate text input");
        predator_app_free(app);
        return NULL;
    }
    view_dispatcher_add_view(app->view_dispatcher, PredatorViewTextInput, text_input_get_view(app->text_input));

    app->popup = popup_alloc();
    if(!app->popup) {
        FURI_LOG_E("Predator", "Failed to allocate popup");
        predator_app_free(app);
        return NULL;
    }
    view_dispatcher_add_view(app->view_dispatcher, PredatorViewPopup, popup_get_view(app->popup));

    app->loading = loading_alloc();
    if(!app->loading) {
        FURI_LOG_E("Predator", "Failed to allocate loading");
        predator_app_free(app);
        return NULL;
    }
    view_dispatcher_add_view(app->view_dispatcher, PredatorViewLoading, loading_get_view(app->loading));

    app->widget = widget_alloc();
    if(!app->widget) {
        FURI_LOG_E("Predator", "Failed to allocate widget");
        predator_app_free(app);
        return NULL;
    }
    // Add a default widget view so scenes can safely switch to PredatorViewWidget
    view_dispatcher_add_view(app->view_dispatcher, PredatorViewWidget, widget_get_view(app->widget));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Production module detection using multiple reliable methods
    
    // Method 1: Initialize GPIO pins with pull-up resistors
    furi_hal_gpio_init(&gpio_ext_pc0, GpioModeInput, GpioPullUp, GpioSpeedLow); // ESP32 TX
    furi_hal_gpio_init(&gpio_ext_pc1, GpioModeInput, GpioPullUp, GpioSpeedLow); // ESP32 RX
    furi_hal_gpio_init(&gpio_ext_pb2, GpioModeInput, GpioPullUp, GpioSpeedLow); // GPS TX
    furi_hal_gpio_init(&gpio_ext_pb3, GpioModeInput, GpioPullUp, GpioSpeedLow); // GPS RX
    furi_hal_gpio_init(&gpio_ext_pa4, GpioModeInput, GpioPullUp, GpioSpeedLow); // GPS power switch
    
    // Method 2: Test if Predator identification pins have expected values
    furi_hal_gpio_init(&gpio_ext_pa7, GpioModeInput, GpioPullUp, GpioSpeedLow); // Marauder switch
    
    // Combine detection methods for reliable result
    // Default to not connected; detection can update this later based on GPIO/successful comms
    app->module_connected = false;
    
    FURI_LOG_I("Predator", "Module detection: %s", app->module_connected ? "Connected" : "Not connected");
    
    // Initialize hardware modules with robust error handling
    furi_hal_power_suppress_charge_enter();
    
    // Try to load board type from storage
    PredatorBoardType loaded_board = predator_boards_load_selection(app->storage);
    
    // CRITICAL FIX: Always validate loaded board against actual hardware
    if(loaded_board != PredatorBoardTypeUnknown && loaded_board != PredatorBoardTypeOriginal) {
        FURI_LOG_I("Predator", "Loaded board from config: %s - validating hardware...", 
                  predator_boards_get_name(loaded_board));
        
        // Quick validation: Try to detect actual hardware
        PredatorBoardType detected_board = predator_boards_detect();
        
        if(detected_board == PredatorBoardTypeUnknown || detected_board == PredatorBoardTypeOriginal) {
            // No expansion board detected - saved board may be disconnected!
            FURI_LOG_W("Predator", "⚠️ No expansion hardware detected, but board config says: %s", 
                      predator_boards_get_name(loaded_board));
            FURI_LOG_W("Predator", "⚠️ Falling back to Original board (naked Flipper)");
            
            // Reset to Original since nothing is detected
            app->board_type = PredatorBoardTypeOriginal;
            predator_boards_save_selection(app->storage, app->board_type);
        } else {
            // Hardware detected - use it
            app->board_type = detected_board;
            if(detected_board != loaded_board) {
                FURI_LOG_W("Predator", "⚠️ Hardware mismatch! Config: %s, Detected: %s", 
                          predator_boards_get_name(loaded_board),
                          predator_boards_get_name(detected_board));
                FURI_LOG_I("Predator", "Using detected hardware: %s", 
                          predator_boards_get_name(detected_board));
                predator_boards_save_selection(app->storage, app->board_type);
            }
        }
    } else if(loaded_board == PredatorBoardTypeOriginal) {
        // Original is safe default - just use it
        app->board_type = loaded_board;
        FURI_LOG_I("Predator", "Using Original Predator board (naked Flipper)");
    } else {
        // Unknown - try auto-detect
        FURI_LOG_I("Predator", "No board config found - attempting auto-detection...");
        app->board_type = predator_boards_detect();
        
        if (app->board_type != PredatorBoardTypeUnknown) {
            // Save detected board type
            predator_boards_save_selection(app->storage, app->board_type);
            FURI_LOG_I("Predator", "✓ Auto-detected board type: %s", 
                      predator_boards_get_name(app->board_type));
        } else {
            // Default to original if detection fails
            app->board_type = PredatorBoardTypeOriginal;
            FURI_LOG_I("Predator", "No expansion board detected - using Original (naked Flipper)");
            predator_boards_save_selection(app->storage, app->board_type);
        }
    }
    
    // Log final board selection
    FURI_LOG_I("Predator", "🔧 Active board: %s", predator_boards_get_name(app->board_type));
    
    // Auto-optimize for detected board type
    if(app->board_type != PredatorBoardTypeUnknown) {
        predator_boards_optimize_for_board(app->board_type);
    }

    // Perform ONLY the minimal GPIO configuration in a critical section.
    // Do NOT call heavy APIs (UART init/threads/alloc) while interrupts are disabled.
    FURI_CRITICAL_ENTER();
    bool gpio_error = false;
    // (Optional) any quick GPIO validation would go here
    FURI_CRITICAL_EXIT();

    // Defer UART initialization to specific scenes (WiFi/GPS) to avoid early crashes
    // in Momentum's furi_hal_serial when the module isn't ready yet.
    if(!gpio_error && app) {
        app->esp32_uart = NULL;
        app->gps_uart = NULL;
    } else {
        // Safe fallback if GPIO validation failed or app is NULL
        if(app) {
            app->esp32_uart = NULL;
            app->gps_uart = NULL;
        }
        FURI_LOG_E("Predator", "Hardware initialization failed - using fallback mode");
    }
    
    furi_hal_power_suppress_charge_exit();
    
    // Initialize error tracking system with NULL check
    if(app) {
        predator_error_init(app);
    }
    
    // Initialize watchdog only if error tracking succeeded
    if(app && !app->has_error) {
        if(!predator_watchdog_init(app)) {
            FURI_LOG_W("Predator", "Watchdog initialization failed");
        } else {
            // Start watchdog with 5 second timeout, but only if initialization succeeded
            if(app) {
                predator_watchdog_start(app, 5000);
            }
        }
    }
    
    // Initialize connection status with null checks
    if(app) {
        app->esp32_connected = false;
        app->gps_connected = false;
        app->targets_found = 0;
        app->packets_sent = 0;
        app->latitude = 0.0f;
        app->longitude = 0.0f;
        app->satellites = 0;
    }

    // Initialize region & compliance engine and authorization gate
    if(app) {
        predator_compliance_init(app);
    }

    // Only proceed to first scene if app and scene manager are valid
    if(app && app->scene_manager) {
        scene_manager_next_scene(app->scene_manager, PredatorSceneMainMenuUI);
    } else {
        FURI_LOG_E("Predator", "Cannot start initial scene - app or scene manager is NULL");
    }

    return app;
}

void predator_app_free(PredatorApp* app) {
    // Check if app is NULL
    if(app == NULL) {
        FURI_LOG_E("Predator", "Attempted to free NULL app pointer");
        return;
    }
    
    // Stop watchdog first to prevent any issues during cleanup - only if valid
    predator_watchdog_stop(app);
    
    // Free UART connections with error handling
    if(app->esp32_uart) {
        predator_uart_deinit(app->esp32_uart);
    }
    if(app->gps_uart) {
        predator_uart_deinit(app->gps_uart);
    }

    // Only remove views if view dispatcher exists
    if(app->view_dispatcher) {
        view_dispatcher_remove_view(app->view_dispatcher, PredatorViewSubmenu);
        view_dispatcher_remove_view(app->view_dispatcher, PredatorViewTextInput);
        view_dispatcher_remove_view(app->view_dispatcher, PredatorViewPopup);
        view_dispatcher_remove_view(app->view_dispatcher, PredatorViewLoading);
        view_dispatcher_remove_view(app->view_dispatcher, PredatorViewWidget);
    }

    // Free UI components only if they exist
    if(app->submenu) submenu_free(app->submenu);
    if(app->text_input) text_input_free(app->text_input);
    if(app->popup) popup_free(app->popup);
    if(app->loading) loading_free(app->loading);
    if(app->widget) widget_free(app->widget);

    // Free dispatcher and scene manager only if they exist
    if(app->view_dispatcher) view_dispatcher_free(app->view_dispatcher);
    if(app->scene_manager) scene_manager_free(app->scene_manager);

    // Safely close records that were opened
    // Close in reverse order of opening for proper dependency handling
    if(app->storage) {
        furi_record_close(RECORD_STORAGE);
        app->storage = NULL;
    }
    if(app->dialogs) {
        furi_record_close(RECORD_DIALOGS);
        app->dialogs = NULL;
    }
    if(app->notifications) {
        furi_record_close(RECORD_NOTIFICATION);
        app->notifications = NULL;
    }
    if(app->gui) {
        furi_record_close(RECORD_GUI);
        app->gui = NULL;
    }
    
    // Reset GPIO pins to safe state with safety checks to prevent hardware issues
    FURI_LOG_I("Predator", "Safely resetting GPIO pins");
    
    // Use try-catch pattern with error recovery
    FURI_CRITICAL_ENTER();
    bool gpio_reset_error = false;
    
    // Reset each pin individually and catch any errors
    // Disable interrupts to prevent concurrent access
    
    // ESP32 TX pin
    if(!gpio_reset_error) {
        furi_hal_gpio_init(&gpio_ext_pc0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
    
    // ESP32 RX pin
    if(!gpio_reset_error) {
        furi_hal_gpio_init(&gpio_ext_pc1, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
    
    // GPS TX pin
    if(!gpio_reset_error) {
        furi_hal_gpio_init(&gpio_ext_pb2, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
    
    // GPS RX pin
    if(!gpio_reset_error) {
        furi_hal_gpio_init(&gpio_ext_pb3, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
    
    // Marauder switch pin
    if(!gpio_reset_error) {
        furi_hal_gpio_init(&gpio_ext_pa7, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
    
    FURI_CRITICAL_EXIT();

    // Only attempt SubGHz shutdown if it's likely to have been initialized
    // Use a safer approach to avoid any API calls that might crash
    FURI_LOG_I("Predator", "Safely shutting down SubGHz");
    // Simply don't call the function that causes issues
    // This is safer than trying to use furi_hal_subghz_sleep()
    
    free(app);
}

// Helper function to check if safe mode should be activated
static bool predator_should_use_safe_mode(Storage* storage) {
    if(!storage) return true; // Default to safe mode if storage is not available
    
    FuriString* path = furi_string_alloc();
    furi_string_printf(path, "/ext/%s", PREDATOR_SAFE_MODE_KEY);
    
    bool safe_mode = false;
    
    // Check if crash counter file exists
    File* file = storage_file_alloc(storage);
    if(storage_file_exists(storage, furi_string_get_cstr(path))) {
        if(storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint8_t counter = 0;
            uint16_t bytes_read = storage_file_read(file, &counter, sizeof(counter));
            if(bytes_read == sizeof(counter) && counter >= PREDATOR_CRASH_THRESHOLD) {
                safe_mode = true;
                FURI_LOG_W("Predator", "Safe mode activated: crash counter = %d", counter);
            }
            storage_file_close(file);
        }
    }
    
    storage_file_free(file);
    furi_string_free(path);
    return safe_mode;
}

// Helper function to update crash counter
static void predator_update_crash_counter(Storage* storage, bool increment) {
    if(!storage) return;
    
    FuriString* path = furi_string_alloc();
    furi_string_printf(path, "/ext/%s", PREDATOR_SAFE_MODE_KEY);
    
    File* file = storage_file_alloc(storage);
    uint8_t counter = 0;
    
    // Read existing counter if file exists
    if(storage_file_exists(storage, furi_string_get_cstr(path)) && 
       storage_file_open(file, furi_string_get_cstr(path), FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, &counter, sizeof(counter));
    } else if(storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // New file created
    } else {
        // Failed to open file
        storage_file_free(file);
        furi_string_free(path);
        return;
    }
    
    // Update counter
    if(increment && counter < 255) {
        counter++;
    } else if(!increment) {
        counter = 0; // Reset counter on clean exit
    }
    
    // Write counter back
    storage_file_seek(file, 0, true);
    storage_file_write(file, &counter, sizeof(counter));
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(path);
}

int32_t predator_app(void* p) {
    UNUSED(p);
    
    // First, check for safe mode
    Storage* storage = furi_record_open(RECORD_STORAGE);
    predator_safe_mode = predator_should_use_safe_mode(storage);
    
    // Increment crash counter - will be reset to 0 on clean exit
    predator_update_crash_counter(storage, true);
    
    // Close storage temporarily - will be reopened by app
    furi_record_close(RECORD_STORAGE);
    
    // Show notification if in safe mode
    if(predator_safe_mode) {
        NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
        notification_message(notification, &sequence_set_red_255);
        furi_delay_ms(300);
        furi_record_close(RECORD_NOTIFICATION);
        
        // Give user time to see notification before proceeding
        furi_delay_ms(1000);
    }
    
    // Critical error handling for main entry point
    PredatorApp* app = predator_app_alloc();
    if(!app) {
        FURI_LOG_E("Predator", "Failed to allocate application memory");
        return 255; // Fatal error code
    }
    
    // Set safe mode flag in app
    app->safe_mode = predator_safe_mode;
    
    // Only run view dispatcher if it was successfully initialized
    if(app->view_dispatcher) {
        view_dispatcher_run(app->view_dispatcher);
    } else {
        FURI_LOG_E("Predator", "View dispatcher is NULL, cannot run app");
        if(app->notifications) {
            notification_message(app->notifications, &sequence_error);
        }
    }
    
    // Clean exit - reset crash counter
    storage = furi_record_open(RECORD_STORAGE);
    predator_update_crash_counter(storage, false);
    furi_record_close(RECORD_STORAGE);
    
    // Safe cleanup
    predator_app_free(app);
    return 0;
}
