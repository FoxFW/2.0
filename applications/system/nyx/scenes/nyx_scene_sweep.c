#include "../nyx_i.h"

static uint8_t tick_counter; // paces the "locked" LED blink
static uint8_t entry_sensitivity; // what it was on the way in, to spot a change

static void nyx_sweep_event_cb(void* context, SweepViewEvent event) {
    NyxApp* app = context;
    uint32_t custom;
    switch(event) {
    case SweepViewEventResetPeak:
        custom = NyxCustomEventReset;
        break;
    case SweepViewEventRenull:
        custom = NyxCustomEventRenull;
        break;
    case SweepViewEventSensUp:
        custom = NyxCustomEventSensUp;
        break;
    default:
        custom = NyxCustomEventSensDown;
        break;
    }
    /* Never touch app state from the input callback — hand it to the scene on
     * the dispatcher thread instead. */
    view_dispatcher_send_custom_event(app->view_dispatcher, custom);
}

/* Right raises the gain, so it walks the index down through High/Medium/Low.
 * Applied to the running worker immediately: the point of putting this on the
 * sweep screen is not having to stop and go to Settings while you are standing
 * in the middle of a room with the lights off. */
static void nyx_sweep_set_sensitivity(NyxApp* app, int delta) {
    int next = (int)app->settings.sensitivity_index + delta;
    if(next < 0) next = 0;
    if(next > 2) next = 2;
    if((uint8_t)next == app->settings.sensitivity_index) return;

    app->settings.sensitivity_index = (uint8_t)next;
    ir_sense_set_sensitivity(app->sense, app->settings.sensitivity_index);
    /* The floor moved, so every peak and hit recorded under the old one is a
     * different measurement. Start the count again rather than mixing them. */
    ir_sense_reset(app->sense);
    if(app->settings.sound) nyx_notify_click(app);
}

void nyx_scene_sweep_on_enter(void* context) {
    NyxApp* app = context;

    app->was_present = false;
    app->last_click_tick = 0;
    tick_counter = 0;
    entry_sensitivity = app->settings.sensitivity_index;

    ir_sense_set_mode(app->sense, (IrSenseMode)app->settings.mode_index);
    ir_sense_set_sensitivity(app->sense, app->settings.sensitivity_index);
    ir_sense_set_probe_pin(app->sense, app->settings.probe_pin_index);

    sweep_view_set_callback(app->sweep_view, nyx_sweep_event_cb, app);
    sweep_view_reset(app->sweep_view, app->settings.sensitivity_index);

    ir_sense_start(app->sense);
    view_dispatcher_switch_to_view(app->view_dispatcher, NyxViewSweep);
}

bool nyx_scene_sweep_on_event(void* context, SceneManagerEvent event) {
    NyxApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case NyxCustomEventReset:
            ir_sense_reset(app->sense);
            consumed = true;
            break;
        case NyxCustomEventRenull:
            ir_sense_renull(app->sense);
            consumed = true;
            break;
        case NyxCustomEventSensUp:
            nyx_sweep_set_sensitivity(app, -1);
            consumed = true;
            break;
        case NyxCustomEventSensDown:
            nyx_sweep_set_sensitivity(app, +1);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        tick_counter++;

        IrStats st;
        ir_sense_get(app->sense, &st);
        sweep_view_update(app->sweep_view, &st, app->settings.sensitivity_index);
        sweep_view_tick(app->sweep_view);

        /* edges */
        if(st.present && !app->was_present) nyx_notify_found(app);
        if(!st.present && app->was_present) nyx_notify_gone(app);
        app->was_present = st.present;

        /* while an emitter is locked on: blink + geiger clicks that speed up as
         * the reading climbs, so you can hunt with the screen at your side */
        if(st.present) {
            if(app->settings.led && (tick_counter % 3u == 0u)) nyx_notify_present_led(app);

            if(app->settings.sound) {
                uint32_t interval = 360u - 3u * st.level;
                if(interval < 70u) interval = 70u;
                if(interval > 360u) interval = 360u;
                uint32_t now = furi_get_tick();
                if((uint32_t)(now - app->last_click_tick) >= interval) {
                    nyx_notify_click(app);
                    app->last_click_tick = now;
                }
            }
        }
        consumed = true;
    }
    return consumed;
}

void nyx_scene_sweep_on_exit(void* context) {
    NyxApp* app = context;
    ir_sense_stop(app->sense);

    /* A gain change made out here is a real preference, so keep it — but write
     * it once on the way out, never mid-sweep: the card is slow and this is the
     * dispatcher thread that also has to keep draining input. */
    if(app->settings.sensitivity_index != entry_sensitivity) {
        nyx_store_settings_save(&app->settings);
        entry_sensitivity = app->settings.sensitivity_index;
    }
}
