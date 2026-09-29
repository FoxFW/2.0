#include "../subghz_i.h"
#include <lib/subghz/protocols/fiat_v1.h>

#define FIAT_V1_RECOVER_EV_DONE (0xF1)

typedef enum {
    FiatV1RecoverStateRunning,
    FiatV1RecoverStateDone,
} FiatV1RecoverState;

typedef struct {
    SubGhz* subghz;
    FuriThread* thread;
    volatile bool cancel;
    uint32_t start_tick;

    uint32_t uid;
    uint8_t button;
    uint16_t control;
    uint32_t hop;

    FiatV1RecoverState state;
    bool success;
    uint8_t found_key[6];
    uint32_t found_epoch;
} FiatV1RecoverCtx;

static bool fiat_v1_recover_progress_cb(uint8_t progress, uint32_t combos_tested, void* context) {
    FiatV1RecoverCtx* ctx = context;
    if(ctx->cancel) return false;

    uint32_t elapsed_ms = furi_get_tick() - ctx->start_tick;
    uint32_t elapsed_sec = elapsed_ms / 1000;
    uint32_t combos_per_sec =
        (elapsed_ms > 0) ? (uint32_t)((uint64_t)combos_tested * 1000 / elapsed_ms) : 0;
    uint32_t total = fiat_v1_hitag2_recovery_total();
    uint32_t remaining = (combos_tested < total) ? (total - combos_tested) : 0;
    uint32_t eta_sec = (combos_per_sec > 0) ? (remaining / combos_per_sec) : 0;

    subghz_view_fiat_v1_recover_update_stats(
        ctx->subghz->subghz_fiat_v1_recover,
        progress,
        combos_tested,
        combos_per_sec,
        elapsed_sec,
        eta_sec);

    return true;
}

static int32_t fiat_v1_recover_thread(void* context) {
    FiatV1RecoverCtx* ctx = context;

    ctx->success = fiat_v1_hitag2_recover(
        ctx->uid,
        ctx->button,
        ctx->control,
        ctx->hop,
        ctx->found_key,
        &ctx->found_epoch,
        fiat_v1_recover_progress_cb,
        ctx);

    view_dispatcher_send_custom_event(ctx->subghz->view_dispatcher, FIAT_V1_RECOVER_EV_DONE);
    return 0;
}

static void fiat_v1_recover_view_callback(SubGhzCustomEvent event, void* context) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, event);
}

static void fiat_v1_recover_widget_callback(GuiButtonType result, InputType type, void* context) {
    SubGhz* subghz = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(subghz->view_dispatcher, result);
    }
}

static void fiat_v1_recover_draw_result(SubGhz* subghz, FiatV1RecoverCtx* ctx) {
    widget_reset(subghz->widget);

    if(ctx->success) {
        FuriString* str = furi_string_alloc();
        furi_string_printf(
            str,
            "\e#Key Recovered!\e#\n"
            "%02X %02X %02X %02X %02X %02X\n"
            "Epoch: 0x%05lX",
            ctx->found_key[0],
            ctx->found_key[1],
            ctx->found_key[2],
            ctx->found_key[3],
            ctx->found_key[4],
            ctx->found_key[5],
            ctx->found_epoch);
        widget_add_text_box_element(
            subghz->widget, 0, 0, 128, 42, AlignCenter, AlignTop, furi_string_get_cstr(str), true);
        furi_string_free(str);

        widget_add_button_element(
            subghz->widget, GuiButtonTypeLeft, "Apply", fiat_v1_recover_widget_callback, subghz);
        widget_add_button_element(
            subghz->widget, GuiButtonTypeRight, "CBF", fiat_v1_recover_widget_callback, subghz);
    } else {
        widget_add_text_box_element(
            subghz->widget,
            0,
            0,
            128,
            42,
            AlignCenter,
            AlignTop,
            "\e#No Key Found\e#\nTry entering the key\nmanually instead.",
            true);
        widget_add_button_element(
            subghz->widget, GuiButtonTypeCenter, "Back", fiat_v1_recover_widget_callback, subghz);
    }

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdWidget);
}

void subghz_scene_fiat_v1_recover_on_enter(void* context) {
    SubGhz* subghz = context;

    FiatV1RecoverCtx* ctx = malloc(sizeof(FiatV1RecoverCtx));
    memset(ctx, 0, sizeof(FiatV1RecoverCtx));
    ctx->subghz = subghz;

    FlipperFormat* fff = subghz_txrx_get_fff_data(subghz->txrx);
    uint32_t value = 0;
    flipper_format_rewind(fff);
    flipper_format_read_uint32(fff, "Serial", &value, 1);
    ctx->uid = value;
    flipper_format_rewind(fff);
    flipper_format_read_uint32(fff, "Btn", &value, 1);
    ctx->button = (uint8_t)value;
    flipper_format_rewind(fff);
    flipper_format_read_uint32(fff, "Cnt", &value, 1);
    ctx->control = (uint16_t)value;
    flipper_format_rewind(fff);
    flipper_format_read_uint32(fff, "Hop", &value, 1);
    ctx->hop = value;

    scene_manager_set_scene_state(
        subghz->scene_manager, SubGhzSceneFiatV1Recover, (uint32_t)(uintptr_t)ctx);

    subghz_view_fiat_v1_recover_reset(subghz->subghz_fiat_v1_recover);
    subghz_view_fiat_v1_recover_set_callback(
        subghz->subghz_fiat_v1_recover, fiat_v1_recover_view_callback, subghz);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdFiatV1Recover);

    ctx->start_tick = furi_get_tick();
    ctx->thread = furi_thread_alloc_ex("FiatV1Recover", 2048, fiat_v1_recover_thread, ctx);
    furi_thread_start(ctx->thread);
}

bool subghz_scene_fiat_v1_recover_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    FiatV1RecoverCtx* ctx = (FiatV1RecoverCtx*)(uintptr_t)
        scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneFiatV1Recover);
    if(!ctx) return false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FIAT_V1_RECOVER_EV_DONE) {
            if(ctx->thread) {
                furi_thread_join(ctx->thread);
                furi_thread_free(ctx->thread);
                ctx->thread = NULL;
            }

            if(ctx->success) {
                FlipperFormat* fff = subghz_txrx_get_fff_data(subghz->txrx);
                if(fff) {
                    flipper_format_insert_or_update_hex(
                        fff, FIAT_V1_HITAG2_KEY_FIELD, ctx->found_key, sizeof(ctx->found_key));
                    flipper_format_insert_or_update_uint32(
                        fff, FIAT_V1_HITAG2_EPOCH_FIELD, &ctx->found_epoch, 1U);
                    subghz_save_protocol_to_file(
                        subghz, fff, furi_string_get_cstr(subghz->file_path));
                }
            }

            ctx->state = FiatV1RecoverStateDone;
            fiat_v1_recover_draw_result(subghz, ctx);
            return true;

        } else if(event.event == SubGhzCustomEventViewTransmitterBack) {

            if(ctx->thread) {
                ctx->cancel = true;
                furi_thread_join(ctx->thread);
                furi_thread_free(ctx->thread);
                ctx->thread = NULL;
            }
            free(ctx);
            scene_manager_set_scene_state(subghz->scene_manager, SubGhzSceneFiatV1Recover, 0);
            scene_manager_previous_scene(subghz->scene_manager);
            return true;

        } else if(event.event == GuiButtonTypeLeft) {

            free(ctx);
            scene_manager_set_scene_state(subghz->scene_manager, SubGhzSceneFiatV1Recover, 0);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneTransmitter);
            return true;

        } else if(event.event == GuiButtonTypeRight) {

            free(ctx);
            scene_manager_set_scene_state(subghz->scene_manager, SubGhzSceneFiatV1Recover, 0);
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneCounterBf);
            return true;

        } else if(event.event == GuiButtonTypeCenter) {

            free(ctx);
            scene_manager_set_scene_state(subghz->scene_manager, SubGhzSceneFiatV1Recover, 0);
            scene_manager_previous_scene(subghz->scene_manager);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(ctx->state == FiatV1RecoverStateRunning) {

            return true;
        }
        free(ctx);
        scene_manager_set_scene_state(subghz->scene_manager, SubGhzSceneFiatV1Recover, 0);
        scene_manager_previous_scene(subghz->scene_manager);
        return true;
    }
    return false;
}

void subghz_scene_fiat_v1_recover_on_exit(void* context) {
    SubGhz* subghz = context;
    FiatV1RecoverCtx* ctx = (FiatV1RecoverCtx*)(uintptr_t)
        scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneFiatV1Recover);

    if(ctx) {
        if(ctx->thread) {
            ctx->cancel = true;
            furi_thread_join(ctx->thread);
            furi_thread_free(ctx->thread);
            ctx->thread = NULL;
        }
        free(ctx);
        scene_manager_set_scene_state(subghz->scene_manager, SubGhzSceneFiatV1Recover, 0);
    }

    widget_reset(subghz->widget);
}
