#include "../subghz_i.h"

#include <lib/subghz/protocols/renault_v1.h>

#define TAG "SubGhzSceneSeedBf"

#define SEED_BF_EVENT_DONE     (0xE0)
#define SEED_BF_EVENT_BACK     (0xE1)
#define SEED_BF_EVENT_PROGRESS (0xE2)

#define SEED_BF_EVENT_PROGRESS_PCT(evt) ((uint8_t)(((evt) >> 8) & 0xFFU))

typedef struct {
    SubGhz* subghz;
    FuriThread* thread;
    volatile bool cancel;
    uint64_t data;
    uint64_t data_2;
    uint32_t serial;
    uint8_t button;
    uint32_t cnt;
    bool success;
    uint32_t seed;
    bool done_handled;
    volatile uint8_t progress;
    uint8_t last_sent_progress;
    uint32_t last_progress_tick;
    bool running;

    char body[32];
} SeedBfCtx;

static bool seed_bf_progress_cb(uint8_t progress, uint32_t cand_tested, void* context) {
    UNUSED(cand_tested);
    SeedBfCtx* ctx = context;
    if(ctx->cancel) return false;

    ctx->progress = progress;

    uint32_t now = furi_get_tick();
    if(progress != ctx->last_sent_progress || (now - ctx->last_progress_tick) >= 200U) {
        ctx->last_sent_progress = progress;
        ctx->last_progress_tick = now;
        view_dispatcher_send_custom_event(
            ctx->subghz->view_dispatcher, SEED_BF_EVENT_PROGRESS | ((uint32_t)progress << 8));
    }

    furi_delay_ms(1);
    return true;
}

static int32_t seed_bf_thread(void* context) {
    SeedBfCtx* ctx = context;

    uint32_t seed = 0;
    bool ok = subghz_protocol_renault_v1_run_seed_bf_ex(
        ctx->data, ctx->data_2, ctx->serial, ctx->button, ctx->cnt, &seed, seed_bf_progress_cb, ctx);
    if(!ctx->cancel) {
        ctx->success = ok;
        ctx->seed = seed;
    }

    FURI_LOG_I(
        TAG,
        "Seed BF done: ok=%d cancel=%d seed=%08lX",
        (int)ok,
        (int)ctx->cancel,
        (unsigned long)seed);

    view_dispatcher_send_custom_event(ctx->subghz->view_dispatcher, SEED_BF_EVENT_DONE);
    return 0;
}

static void seed_bf_popup_callback(void* context) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, SEED_BF_EVENT_BACK);
}

static void seed_bf_write_result_and_save(SeedBfCtx* ctx, uint8_t recovered_value) {
    FlipperFormat* fff = subghz_txrx_get_fff_data(ctx->subghz->txrx);
    if(!fff) return;

    if(recovered_value == RENAULT_V1_SEED_RECOVERED_YES) {
        uint8_t seed_be[4] = {
            (uint8_t)(ctx->seed >> 24U),
            (uint8_t)(ctx->seed >> 16U),
            (uint8_t)(ctx->seed >> 8U),
            (uint8_t)ctx->seed,
        };
        flipper_format_rewind(fff);
        flipper_format_insert_or_update_hex(fff, "Seed", seed_be, 4U);
    }

    flipper_format_rewind(fff);
    flipper_format_insert_or_update_hex(fff, "Recovered", &recovered_value, 1U);

    subghz_save_protocol_to_file(ctx->subghz, fff, furi_string_get_cstr(ctx->subghz->file_path));
}

static void seed_bf_stop_thread(SeedBfCtx* ctx) {
    if(ctx->thread) {
        furi_thread_join(ctx->thread);
        furi_thread_free(ctx->thread);
        ctx->thread = NULL;
    }
}

void subghz_scene_seed_bf_on_enter(void* context) {
    SubGhz* subghz = context;

    SeedBfCtx* ctx = malloc(sizeof(SeedBfCtx));
    memset(ctx, 0, sizeof(*ctx));
    ctx->subghz = subghz;

    FlipperFormat* fff = subghz_txrx_get_fff_data(subghz->txrx);
    bool fields_ok = false;
    if(fff) {
        uint8_t key_bytes[8] = {0};
        uint8_t key2_bytes[8] = {0};
        flipper_format_rewind(fff);
        if(flipper_format_read_hex(fff, "Key", key_bytes, sizeof(key_bytes))) {
            flipper_format_rewind(fff);
            if(flipper_format_read_hex(fff, "Key_2", key2_bytes, sizeof(key2_bytes))) {
                uint64_t data = 0;
                uint64_t data_2 = 0;
                for(size_t i = 0; i < sizeof(key_bytes); i++) {
                    data = (data << 8) | key_bytes[i];
                }
                for(size_t i = 0; i < sizeof(key2_bytes); i++) {
                    data_2 = (data_2 << 8) | key2_bytes[i];
                }
                ctx->data = data;
                ctx->data_2 = data_2;

                uint32_t serial = 0;
                uint32_t btn_u32 = 0;
                uint32_t cnt_u32 = 0;
                flipper_format_rewind(fff);
                flipper_format_read_uint32(fff, "Serial", &serial, 1);
                flipper_format_rewind(fff);
                flipper_format_read_uint32(fff, "Btn", &btn_u32, 1);
                flipper_format_rewind(fff);
                flipper_format_read_uint32(fff, "Cnt", &cnt_u32, 1);
                ctx->serial = serial;
                ctx->button = (uint8_t)btn_u32;
                ctx->cnt = cnt_u32;
                fields_ok = true;
            }
        }
    }

    scene_manager_set_scene_state(
        subghz->scene_manager, SubGhzSceneSeedBf, (uint32_t)(uintptr_t)ctx);

    Popup* popup = subghz->popup;
    popup_reset(popup);
    popup_set_context(popup, subghz);

    if(!fields_ok) {

        popup_set_callback(popup, seed_bf_popup_callback);
        popup_set_header(popup, "Seed BF", 64, 6, AlignCenter, AlignTop);
        popup_set_text(popup, "Missing Key/Key_2\nfields.", 64, 30, AlignCenter, AlignTop);
        popup_disable_timeout(popup);
        view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdPopup);
        return;
    }

    ctx->running = true;
    ctx->progress = 0;
    ctx->last_sent_progress = 0xFF;
    ctx->last_progress_tick = furi_get_tick();
    strncpy(ctx->body, "Running... 0%", sizeof(ctx->body) - 1);

    popup_set_callback(popup, NULL);
    popup_set_header(popup, "Seed BF", 64, 12, AlignCenter, AlignTop);
    popup_set_text(popup, ctx->body, 64, 34, AlignCenter, AlignTop);
    popup_disable_timeout(popup);
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdPopup);

    ctx->thread = furi_thread_alloc_ex("SeedBF", 2048, seed_bf_thread, ctx);

    furi_thread_set_priority(ctx->thread, FuriThreadPriorityLow);
    furi_thread_start(ctx->thread);
}

bool subghz_scene_seed_bf_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    SeedBfCtx* ctx = (SeedBfCtx*)(uintptr_t)
        scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneSeedBf);
    if(!ctx) return false;

    if(event.type == SceneManagerEventTypeCustom) {
        if((event.event & 0xFFU) == SEED_BF_EVENT_PROGRESS) {
            if(ctx->running && !ctx->done_handled) {
                uint8_t p = SEED_BF_EVENT_PROGRESS_PCT(event.event);
                snprintf(ctx->body, sizeof(ctx->body), "Running... %u%%", (unsigned)p);
                popup_set_text(subghz->popup, ctx->body, 64, 34, AlignCenter, AlignTop);
            }
            return true;
        } else if(event.event == SEED_BF_EVENT_DONE) {
            ctx->running = false;
            if(ctx->done_handled) return true;
            ctx->done_handled = true;

            seed_bf_stop_thread(ctx);

            Popup* popup = subghz->popup;
            popup_reset(popup);
            popup_set_context(popup, subghz);
            popup_set_callback(popup, seed_bf_popup_callback);

            if(ctx->success) {
                seed_bf_write_result_and_save(ctx, RENAULT_V1_SEED_RECOVERED_YES);

                snprintf(
                    ctx->body, sizeof(ctx->body), "Seed: %08lX", (unsigned long)ctx->seed);
                popup_set_header(popup, "Seed found", 64, 6, AlignCenter, AlignTop);
                popup_set_text(popup, ctx->body, 64, 30, AlignCenter, AlignTop);
                FURI_LOG_I(TAG, "SEED recovered: %08lX", (unsigned long)ctx->seed);
            } else {
                seed_bf_write_result_and_save(ctx, RENAULT_V1_SEED_RECOVERED_BF_MISS);

                popup_set_header(popup, "Seed BF", 64, 6, AlignCenter, AlignTop);
                popup_set_text(popup, "Seed not found", 64, 30, AlignCenter, AlignTop);
            }
            popup_disable_timeout(popup);
            view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdPopup);
            return true;

        } else if(event.event == SEED_BF_EVENT_BACK) {
            scene_manager_previous_scene(subghz->scene_manager);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {

        ctx->cancel = true;
        scene_manager_previous_scene(subghz->scene_manager);
        return true;
    }
    return false;
}

void subghz_scene_seed_bf_on_exit(void* context) {
    SubGhz* subghz = context;
    SeedBfCtx* ctx = (SeedBfCtx*)(uintptr_t)
        scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneSeedBf);

    if(ctx) {
        if(ctx->thread) {
            ctx->cancel = true;
            furi_thread_join(ctx->thread);
            furi_thread_free(ctx->thread);
            ctx->thread = NULL;
        }
        free(ctx);
        scene_manager_set_scene_state(subghz->scene_manager, SubGhzSceneSeedBf, 0);
    }

    popup_reset(subghz->popup);
}
