#include "tpms_app_i.h"
#include <furi_hal_rfid.h>

#define TAG "TPMS"
#include <flipper_format/flipper_format_i.h>

/* LF 125kHz "relearn"/wake trigger - see lf_relearn_timer's comment in
 * tpms_app_i.h. Ported from flipperzero-tpms's own receiver view
 * (views/tpms_receiver.c: tpms_relearn_start()/tpms_relearn_stop(), bound
 * to the Right key) - same two HAL calls, same idea (leave the RFID coil's
 * normal "read" timer running unmodulated as a bare carrier instead of
 * actually reading anything), just owned at the app level here so both the
 * manual Relearn scene and the guided vehicle-steps flow can fire it. That
 * project's README says "1 second"; its actual code uses a 3-second window
 * - this follows the code, not the doc, on the assumption the code is what
 * was actually tested. */
#define LF_RELEARN_DURATION_MS 3000
#define LF_RELEARN_FREQUENCY_HZ 125000.0f
#define LF_RELEARN_DUTY 0.5f

static void tpms_relearn_lf_timer_callback(void* context) {
    TPMSApp* app = context;
    tpms_relearn_lf_stop(app);
}

void tpms_relearn_lf_start(TPMSApp* app) {
    furi_assert(app);
    if(app->lf_relearn_active) {
        tpms_relearn_lf_stop(app);
    }
    if(!app->lf_relearn_timer) {
        app->lf_relearn_timer =
            furi_timer_alloc(tpms_relearn_lf_timer_callback, FuriTimerTypeOnce, app);
    }
    app->lf_relearn_active = true;
    furi_hal_rfid_tim_read_start(LF_RELEARN_FREQUENCY_HZ, LF_RELEARN_DUTY);
    furi_timer_start(app->lf_relearn_timer, furi_ms_to_ticks(LF_RELEARN_DURATION_MS));
}

void tpms_relearn_lf_stop(TPMSApp* app) {
    furi_assert(app);
    if(!app->lf_relearn_active) return;
    app->lf_relearn_active = false;
    if(app->lf_relearn_timer) {
        furi_timer_stop(app->lf_relearn_timer);
    }
    furi_hal_rfid_tim_read_stop();
}

void tpms_vehicle_group_apply_candidate(TPMSApp* app, uint8_t candidate_idx) {
    furi_assert(app);
    if(app->active_vehicle_group < 0 || app->active_vehicle_group >= TPMS_VEHICLE_GROUP_COUNT) {
        return;
    }
    const TPMSVehicleGroup* group = &tpms_vehicle_groups[app->active_vehicle_group];
    if(group->candidate_count == 0) return;
    if(candidate_idx >= group->candidate_count) candidate_idx = 0;
    const TPMSVehicleRfCandidate* candidate = &group->candidates[candidate_idx];
    tpms_preset_init(app, candidate->preset_name, candidate->frequency, NULL, 0);
}

void tpms_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size) {
    furi_assert(context);
    TPMSApp* app = context;
    furi_string_set(app->txrx->preset->name, preset_name);
    app->txrx->preset->frequency = frequency;
    app->txrx->preset->data = preset_data;
    app->txrx->preset->data_size = preset_data_size;
}

bool tpms_set_preset(TPMSApp* app, const char* preset) {
    if(!strcmp(preset, "FuriHalSubGhzPresetOok270Async")) {
        furi_string_set(app->txrx->preset->name, "AM270");
    } else if(!strcmp(preset, "FuriHalSubGhzPresetOok650Async")) {
        furi_string_set(app->txrx->preset->name, "AM650");
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev238Async")) {
        furi_string_set(app->txrx->preset->name, "FM238");
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev12KAsync")) {
        furi_string_set(app->txrx->preset->name, "FM12K");
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev476Async")) {
        furi_string_set(app->txrx->preset->name, "FM476");
    } else if(!strcmp(preset, "FuriHalSubGhzPresetCustom")) {
        furi_string_set(app->txrx->preset->name, "CUSTOM");
    } else {
        FURI_LOG_E(TAG, "Unknown preset");
        return false;
    }
    return true;
}

void tpms_get_frequency_modulation(TPMSApp* app, FuriString* frequency, FuriString* modulation) {
    furi_assert(app);
    if(frequency != NULL) {
        furi_string_printf(
            frequency,
            "%03ld.%02ld",
            app->txrx->preset->frequency / 1000000 % 1000,
            app->txrx->preset->frequency / 10000 % 100);
    }
    if(modulation != NULL) {
        furi_string_printf(modulation, "%.2s", furi_string_get_cstr(app->txrx->preset->name));
    }
}

void tpms_begin(TPMSApp* app, uint8_t* preset_data) {
    furi_assert(app);
    UNUSED(preset_data);
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(preset_data);
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
    app->txrx->txrx_state = TPMSTxRxStateIDLE;
}

uint32_t tpms_rx(TPMSApp* app, uint32_t frequency) {
    furi_assert(app);
    if(!furi_hal_subghz_is_frequency_valid(frequency)) {
        furi_crash("TPMS: Incorrect RX frequency.");
    }
    furi_assert(
        app->txrx->txrx_state != TPMSTxRxStateRx && app->txrx->txrx_state != TPMSTxRxStateSleep);

    furi_hal_subghz_idle();
    uint32_t value = furi_hal_subghz_set_frequency_and_path(frequency);
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_rx();

    furi_hal_subghz_start_async_rx(subghz_worker_rx_callback, app->txrx->worker);
    subghz_worker_start(app->txrx->worker);
    app->txrx->txrx_state = TPMSTxRxStateRx;
    return value;
}

void tpms_idle(TPMSApp* app) {
    furi_assert(app);
    furi_assert(app->txrx->txrx_state != TPMSTxRxStateSleep);
    furi_hal_subghz_idle();
    app->txrx->txrx_state = TPMSTxRxStateIDLE;
}

void tpms_rx_end(TPMSApp* app) {
    furi_assert(app);
    furi_assert(app->txrx->txrx_state == TPMSTxRxStateRx);
    if(subghz_worker_is_running(app->txrx->worker)) {
        subghz_worker_stop(app->txrx->worker);
        furi_hal_subghz_stop_async_rx();
    }
    furi_hal_subghz_idle();
    app->txrx->txrx_state = TPMSTxRxStateIDLE;
}

void tpms_sleep(TPMSApp* app) {
    furi_assert(app);
    furi_hal_subghz_sleep();
    app->txrx->txrx_state = TPMSTxRxStateSleep;
}

void tpms_hopper_update(TPMSApp* app) {
    furi_assert(app);

    switch(app->txrx->hopper_state) {
    case TPMSHopperStateOFF:
    case TPMSHopperStatePause:
        return;
    case TPMSHopperStateRSSITimeOut:
        if(app->txrx->hopper_timeout != 0) {
            app->txrx->hopper_timeout--;
            return;
        }
        break;
    default:
        break;
    }
    float rssi = -127.0f;
    if(app->txrx->hopper_state != TPMSHopperStateRSSITimeOut) {
        // See RSSI Calculation timings in CC1101 17.3 RSSI
        rssi = furi_hal_subghz_get_rssi();

        // Stay if RSSI is high enough
        if(rssi > -90.0f) {
            app->txrx->hopper_timeout = 10;
            app->txrx->hopper_state = TPMSHopperStateRSSITimeOut;
            return;
        }
    } else {
        app->txrx->hopper_state = TPMSHopperStateRunnig;
    }

    // A guided vehicle-group flow hops a short, brand-relevant {frequency,
    // modulation} candidate list (tpms_vehicle_groups.c) instead of the
    // generic ISM frequency table - same hopper_idx_frequency field, just
    // bounded by whichever list is active.
    bool group_active =
        app->active_vehicle_group >= 0 && app->active_vehicle_group < TPMS_VEHICLE_GROUP_COUNT;
    uint8_t candidate_count = group_active ?
                                   tpms_vehicle_groups[app->active_vehicle_group].candidate_count :
                                   subghz_setting_get_hopper_frequency_count(app->setting);
    if(candidate_count == 0) return;

    // Select next frequency/candidate
    if(app->txrx->hopper_idx_frequency < candidate_count - 1) {
        app->txrx->hopper_idx_frequency++;
    } else {
        app->txrx->hopper_idx_frequency = 0;
    }

    if(app->txrx->txrx_state == TPMSTxRxStateRx) {
        tpms_rx_end(app);
    };
    if(app->txrx->txrx_state == TPMSTxRxStateIDLE) {
        subghz_receiver_reset(app->txrx->receiver);
        if(group_active) {
            // Unlike the generic hop below (frequency-only, via a bare
            // tpms_rx() retune), a vehicle group's candidates can also
            // differ by modulation - landing on a new one needs the radio's
            // custom preset reloaded, so this goes through tpms_begin()
            // again rather than just tpms_rx().
            tpms_vehicle_group_apply_candidate(app, app->txrx->hopper_idx_frequency);
            tpms_begin(
                app,
                subghz_setting_get_preset_data_by_name(
                    app->setting, furi_string_get_cstr(app->txrx->preset->name)));
            tpms_rx(app, app->txrx->preset->frequency);
        } else {
            app->txrx->preset->frequency = subghz_setting_get_hopper_frequency(
                app->setting, app->txrx->hopper_idx_frequency);
            tpms_rx(app, app->txrx->preset->frequency);
        }
    }
}
