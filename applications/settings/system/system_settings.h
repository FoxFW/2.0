#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_version.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <storage/storage.h>

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    VariableItemList* var_item_list;
    TextInput* text_input;
    Storage* storage;
    char device_name[FURI_HAL_VERSION_ARRAY_NAME_LENGTH];
} SystemSettings;

typedef enum {
    SystemSettingsViewVarItemList,
    SystemSettingsViewTextInput,
} SystemSettingsView;
