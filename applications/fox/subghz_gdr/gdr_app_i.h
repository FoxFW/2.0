// gdr_app_i.h
#pragma once

#include <stddef.h>
#include "helpers/gdr_types.h"
#include "helpers/gdr_settings.h"
#include "scenes/gdr_scene.h"
#include "views/gdr_receiver.h"
#include "gdr_history.h"
#include "helpers/radio_device_loader.h"
#ifdef ENABLE_DUAL_RX_SCENE
#include "helpers/gdr_rx_chain.h"
#include "views/gdr_dual_receiver.h"
#endif
#ifdef ENABLE_SHIELD_RX_SCENE
#include "helpers/gdr_rx_chain.h"
#include "helpers/gdr_tx_chain.h"
#endif

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/view_holder.h>
#include <gui/modules/loading.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <notification/notification_messages.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/subghz_file_encoder_worker.h>
#include <lib/flipper_application/plugins/plugin_manager.h>
#include <lib/flipper_application/plugins/composite_resolver.h>
#include <dialogs/dialogs.h>
#include "defines.h"
#include "protocols/protocols_common.h"
#include "protocols/protocol_items.h"
#include "protocols/gdr_protocol_plugins.h"
#ifdef ENABLE_EMULATE_FEATURE
#include "scenes/plugins/gdr_emulate_plugin.h"
#endif
#include "scenes/plugins/gdr_psa_bf_plugin.h"

#define GDR_KEYSTORE_DIR_NAME APP_ASSETS_PATH("encrypted")

typedef struct GDRApp GDRApp;

typedef enum {
    GDRCaptureOwnerNone = 0,
    GDRCaptureOwnerReceiver,
    GDRCaptureOwnerDualReceiver,
#ifdef ENABLE_SHIELD_RX_SCENE
    GDRCaptureOwnerShieldReceiver,
#endif
    GDRCaptureOwnerSubDecode,
} GDRCaptureOwner;

typedef struct {
    GDRHistory* history;
    FuriMutex* mutex;
    uint16_t index;
    GDRCaptureOwner owner;
} GDRSelectedCapture;

typedef struct {
    SubGhzWorker* worker;
    SubGhzEnvironment* environment;
    SubGhzReceiver* receiver;
    SubGhzRadioPreset* preset;
    const SubGhzProtocolRegistry* protocol_registry;
    CompositeApiResolver* plugin_resolver;
    PluginManager* protocol_plugin_manager;
    const GDRProtocolPlugin* protocol_plugin;
    GDRProtocolRegistryFilter protocol_registry_filter;
    GDRHistory* history;
    const SubGhzDevice* radio_device;
    GDRTxRxState txrx_state;
    GDRHopperState hopper_state;
    GDRRxKeyState rx_key_state;
    uint8_t hopper_idx_frequency;
    uint8_t hopper_timeout;
    uint16_t idx_menu_chosen;
} GDRTxRx;

struct GDRApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    DialogsApp* dialogs;
    VariableItemList* variable_item_list;
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    View* view_about;
    FuriString* file_path;
    GDRReceiver* gdr_receiver;
    GDRTxRx* txrx;
    SubGhzSetting* setting;
    GDRLock lock;
    FuriString* loaded_file_path;
    bool auto_save;
    bool radio_initialized;

    /* Startup loading wheel — shown during gdr_app_alloc() to bridge the gap
     * between the loader's FAP-load spinner and GDR's first scene appearing.
     * Dismissed in gdr_scene_start_on_enter(). */
    ViewHolder* startup_holder;
    Loading*    startup_loading;
    GDRSettings settings;
    uint32_t start_tx_time;
    uint8_t tx_power;
    char save_filename[64];
    FuriString* save_protocol;
    uint16_t save_history_idx;
    bool save_from_saved_info;
    bool emulate_disabled_for_loaded;
    bool emulate_feature_enabled;
    GDRSelectedCapture selected_capture;
    GDRCaptureOwner unsaved_history_owner;
#ifdef ENABLE_EMULATE_FEATURE
#define EMULATE_NAV_NONE     0U
#define EMULATE_NAV_POP      1U
#define EMULATE_NAV_STOP_APP 2U
    CompositeApiResolver* emulate_plugin_resolver;
    PluginManager* emulate_plugin_manager;
    const GDREmulatePlugin* emulate_plugin;
    uint8_t emulate_nav_pending;
#endif
    CompositeApiResolver* psa_bf_plugin_resolver;
    PluginManager* psa_bf_plugin_manager;
    const GDRPsaBfPlugin* psa_bf_plugin;
#ifdef ENABLE_DUAL_RX_SCENE
    GDRDualReceiver* dual_receiver;
    GDRRxChain* dual_chain_a;
    GDRRxChain* dual_chain_b;
    GDRHistory* dual_history;
    FuriMutex* dual_history_mutex;
    uint32_t dual_freq_a;
    uint32_t dual_freq_b;
    uint8_t dual_preset_a;
    uint8_t dual_preset_b;
#endif
#ifdef ENABLE_SHIELD_RX_SCENE
    GDRRxChain* shield_rx_chain;
    GDRTxChain* shield_tx_chain;
    GDRHistory* shield_history;
    FuriMutex* shield_history_mutex;
    uint32_t shield_freq;
    uint8_t shield_preset_index;
    uint8_t shield_tx_offset_index;
    uint8_t shield_tx_power;
    bool shield_auto_save_failed;
#endif
};

#ifdef ENABLE_EMULATE_FEATURE
void gdr_emulate_context_release(GDRApp* app);
#endif

typedef enum {
    GDRSetTypeFord_v0,
    GDRSetTypeMAX,
} GDRSetType;

void gdr_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size);

void gdr_get_frequency_modulation(
    GDRApp* app,
    FuriString* frequency,
    FuriString* modulation);
void gdr_get_frequency_modulation_str(
    GDRApp* app,
    char* frequency,
    size_t frequency_size,
    char* modulation,
    size_t modulation_size);

void gdr_begin(GDRApp* app, uint8_t* preset_data);
uint32_t gdr_rx(GDRApp* app, uint32_t frequency);
void gdr_idle(GDRApp* app);
void gdr_rx_end(GDRApp* app);
void gdr_sleep(GDRApp* app);
void gdr_hopper_update(GDRApp* app);
void gdr_tx(GDRApp* app, uint32_t frequency);
void gdr_tx_stop(GDRApp* app);
bool gdr_radio_init(GDRApp* app);
void gdr_radio_deinit(GDRApp* app);
bool gdr_refresh_protocol_registry(GDRApp* app, bool ensure_receiver_ready);
bool gdr_apply_protocol_registry_for_preset_data(
    GDRApp* app,
    const uint8_t* preset_data,
    size_t preset_data_size);
bool gdr_ensure_variable_item_list(GDRApp* app);
bool gdr_ensure_widget(GDRApp* app);
bool gdr_ensure_text_input(GDRApp* app);
bool gdr_ensure_view_about(GDRApp* app);
bool gdr_ensure_receiver_view(GDRApp* app);
#ifdef ENABLE_DUAL_RX_SCENE
bool gdr_ensure_dual_receiver_view(GDRApp* app);
#endif
void gdr_release_shared_radio_state(GDRApp* app);

void gdr_rx_stack_suspend_for_tx(GDRApp* app);

void gdr_rx_stack_resume_after_tx(GDRApp* app);

void gdr_selected_capture_set(
    GDRApp* app,
    GDRHistory* history,
    FuriMutex* mutex,
    uint16_t index,
    GDRCaptureOwner owner);
void gdr_selected_capture_clear(GDRApp* app);
bool gdr_selected_capture_is_valid(GDRApp* app);
GDRHistory* gdr_selected_capture_get_history(GDRApp* app);
uint16_t gdr_selected_capture_get_index(GDRApp* app);
GDRHistorySource gdr_selected_capture_get_source(GDRApp* app);
FlipperFormat* gdr_selected_capture_get_raw_data(GDRApp* app);
bool gdr_selected_capture_get_path(GDRApp* app, FuriString* out_path);
void gdr_selected_capture_release_scratch(GDRApp* app);

void gdr_app_free(GDRApp* app);

static const NotificationSequence sequence_tx = {
    &message_note_c5,
    &message_vibro_on,
    &message_red_255,
    &message_blue_255,
    &message_blink_start_10,
    &message_delay_25,
    &message_vibro_off,
    &message_delay_25,
    &message_sound_off,
    NULL,
};
