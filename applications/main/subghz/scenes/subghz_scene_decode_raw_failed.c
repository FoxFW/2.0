/**
 * @file subghz_scene_decode_raw_failed.c
 * @brief Shown when Decode RAW finishes with zero results, replacing the
 *        old generic "Done!" / empty history-list screen.
 *
 * Two cases, decided once at on_enter:
 *
 *  1. Not all protocols were enabled when decode ran → show a prompt:
 *     "Raw sub failed to decode with the available protocols"
 *     [Cancel]                          [Re-Run All Protocols]
 *     Re-Run forces every protocol filter bit on and re-runs decode from
 *     scratch. Cancel returns to wherever Decode was launched from.
 *
 *  2. All protocols were already enabled → show a brief auto-dismissing
 *     message ("Failed to decode with available protocols (At used
 *     Modulation)") for ~3 seconds, then return automatically — no need
 *     to ask the user anything since re-running with the same (already
 *     maximal) protocol set would produce an identical result.
 *
 * The cleanup-and-return logic (stop/free the file encoder worker, clear
 * the rx callback, reset notification state, navigate back) is copied
 * verbatim from the proven pattern in subghz_scene_save_success.c's
 * DecodeRAW-context branch, since this scene reaches the same point in
 * the same way (entered via scene_manager_next_scene from DecodeRAW).
 */

#include "../subghz_i.h"
#include "../subghz_protocol_filter.h"
#include "../helpers/subghz_custom_event.h"
#include <lib/subghz/subghz_protocol_registry.h>

#define TAG "SubGhzSceneDecodeRawFailed"
#define DECODE_FAILED_POPUP_TIMEOUT_MS 3000

/* Set by subghz_scene_decode_raw.c right before navigating here, so
 * on_enter knows which of the two cases to show without recomputing it
 * twice in two different places. */
static bool g_all_protocols_were_enabled = true;

void subghz_scene_decode_raw_failed_set_context(bool all_protocols_enabled) {
    g_all_protocols_were_enabled = all_protocols_enabled;
}

/* ── Shared cleanup + return, copied verbatim from the proven pattern in
 * subghz_scene_save_success.c's DecodeRAW-context branch. ── */
static void decode_raw_failed_cleanup_and_return(SubGhz* subghz) {
    scene_manager_set_scene_state(
        subghz->scene_manager, SubGhzSceneDecodeRAW, SubGhzDecodeRawStateStart);

    subghz->idx_menu_chosen = 0;
    subghz_txrx_set_rx_callback(subghz->txrx, NULL, subghz);

    if(subghz_file_encoder_worker_is_running(subghz->decode_raw_file_worker_encoder)) {
        subghz_file_encoder_worker_stop(subghz->decode_raw_file_worker_encoder);
    }
    subghz_file_encoder_worker_free(subghz->decode_raw_file_worker_encoder);

    subghz->state_notifications = SubGhzNotificationStateIDLE;
    subghz_rx_key_state_set(subghz, SubGhzRxKeyStateIDLE);

    scene_manager_set_scene_state(
        subghz->scene_manager, SubGhzSceneReadRAW, SubGhzCustomEventManagerNoSet);

    /* DecodeRAW is still on the stack (we arrived here via next_scene from
     * it) — pop back through it to wherever launched Decode in the first
     * place (the RAW player, via MoreRAW). */
    if(!scene_manager_search_and_switch_to_previous_scene(
           subghz->scene_manager, SubGhzSceneMoreRAW)) {
        if(!scene_manager_search_and_switch_to_previous_scene(
               subghz->scene_manager, SubGhzSceneStart)) {
            scene_manager_stop(subghz->scene_manager);
            view_dispatcher_stop(subghz->view_dispatcher);
        }
    }
}

/* ── Case 2: timed auto-dismiss popup ── */

static void decode_raw_failed_popup_cb(void* context) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, SubGhzCustomEventDecodeRawFailedCancel);
}

/* ── Case 1: Re-Run / Cancel widget buttons ── */

static void decode_raw_failed_widget_cb(GuiButtonType result, InputType type, void* context) {
    SubGhz* subghz = context;
    if(type != InputTypeShort) return;

    if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(subghz->view_dispatcher, SubGhzCustomEventDecodeRawFailedRetry);
    } else if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(subghz->view_dispatcher, SubGhzCustomEventDecodeRawFailedCancel);
    }
}

/* ── Scene lifecycle ─────────────────────────────────────────────────────── */

void subghz_scene_decode_raw_failed_on_enter(void* context) {
    SubGhz* subghz = context;

    if(g_all_protocols_were_enabled) {
        Popup* popup = subghz->popup;
        popup_set_header(popup, "No match found", 64, 6, AlignCenter, AlignTop);
        popup_set_text(
            popup,
            "Failed to decode with\navailable protocols\n(at used modulation)",
            64, 22, AlignCenter, AlignTop);
        popup_set_timeout(popup, DECODE_FAILED_POPUP_TIMEOUT_MS);
        popup_set_context(popup, subghz);
        popup_set_callback(popup, decode_raw_failed_popup_cb);
        popup_enable_timeout(popup);
        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdPopup);
    } else {
        Widget* widget = subghz->widget;
        widget_add_string_multiline_element(
            widget, 64, 8, AlignCenter, AlignTop, FontPrimary, "Decode Failed");
        /* FontSecondary is ~8px/line. 3 lines = 24px. Buttons sit at
         * y≈51. So text must start no lower than y=51-24=27 to avoid
         * overlap. Use y=18 to give a clean gap below the title. */
        widget_add_string_multiline_element(
            widget, 64, 18, AlignCenter, AlignTop, FontSecondary,
            "Raw sub failed to decode\nwith the available\nprotocols.");
        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Cancel", decode_raw_failed_widget_cb, subghz);
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Re-Run All", decode_raw_failed_widget_cb, subghz);
        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdWidget);
    }
}

bool subghz_scene_decode_raw_failed_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;

    if(event.type == SceneManagerEventTypeBack) {
        decode_raw_failed_cleanup_and_return(subghz);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubGhzCustomEventDecodeRawFailedCancel) {
            decode_raw_failed_cleanup_and_return(subghz);
            return true;
        }
        if(event.event == SubGhzCustomEventDecodeRawFailedRetry) {
            /* Force every protocol on, then re-run Decode from scratch.
             *
             * The issue with the previous approach (search_and_switch_to
             * _previous_scene for SubGhzSceneDecodeRAW): after our cleanup
             * path runs, DecodeRAW may have already been popped off the
             * scene stack, so the search fails and we fall through to
             * next_scene — but without the worker being freshly allocated
             * (because cleanup_and_return already freed it), the decode
             * thread immediately re-enters its "waiting for work" loop at
             * whatever progress it was at, hence "stuck at 97%".
             *
             * Fix: fully reset the scene state FIRST, then unconditionally
             * use next_scene. DecodeRAW's on_enter allocates a fresh worker
             * and starts from the beginning when it sees
             * SubGhzDecodeRawStateStart. */
            subghz_protocol_filter_reset(subghz->protocol_filter);
            subghz_protocol_filter_save(subghz->protocol_filter);

            /* Ensure any residual worker is fully stopped before re-entering */
            if(subghz->decode_raw_file_worker_encoder &&
               subghz_file_encoder_worker_is_running(subghz->decode_raw_file_worker_encoder)) {
                subghz_file_encoder_worker_stop(subghz->decode_raw_file_worker_encoder);
            }

            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneDecodeRAW, SubGhzDecodeRawStateStart);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneDecodeRAW);
            return true;
        }
    }

    return false;
}

void subghz_scene_decode_raw_failed_on_exit(void* context) {
    SubGhz* subghz = context;
    popup_reset(subghz->popup);
    widget_reset(subghz->widget);
}
