#pragma once

#include "helpers/tpms_types.h"

#include "scenes/tpms_scene.h"
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/view_holder.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/number_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/loading.h>
#include <notification/notification_messages.h>
#include "views/tpms_receiver.h"
#include "views/tpms_receiver_info.h"
#include "views/tpms_view_box_list.h"
#include "tpms_history.h"
#include "helpers/tpms_vehicle_groups.h"

#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/registry.h>

#include "helpers/radio_device_loader.h"

typedef struct TPMSApp TPMSApp;

struct TPMSTxRx
{
    SubGhzWorker *worker;

    const SubGhzDevice *radio_device;
    SubGhzEnvironment *environment;
    SubGhzReceiver *receiver;
    SubGhzRadioPreset *preset;
    TPMSHistory *history;
    uint16_t idx_menu_chosen;
    TPMSTxRxState txrx_state;
    TPMSHopperState hopper_state;
    uint8_t hopper_timeout;
    uint8_t hopper_idx_frequency;
    TPMSRxKeyState rx_key_state;
};

typedef struct TPMSTxRx TPMSTxRx;

struct TPMSApp
{
    Gui *gui;
    ViewDispatcher *view_dispatcher;
    TPMSTxRx *txrx;
    SceneManager *scene_manager;
    NotificationApp *notifications;
    VariableItemList *variable_item_list;
    Submenu *submenu;
    Widget *widget;
    TPMSReceiver *tpms_receiver;
    TPMSReceiverInfo *tpms_receiver_info;
    TPMSLock lock;
    SubGhzSetting *setting;
    TPMSRelearn relearn;
    TPMSRelearnType relearn_type;

    NumberInput *number_input;
    ByteInput *byte_input;
    TPMSField tpms_edit_field;
    int32_t tpms_edit_number_value;
    uint8_t tpms_edit_id_bytes[4];

    /* Double-row box list shared by the redesigned Start scene and the
     * Vehicle Make picker (tpms_scene_start.c / tpms_scene_vehicle_make.c)
     * - one instance, re-populated per scene via tpms_box_list_set_options(),
     * same pattern as TPMSViewSubmenu/TPMSViewWidget above. */
    TPMSBoxList *box_list;

    /* Which tpms_vehicle_groups[] entry the guided "Select Model" flow
     * picked, or -1 for the plain "Manual Scan" path (today's original
     * behaviour: hardcoded AM650 + the generic ISM hopper list, untouched).
     * Read by tpms_scene_receiver.c's on_enter (which preset to start on)
     * and tpms_hopper_update() (which candidate list to hop, if any) -
     * see tpms_app_i.c. Reset to -1 whenever Receiver's Back handler tears
     * down back to the Start scene, so a later Manual Scan never inherits a
     * stale group. */
    int8_t active_vehicle_group;
    /* Current page into active_vehicle_group's steps[] while
     * tpms_scene_vehicle_steps.c is on screen. */
    uint8_t vehicle_step_index;

    /* Fires a bare, unmodulated 125kHz carrier on the Flipper's own LF-RFID
     * coil for LF_RELEARN_DURATION_MS (tpms_app_i.c) - the same "hold near
     * the sensor, wake it for an on-demand transmission" trigger
     * flipperzero-tpms's receiver view uses (furi_hal_rfid_tim_read_start/
     * _stop, not the DMA-emulate/replay path). Owned at the app level
     * rather than per-scene since both tpms_scene_relearn.c (manual trigger)
     * and the last page of tpms_scene_vehicle_steps.c (guided flow) fire it,
     * and it must stay valid/stoppable even if the user backs out mid-pulse.
     * See tpms_relearn_lf_start()/tpms_relearn_lf_stop() below. */
    FuriTimer *lf_relearn_timer;
    bool lf_relearn_active;

    /* Auto-retry state for the Receiver ("Scanning") screen's LF hunt -
     * see tpms_scene_receiver.c's tick handler and
     * LF_AUTO_RETRIGGER_INTERVAL_TICKS's comment there for the reasoning
     * (a real TPMS sensor answers an LF trigger with one RF reply, not a
     * held stream, so sitting in RX forever after a single pulse wastes
     * the very time the user is sweeping the Flipper around the sensor
     * looking for the near-field sweet spot - this re-fires the pulse on
     * an interval instead). Armed by tpms_scene_receiver.c whenever an LF
     * trigger is fired from that screen (guided-flow arrival or a manual
     * Re-Trigger), disarmed on Back; count down in 100ms ticks
     * (view_dispatcher's tick period, tpms_app.c). lf_relearn_is_retry
     * distinguishes an auto-fired retry pulse (the miss/"*Reposition*"
     * screen cue + long-vibrate alert apply) from the guided flow's own
     * first pulse or a manual Re-Trigger (neither counts as a "miss" yet). */
    bool lf_auto_retrigger_armed;
    uint8_t lf_auto_retrigger_countdown;
    bool lf_relearn_is_retry;

    /* Startup loading wheel - shown immediately on launch (before the SD-
     * card setting_load()/CC1101 probe work in tpms_app_alloc() has run),
     * removed once the Start scene has something ready to draw. Same
     * ViewHolder-over-the-GUI pattern as subghz_garage's subghz_alloc() -
     * see applications/fox/subghz_garage/subghz.c - fixes the bug where the
     * Apps menu flashed through for a frame or two before this app's own
     * first screen appeared. */
    Loading *startup_loading;
    ViewHolder *startup_holder;
};

void tpms_preset_init(
    void *context,
    const char *preset_name,
    uint32_t frequency,
    uint8_t *preset_data,
    size_t preset_data_size);
bool tpms_set_preset(TPMSApp *app, const char *preset);
void tpms_get_frequency_modulation(TPMSApp *app, FuriString *frequency, FuriString *modulation);
void tpms_begin(TPMSApp *app, uint8_t *preset_data);
uint32_t tpms_rx(TPMSApp *app, uint32_t frequency);
void tpms_idle(TPMSApp *app);
void tpms_rx_end(TPMSApp *app);
void tpms_sleep(TPMSApp *app);
void tpms_hopper_update(TPMSApp *app);

/* Fire (or cancel) the LF 125kHz "relearn" wake pulse described on
 * lf_relearn_timer's comment above. Safe to call tpms_relearn_lf_start()
 * again while already active - it just restarts the pulse/timer. Safe to
 * call tpms_relearn_lf_stop() when not active (no-op). */
void tpms_relearn_lf_start(TPMSApp *app);
void tpms_relearn_lf_stop(TPMSApp *app);

/* Point the radio at one of active_vehicle_group's candidates (wrapping
 * around candidate_count) and reset RSSI hopper timing - used both to seed
 * the initial preset when a guided vehicle flow reaches the Receiver scene
 * and by tpms_hopper_update() to advance through that group's short
 * candidate list instead of the generic ISM frequency table. */
void tpms_vehicle_group_apply_candidate(TPMSApp *app, uint8_t candidate_idx);
