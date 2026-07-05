#pragma once

#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/popup.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_input.h>

#include <desktop/desktop.h>
#include <desktop/helpers/pin_code.h> // Brings back the true definition for DesktopPinCode!
#include <dialogs/dialogs.h>

#include "views/desktop_settings_view_pin_setup_howto.h"
#include "views/desktop_settings_view_pin_setup_howto2.h"
#include "views/desktop_settings_view_numeric_pin.h"

// Clean import to inherit all dynamically configured scenes and views without duplicate redeclarations
#include "scenes/desktop_settings_scene.h"

// ==========================================
// DESKTOP SERVICE COMPATIBILITY MOCK BLOCKS
// ==========================================
// We do NOT mock desktop_pin_code_set() anymore so it successfully writes the PIN to flash storage!
#define desktop_pin_lock_error_notify()   (void)0

// Route desktop_set_pin through the proper service API.
// desktop_api_set_pin is declared in <desktop/desktop.h> (already included above).
// It updates both in-memory PIN state and persists to flash — no reboot needed.
#define desktop_set_pin(service, pin_code) desktop_api_set_pin(service, pin_code)

// Fix for FreeRTOS scheduler tracking variable
#define uxTopUsedPriority 0

typedef struct {
    Gui* gui;
    DialogsApp* dialogs;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    
    Popup* popup;
    Submenu* submenu;
    VariableItemList* variable_item_list;
    
    DesktopSettingsViewPinSetupHowto* pin_setup_howto_view;
    void* pin_setup_howto2_view;
    DesktopSettingsViewNumericPin* numeric_pin_view;
    DialogEx* dialog_ex;
    TextInput* text_input;

    char device_name[64];
    bool save_name;
    uint32_t pin_menu_idx;
    DesktopSettings settings;
    DesktopPinCode pincode_buffer;
} DesktopSettingsApp;