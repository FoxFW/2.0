#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view_stack.h>
#include <gui/scene_manager.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <gui/modules/loading.h>
#include <gui/modules/variable_item_list.h>

#include <subghz_randomattack_icons.h>

#include <dialogs/dialogs.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "subratt.h"
#include "subratt_device.h"
#include "subratt_settings.h"
#include "helpers/subratt_worker.h"
#include "views/subratt_attack_view.h"
#include "views/subratt_main_view.h"
#include "views/subratt_attack_mode_view.h"

#define SUB_RATT_VERSION "SubRATT 1.0"
#define SUB_RATT_APP_TITLE "Fox SubRATT"

#ifdef FURI_DEBUG

#endif

typedef enum {
    SubRattViewNone,
    SubRattViewMain,
    SubRattViewAttack,
    SubRattViewTextInput,
    SubRattViewDialogEx,
    SubRattViewPopup,
    SubRattViewWidget,
    SubRattViewStack,
    SubRattViewVarList,
    SubRattViewLoading,
    SubRattViewAttackMode,
} SubRattView;

struct SubRattState {

    NotificationApp* notifications;
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    ViewStack* view_stack;
    TextInput* text_input;
    Popup* popup;
    Widget* widget;
    VariableItemList* var_list;
    Loading* loading;
    DialogsApp* dialogs;

    char text_store[SUBRATT_MAX_LEN_NAME];
    FuriString* file_path;

    const SubGhzDevice* radio_device;

    SubRattMainView* view_main;
    SubRattAttackView* view_attack;
    SubRattAttackModeView* view_attack_mode;
    SubRattView current_view;

    SceneManager* scene_manager;

    SubRattDevice* device;
    SubRattWorker* worker;
    SubRattSettings* settings;
};

void subratt_show_loading_popup(void* context, bool show);

void subratt_text_input_callback(void* context);

void subratt_popup_closed_callback(void* context);
