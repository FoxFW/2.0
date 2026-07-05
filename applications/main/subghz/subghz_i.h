#pragma once

#include "helpers/subghz_types.h"
#include "helpers/subghz_gen_info.h"
#include <lib/subghz/types.h>
#include "subghz.h"
#include "views/receiver.h"
#include "views/transmitter.h"
#include "views/subghz_signal_visualizer.h"
#include "subghz_protocol_filter.h"
#include "subghz_modulation_filter.h"
#include "views/subghz_view_start_grid.h"
#include <gui/modules/loading.h>
#include <gui/view_holder.h>
#include "views/subghz_read_raw.h"
#include "views/subghz_psa_decrypt.h"
#include "views/subghz_keeloq_decrypt.h"

#include <gui/gui.h>
#include <gui/view_port.h>
#include <assets_icons.h>
#include <dialogs/dialogs.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/widget.h>

#include <subghz/scenes/subghz_scene.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/subghz_file_encoder_worker.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>

#include "subghz_history.h"
#include "subghz_last_settings.h"

#include <gui/modules/variable_item_list.h>
#include <lib/toolbox/path.h>

#include "rpc/rpc_app.h"

#include "helpers/subghz_threshold_rssi.h"

#include "helpers/subghz_txrx.h"
#include "helpers/subghz_keeloq_keys.h"

#define SUBGHZ_MAX_LEN_NAME      64
#define SUBGHZ_EXT_PRESET_NAME   true
#define SUBGHZ_RAW_THRESHOLD_MIN (-90.0f)
#define SUBGHZ_MEASURE_LOADING   false

struct SubGhz {
    Gui* gui;
    NotificationApp* notifications;

    SubGhzTxRx* txrx;

    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    /* A standalone ViewPort (NOT managed by view_dispatcher) that paints
     * solid black over the full screen, registered right before SubGHz
     * exits to launch a sub-tool FAP. view_dispatcher_stop() fully tears
     * down SubGHz's own GUI presence, briefly revealing the Desktop/Apps
     * menu underneath before the next app's view attaches — this gives
     * our OWN content something to show during that exact gap instead,
     * since it's independent of the ViewDispatcher being torn down.
     * Cleaned up in subghz_free(), right before the app's thread ends. */
    ViewPort* blank_transition_viewport;

    Submenu* submenu;
    Popup* popup;
    TextInput* text_input;
    ByteInput* byte_input;
    Widget* widget;
    DialogsApp* dialogs;
    FuriString* file_path;
    FuriString* file_path_tmp;

    /* Decoded-protocol "Send" preview: when a decoded file's Emulate
     * action is triggered, the protocol is synthesized into a temporary
     * RAW capture and played through the RAW player (waveform/bargraph,
     * pause, seek). These track that state so the ORIGINAL file can be
     * restored when the user backs out, and the temp file cleaned up. */
    bool        decoded_preview_active;
    FuriString* decoded_preview_orig_path;
    char file_name_tmp[SUBGHZ_MAX_LEN_NAME];
    SubGhzNotificationState state_notifications;

    SubGhzViewReceiver* subghz_receiver;
    SubGhzViewTransmitter* subghz_transmitter;
    VariableItemList* variable_item_list;

    SubGhzSignalVisualizer*       subghz_signal_visualizer;
    SubGhzProtocolFilter*         protocol_filter;
    SubGhzModulationFilter*        modulation_filter;
    SubGhzStartGrid*               start_grid;
    /* Startup loading wheel — shown immediately on launch, removed
     * when the start grid scene enters (hides apps menu + input). */
    Loading*                       startup_loading;
    ViewHolder*                    startup_holder;
    SubGhzReadRAW* subghz_read_raw;
    SubGhzViewPsaDecrypt* subghz_psa_decrypt;
    SubGhzViewKeeloqDecrypt* subghz_keeloq_decrypt;
    bool raw_send_only;

    bool save_datetime_set;
    DateTime save_datetime;

    SubGhzLastSettings* last_settings;

    SubGhzProtocolFlag filter;
    SubGhzProtocolFlag ignore_filter;
    FuriString* error_str;
    SubGhzLock lock;

    GenInfo* gen_info;

    SubGhzFileEncoderWorker* decode_raw_file_worker_encoder;

    SubGhzThresholdRssi* threshold_rssi;
    SubGhzRxKeyState rx_key_state;
    SubGhzHistory* history;

    uint16_t idx_menu_chosen;
    SubGhzLoadTypeFile load_type_file;
    uint8_t tx_power;
    void* rpc_ctx;

    // KeeLoq key management
    SubGhzKeeloqKeysManager* keeloq_keys_manager;
    struct {
        uint8_t key_bytes[8];
        char name[65];
        uint16_t type;
        bool is_new;
        size_t edit_index;
        uint8_t edit_step;
    } keeloq_edit;

    struct {
        uint32_t fix;
        uint32_t hop1;
        uint32_t hop2;
        uint32_t serial;
        bool sig1_loaded;
        bool sig2_loaded;
        FuriString* sig1_path;
        FuriString* sig2_path;
        uint8_t learn_type;
    } keeloq_bf2;
};

void subghz_blink_start(SubGhz* subghz);
void subghz_blink_stop(SubGhz* subghz);

bool subghz_tx_start(SubGhz* subghz, FlipperFormat* flipper_format);
void subghz_dialog_message_freq_error(SubGhz* subghz, bool only_rx);

bool subghz_key_load(SubGhz* subghz, const char* file_path, bool show_dialog);
bool subghz_get_next_name_file(SubGhz* subghz, uint8_t max_len);
bool subghz_save_protocol_to_file(
    SubGhz* subghz,
    FlipperFormat* flipper_format,
    const char* dev_file_name);
void subghz_save_to_file(void* context);
bool subghz_load_protocol_from_file(SubGhz* subghz);
bool subghz_rename_file(SubGhz* subghz);
bool subghz_file_available(SubGhz* subghz);
bool subghz_delete_file(SubGhz* subghz);
void subghz_file_name_clear(SubGhz* subghz);
bool subghz_path_is_file(FuriString* path);
SubGhzLoadTypeFile subghz_get_load_type_file(SubGhz* subghz);

void subghz_lock(SubGhz* subghz);
void subghz_unlock(SubGhz* subghz);
bool subghz_is_locked(SubGhz* subghz);

void subghz_rx_key_state_set(SubGhz* subghz, SubGhzRxKeyState state);
SubGhzRxKeyState subghz_rx_key_state_get(SubGhz* subghz);

extern const NotificationSequence subghz_sequence_rx;
extern const NotificationSequence subghz_sequence_rx_locked;
void subghz_save_all(SubGhz* subghz);
