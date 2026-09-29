#include "../mass_storage_app_i.h"
#include "../views/mass_storage_view.h"
#include "../helpers/mass_storage_usb.h"
#include <lib/toolbox/path.h>
#include <cli/cli_vcp.h>
#include <desktop/desktop.h>

#define TAG "MassStorageSceneWork"

static bool file_read(
    void* ctx,
    uint32_t lba,
    uint16_t count,
    uint8_t* out,
    uint32_t* out_len,
    uint32_t out_cap) {
    MassStorageApp* app = ctx;
    FURI_LOG_T(TAG, "file_read lba=%08lX count=%04X out_cap=%08lX", lba, count, out_cap);
    if(!storage_file_seek(app->file, lba * SCSI_BLOCK_SIZE, true)) {
        FURI_LOG_W(TAG, "seek failed");
        return false;
    }
    uint16_t clamp = MIN(out_cap, count * SCSI_BLOCK_SIZE);
    *out_len = storage_file_read(app->file, out, clamp);
    FURI_LOG_T(TAG, "%lu/%lu", *out_len, count * SCSI_BLOCK_SIZE);
    app->bytes_read += *out_len;
    return *out_len == clamp;
}

static bool file_write(void* ctx, uint32_t lba, uint16_t count, uint8_t* buf, uint32_t len) {
    MassStorageApp* app = ctx;
    FURI_LOG_T(TAG, "file_write lba=%08lX count=%04X len=%08lX", lba, count, len);
    if(len != count * SCSI_BLOCK_SIZE) {
        FURI_LOG_W(TAG, "bad write params count=%u len=%lu", count, len);
        return false;
    }
    if(!storage_file_seek(app->file, lba * SCSI_BLOCK_SIZE, true)) {
        FURI_LOG_W(TAG, "seek failed");
        return false;
    }
    app->bytes_written += len;
    return storage_file_write(app->file, buf, len) == len;
}

static uint32_t file_num_blocks(void* ctx) {
    MassStorageApp* app = ctx;
    return storage_file_size(app->file) / SCSI_BLOCK_SIZE;
}

static void file_eject(void* ctx) {
    MassStorageApp* app = ctx;
    FURI_LOG_D(TAG, "EJECT");
    view_dispatcher_send_custom_event(app->view_dispatcher, MassStorageCustomEventEject);
}

static void usb_connection_status_cb(bool connected, void* ctx);

static bool card_read(
    void* ctx,
    uint32_t lba,
    uint16_t count,
    uint8_t* out,
    uint32_t* out_len,
    uint32_t out_cap) {
    MassStorageApp* app = ctx;
    uint32_t blocks = out_cap / SCSI_BLOCK_SIZE;
    if(blocks > count) blocks = count;
    if(blocks == 0) return false;
    if(furi_hal_sd_read_blocks((uint32_t*)out, lba, blocks) != FuriStatusOk) {
        FURI_LOG_W(TAG, "card read failed lba=%08lX count=%lu", lba, blocks);
        return false;
    }
    *out_len = blocks * SCSI_BLOCK_SIZE;
    app->bytes_read += *out_len;
    return true;
}

static bool card_write(void* ctx, uint32_t lba, uint16_t count, uint8_t* buf, uint32_t len) {
    MassStorageApp* app = ctx;
    if(len != (uint32_t)count * SCSI_BLOCK_SIZE) {
        FURI_LOG_W(TAG, "bad card write params count=%u len=%lu", count, len);
        return false;
    }
    if(furi_hal_sd_write_blocks((const uint32_t*)buf, lba, count) != FuriStatusOk) {
        FURI_LOG_W(TAG, "card write failed lba=%08lX count=%u", lba, count);
        return false;
    }
    app->bytes_written += len;
    return true;
}

static uint32_t card_num_blocks(void* ctx) {
    MassStorageApp* app = ctx;
    return app->sd_card_block_count;
}

static void sd_card_disconnect_cb(void* ctx) {
    MassStorageApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, MassStorageCustomEventEject);
}

static void sd_card_set_desktop_flag(bool active) {
    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    desktop_api_set_usb_mode(
        desktop, active ? DesktopUsbModeMassStorage : DesktopUsbModeQflipper);
    furi_record_close(RECORD_DESKTOP);
}

static void sd_card_session_lock(MassStorageApp* app, bool lock) {
    if(app->sd_card_session_locked == lock) return;
    CliVcp* cli_vcp = furi_record_open(RECORD_CLI_VCP);
    if(lock) {
        cli_vcp_session_lock(cli_vcp);
    } else {
        cli_vcp_session_unlock(cli_vcp);
    }
    furi_record_close(RECORD_CLI_VCP);
    app->sd_card_session_locked = lock;
}

static bool sd_card_mode_start(MassStorageApp* app) {
    sd_card_session_lock(app, true);

    uint32_t waited_ms = 0;
    while(furi_hal_usb_is_locked() && waited_ms < 2000) {
        furi_delay_ms(50);
        waited_ms += 50;
    }
    if(furi_hal_usb_is_locked()) {
        furi_hal_usb_unlock();
    }
    furi_delay_ms(300);

    FuriHalSdInfo info;
    if(furi_hal_sd_info(&info) != FuriStatusOk || info.capacity < SCSI_BLOCK_SIZE) {
        FURI_LOG_E(TAG, "SD card info unavailable");
        return false;
    }
    app->sd_card_block_count = (uint32_t)(info.capacity / SCSI_BLOCK_SIZE);

    sd_card_set_desktop_flag(true);

    FS_Error unmount = FSE_INTERNAL;
    for(uint8_t attempt = 0; attempt < 10; attempt++) {
        unmount = storage_sd_unmount(app->fs_api);
        if(unmount == FSE_OK || unmount == FSE_NOT_READY) break;
        furi_delay_ms(100);
    }
    if(unmount != FSE_OK && unmount != FSE_NOT_READY) {
        FURI_LOG_E(TAG, "SD unmount failed (%d)", unmount);
        sd_card_set_desktop_flag(false);
        return false;
    }

    SCSIDeviceFunc fn = {
        .ctx = app,
        .read = card_read,
        .write = card_write,
        .num_blocks = card_num_blocks,
        .eject = file_eject,
    };
    app->usb = mass_storage_usb_start("SD Card", fn);
    if(!app->usb) {
        FURI_LOG_E(TAG, "mass storage start failed");
        storage_sd_mount(app->fs_api);
        sd_card_set_desktop_flag(false);
        return false;
    }
    mass_storage_usb_set_connection_status_callback(app->usb, usb_connection_status_cb, app);
    return true;
}

static void sd_card_mode_stop(MassStorageApp* app) {
    if(app->usb) {
        mass_storage_usb_set_connection_status_callback(app->usb, NULL, NULL);
        mass_storage_usb_stop(app->usb);
        app->usb = NULL;
    }
    storage_sd_mount(app->fs_api);
    sd_card_set_desktop_flag(false);
    sd_card_session_lock(app, false);
}

static void usb_connection_status_cb(bool connected, void* ctx) {
    UNUSED(connected);
    MassStorageApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, MassStorageCustomEventConnectionError);
}

bool mass_storage_scene_work_on_event(void* context, SceneManagerEvent event) {
    MassStorageApp* app = context;
    bool consumed = false;
    if(app->sd_card_mode) {
        if(event.type == SceneManagerEventTypeCustom &&
           event.event == MassStorageCustomEventConnectionError) {
            mass_storage_set_connection_error(app->mass_storage_view);
            return true;
        }
        if(event.type == SceneManagerEventTypeTick) {
            mass_storage_set_stats(app->mass_storage_view, app->bytes_read, app->bytes_written);
            return true;
        }
        if(event.type == SceneManagerEventTypeBack ||
           (event.type == SceneManagerEventTypeCustom &&
            event.event == MassStorageCustomEventEject)) {
            scene_manager_stop(app->scene_manager);
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
        return false;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MassStorageCustomEventEject) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MassStorageSceneFileSelect);
            if(!consumed) {
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, MassStorageSceneStart);
            }
        } else if(event.event == MassStorageCustomEventConnectionError) {
            mass_storage_set_connection_error(app->mass_storage_view);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        mass_storage_set_stats(app->mass_storage_view, app->bytes_read, app->bytes_written);
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, MassStorageSceneFileSelect);
        if(!consumed) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MassStorageSceneStart);
        }
    }
    return consumed;
}

void mass_storage_scene_work_on_enter(void* context) {
    MassStorageApp* app = context;
    app->bytes_read = app->bytes_written = 0;
    mass_storage_clear_connection_error(app->mass_storage_view);

    if(app->sd_card_mode) {
        mass_storage_app_show_loading_popup(app, true);
        app->usb_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        FuriString* card_name = furi_string_alloc_set_str("SD Card");
        mass_storage_set_file_name(app->mass_storage_view, card_name);
        furi_string_free(card_name);
        mass_storage_set_sd_card_mode(app->mass_storage_view, true, sd_card_disconnect_cb, app);
        bool started = sd_card_mode_start(app);
        mass_storage_app_show_loading_popup(app, false);
        if(!started) {
            sd_card_session_lock(app, false);
            scene_manager_stop(app->scene_manager);
            view_dispatcher_stop(app->view_dispatcher);
            return;
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, MassStorageAppViewWork);
        return;
    }

    if(!storage_file_exists(app->fs_api, furi_string_get_cstr(app->file_path))) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, MassStorageSceneStart);
        return;
    }

    mass_storage_app_show_loading_popup(app, true);

    app->usb_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    FuriString* file_name = furi_string_alloc();
    path_extract_filename(app->file_path, file_name, true);

    mass_storage_set_file_name(app->mass_storage_view, file_name);
    app->file = storage_file_alloc(app->fs_api);
    furi_assert(storage_file_open(
        app->file,
        furi_string_get_cstr(app->file_path),
        FSAM_READ | FSAM_WRITE,
        FSOM_OPEN_EXISTING));

    SCSIDeviceFunc fn = {
        .ctx = app,
        .read = file_read,
        .write = file_write,
        .num_blocks = file_num_blocks,
        .eject = file_eject,
    };

    app->usb = mass_storage_usb_start(furi_string_get_cstr(file_name), fn);
    if(app->usb) {
        mass_storage_usb_set_connection_status_callback(app->usb, usb_connection_status_cb, app);
    }

    furi_string_free(file_name);

    mass_storage_app_show_loading_popup(app, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, MassStorageAppViewWork);
}

void mass_storage_scene_work_on_exit(void* context) {
    MassStorageApp* app = context;
    mass_storage_app_show_loading_popup(app, true);

    if(app->usb_mutex) {
        furi_mutex_free(app->usb_mutex);
        app->usb_mutex = NULL;
    }
    if(app->sd_card_mode) {
        sd_card_mode_stop(app);
        mass_storage_app_show_loading_popup(app, false);
        return;
    }
    if(app->usb) {
        mass_storage_usb_set_connection_status_callback(app->usb, NULL, NULL);
        mass_storage_usb_stop(app->usb);
        app->usb = NULL;
    }
    if(app->file) {
        storage_file_free(app->file);
        app->file = NULL;
    }
    mass_storage_app_show_loading_popup(app, false);
}
