#include "downloader.h"
#include "github_release.h"
#include "version_util.h"
#include "update_meta.h"
#include "json_mini.h"
#include "strutil.h"

#include <toolbox/version.h>
#include <furi_hal_version.h>

#include <string.h>
#include <stdio.h>

#define DL_TIMEOUT_MS            15000
#define DL_CHUNK_TIMEOUT_MS      10000
#define RELEASE_CHECK_TIMEOUT_MS 45000
#define DL_STREAM_FRAME_MAX      1024

typedef enum { WaitOk, WaitError, WaitTimeout } WaitResult;

static EspAtMsg s_esp_msg;

static WaitResult wait_for_line_prefix(
    EspAt* esp_at,
    const char* tag,
    char* out_rest,
    size_t out_rest_size,
    uint32_t timeout_ms) {
    uint32_t start = furi_get_tick();
    size_t tag_len = strlen(tag);
    while((furi_get_tick() - start) < timeout_ms) {
        if(!esp_at_receive(esp_at, &s_esp_msg, 300)) continue;
        if(strncmp(s_esp_msg.line, tag, tag_len) == 0) {
            if(out_rest && out_rest_size) str_copy(out_rest, out_rest_size, s_esp_msg.line + tag_len);
            return WaitOk;
        }
        if(strncmp(s_esp_msg.line, "[ERROR]", 7) == 0) {
            if(out_rest && out_rest_size) str_copy(out_rest, out_rest_size, s_esp_msg.line);
            return WaitError;
        }
    }
    return WaitTimeout;
}

static void drain_stray_messages(EspAt* esp_at) {
    for(int i = 0; i < 8; i++) {
        if(!esp_at_receive(esp_at, &s_esp_msg, 50)) break;
    }
}

static void set_progress_error(UpdaterApp* app, const char* msg) {
    furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
    app->progress_done = true;
    app->progress_ok = false;
    snprintf(app->progress_error, sizeof(app->progress_error), "%s", msg);
    furi_mutex_release(app->progress_mutex);
}

static void set_progress_done_ok(UpdaterApp* app) {
    furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
    app->progress_done = true;
    app->progress_ok = true;
    furi_mutex_release(app->progress_mutex);
}

static void add_progress_bytes(UpdaterApp* app, uint32_t n) {
    furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
    app->progress_bytes += n;
    furi_mutex_release(app->progress_mutex);
}

static void sub_progress_bytes(UpdaterApp* app, uint32_t n) {
    furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
    app->progress_bytes = (app->progress_bytes > n) ? (app->progress_bytes - n) : 0;
    furi_mutex_release(app->progress_mutex);
}

static void set_progress_total(UpdaterApp* app, uint32_t total) {
    furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
    app->progress_total = total;
    furi_mutex_release(app->progress_mutex);
}

static void add_progress_total(UpdaterApp* app, uint32_t n) {
    furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
    app->progress_total += n;
    furi_mutex_release(app->progress_mutex);
}

void updater_set_check_stage(UpdaterApp* app, const char* label, uint8_t target_pct) {
    furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
    snprintf(app->check_stage_label, sizeof(app->check_stage_label), "%s", label);
    app->check_stage_target_pct = target_pct;
    furi_mutex_release(app->progress_mutex);
}

static void run_check(UpdaterApp* app) {
    updater_set_check_stage(app, "Connecting...", 10);
    if(!github_probe(app->esp_at, 2000)) {
        set_progress_error(app, "ESP32 not responding");
        return;
    }

    updater_set_check_stage(app, "Contacting Server...", 25);
    const char* repo = (app->flow == UpdaterFlowFirmware) ? FOXFW_REPO : ESP32FW_REPO;
    bool need_assets = (app->flow == UpdaterFlowFirmware);
    if(!github_release_check(
           app->esp_at, repo, false, need_assets, &app->release, RELEASE_CHECK_TIMEOUT_MS, app)) {
        set_progress_error(app, "No response from GitHub");
        return;
    }

    updater_set_check_stage(app, "Analysing Data", 95);

    if(!app->release.ok) {
        set_progress_error(app, app->release.error[0] ? app->release.error : "Release lookup failed");
        return;
    }

    app->matched_asset = -1;
    app->matched_boot = -1;
    app->matched_part = -1;
    app->matched_fw = -1;
    app->esp32_assets_ready = false;
    bool update_available = false;

    if(app->flow == UpdaterFlowFirmware) {
        const Version* ver = furi_hal_version_get_firmware_version();
        const char* local_version = version_get_version(ver);
        str_copy(app->compare_current, sizeof(app->compare_current), local_version);
        str_copy(app->compare_latest, sizeof(app->compare_latest), app->release.tag);

        update_available = (app->release.tag[0] != '\0') &&
                           version_compare_dotted(app->release.tag, local_version) > 0;

        for(uint8_t i = 0; i < app->release.asset_count; i++) {
            size_t len = strlen(app->release.assets[i].name);
            if(len >= 4 && strcmp(app->release.assets[i].name + len - 4, ".tar") == 0) {
                app->matched_asset = (int)i;
                break;
            }
        }

        app->result = update_available ? UpdaterResultUpdateAvailable : UpdaterResultUpToDate;
        if(update_available && app->matched_asset < 0) {
            set_progress_error(app, "Update found but no .tar asset in the release");
            return;
        }
    } else {
        char rest[32] = {0};
        esp_at_send(app->esp_at, "[VERSION]");
        WaitResult wr =
            wait_for_line_prefix(app->esp_at, "[VERSION/SUCCESS]", rest, sizeof(rest), 3000);
        if(wr != WaitOk) {
            set_progress_error(app, "Could not read ESP32 version");
            return;
        }
        str_copy(app->compare_current, sizeof(app->compare_current), rest);
        str_copy(app->compare_latest, sizeof(app->compare_latest), app->release.tag);

        update_available = version_compare_dotted(app->release.tag, rest) > 0;

        const UpdaterBoard* board = &k_updater_boards[app->board_index];
        snprintf(
            app->esp32_fw_asset.name, sizeof(app->esp32_fw_asset.name), "firmware-%s.bin", board->match);
        snprintf(
            app->esp32_boot_asset.name,
            sizeof(app->esp32_boot_asset.name),
            "firmware-%s.bootloader.bin",
            board->match);
        snprintf(
            app->esp32_part_asset.name,
            sizeof(app->esp32_part_asset.name),
            "firmware-%s.partitions.bin",
            board->match);
        snprintf(
            app->esp32_fw_asset.url,
            sizeof(app->esp32_fw_asset.url),
            "https://github.com/%s/releases/download/%s/%s",
            ESP32FW_REPO,
            app->release.tag,
            app->esp32_fw_asset.name);
        snprintf(
            app->esp32_boot_asset.url,
            sizeof(app->esp32_boot_asset.url),
            "https://github.com/%s/releases/download/%s/%s",
            ESP32FW_REPO,
            app->release.tag,
            app->esp32_boot_asset.name);
        snprintf(
            app->esp32_part_asset.url,
            sizeof(app->esp32_part_asset.url),
            "https://github.com/%s/releases/download/%s/%s",
            ESP32FW_REPO,
            app->release.tag,
            app->esp32_part_asset.name);
        app->esp32_fw_asset.size = 0;
        app->esp32_boot_asset.size = 0;
        app->esp32_part_asset.size = 0;
        app->esp32_assets_ready = true;

        app->result = update_available ? UpdaterResultUpdateAvailable : UpdaterResultUpToDate;
    }

    updater_set_check_stage(app, "Analysing Data", 100);
    set_progress_done_ok(app);
}

static bool download_one(
    UpdaterApp* app,
    ReleaseAsset* asset,
    const char* dest_path,
    char* error_msg,
    size_t error_msg_size,
    uint32_t* out_bytes) {
    drain_stray_messages(app->esp_at);

    char start_cmd[UPDATER_URL_LEN + 32];
    snprintf(start_cmd, sizeof(start_cmd), "[DOWNLOAD/START]{\"url\":\"%s\"}", asset->url);
    esp_at_send(app->esp_at, start_cmd);

    char rest[64] = {0};
    if(wait_for_line_prefix(
           app->esp_at, "[DOWNLOAD/START/SUCCESS]", rest, sizeof(rest), DL_TIMEOUT_MS) != WaitOk) {
        snprintf(error_msg, error_msg_size, "Download could not start");
        return false;
    }

    uint32_t live_size = 0;
    json_mini_get_uint(rest, "size", &live_size);
    uint32_t expected_size = (live_size > 0) ? live_size : asset->size;
    if(asset->size == 0 && live_size > 0) {
        add_progress_total(app, live_size);
        asset->size = live_size;
    }

    File* file = storage_file_alloc(app->storage);
    if(!storage_file_open(file, dest_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        snprintf(error_msg, error_msg_size, "Could not create file");
        return false;
    }

    esp_at_send(app->esp_at, "[DOWNLOAD/STREAM]");
    if(wait_for_line_prefix(
           app->esp_at, "[DOWNLOAD/STREAM/BEGIN]", NULL, 0, DL_TIMEOUT_MS) != WaitOk) {
        storage_file_close(file);
        storage_file_free(file);
        storage_common_remove(app->storage, dest_path);
        snprintf(error_msg, error_msg_size, "Stream did not start");
        if(out_bytes) *out_bytes = 0;
        return false;
    }

    // No per-chunk framing and no baud switching here, by design: the ESP32
    // just pushes raw bytes with nothing between them (like FlipperHTTP's
    // proven stream() implementation), and since we already know the exact
    // expected size from DOWNLOAD/START/SUCCESS above, we read exactly that
    // many raw bytes rather than relying on a length-prefixed frame or an
    // end-of-stream marker. A single dropped byte here just costs one byte
    // at the very end (caught by the size check below), instead of
    // desyncing every frame header for the rest of the transfer.
    static uint8_t stream_buf[DL_STREAM_FRAME_MAX];
    bool success = false;
    bool cancelled = false;
    bool stream_ok = true;
    uint32_t bytes_this_file = 0;
    snprintf(error_msg, error_msg_size, "Download failed");

    esp_at_begin_raw(app->esp_at);

    while(expected_size == 0 || bytes_this_file < expected_size) {
        if(app->cancel_requested && !cancelled) {
            esp_at_send(app->esp_at, "[DOWNLOAD/CANCEL]");
            cancelled = true;
        }
        if(cancelled) break;

        size_t want = sizeof(stream_buf);
        if(expected_size > 0) {
            uint32_t remaining = expected_size - bytes_this_file;
            if(remaining < want) want = remaining;
        }

        size_t got = esp_at_read_raw(app->esp_at, stream_buf, want, DL_CHUNK_TIMEOUT_MS);
        if(got == 0) {
            if(expected_size == 0) break; // unknown size: no more data means we're done
            snprintf(error_msg, error_msg_size, "Connection timed out");
            stream_ok = false;
            break;
        }
        storage_file_write(file, stream_buf, got);
        add_progress_bytes(app, (uint32_t)got);
        bytes_this_file += (uint32_t)got;
    }

    esp_at_end_raw(app->esp_at);

    if(!stream_ok) {
        drain_stray_messages(app->esp_at);
    } else if(cancelled) {
        wait_for_line_prefix(app->esp_at, "[DOWNLOAD/CANCEL/SUCCESS]", NULL, 0, 2000);
        snprintf(error_msg, error_msg_size, "Cancelled");
    } else {
        if(!esp_at_receive(app->esp_at, &s_esp_msg, 3000)) {
            snprintf(error_msg, error_msg_size, "No response after download");
        } else if(strncmp(s_esp_msg.line, "[ERROR]", 7) == 0) {
            str_copy(error_msg, error_msg_size, s_esp_msg.line + 7);
        } else if(strcmp(s_esp_msg.line, "[DOWNLOAD/STREAM/END]") == 0) {
            if(expected_size > 0 && bytes_this_file < expected_size) {
                snprintf(
                    error_msg,
                    error_msg_size,
                    "Download incomplete (%lu of %lu bytes)",
                    (unsigned long)bytes_this_file,
                    (unsigned long)expected_size);
            } else {
                success = true;
            }
        } else {
            snprintf(error_msg, error_msg_size, "Unexpected response");
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    if(out_bytes) *out_bytes = bytes_this_file;

    if(!success) {
        storage_common_remove(app->storage, dest_path);
    }
    return success;
}

static bool download_one_retrying(
    UpdaterApp* app,
    ReleaseAsset* asset,
    const char* dest_path,
    char* error_msg,
    size_t error_msg_size) {
    for(int attempt = 0; attempt < 3; attempt++) {
        if(attempt > 0) furi_delay_ms(500);
        uint32_t bytes_written = 0;
        bool ok = download_one(app, asset, dest_path, error_msg, error_msg_size, &bytes_written);
        if(ok) return true;
        sub_progress_bytes(app, bytes_written);
        if(app->cancel_requested) return false;
    }
    return false;
}

static void run_download_firmware(UpdaterApp* app) {
    if(app->matched_asset < 0 || app->matched_asset >= app->release.asset_count) {
        set_progress_error(app, "No matching asset");
        return;
    }
    ReleaseAsset* asset = &app->release.assets[app->matched_asset];
    set_progress_total(app, asset->size);

    storage_simply_mkdir(app->storage, UPDATER_DATA_DIR);
    storage_simply_mkdir(app->storage, UPDATER_DATA_DIR "/downloads");
    snprintf(
        app->download_path, sizeof(app->download_path), "%s/downloads/%s", UPDATER_DATA_DIR, asset->name);
    snprintf(app->download_name, sizeof(app->download_name), "%s", asset->name);

    // Streaming download stays at the connection's normal baud rate rather
    // than boosting to UPDATER_FAST_BAUD - a fixed, never-renegotiated baud
    // rate is what the (known-reliable) FlipperHTTP library does, and it
    // removes a whole class of timing/desync risk during the transfer.
    char error_msg[UPDATER_STR_LEN];
    bool success = download_one_retrying(app, asset, app->download_path, error_msg, sizeof(error_msg));

    if(!success) {
        set_progress_error(app, error_msg);
        return;
    }

    update_meta_write(
        app->storage, app->download_path, app->flow, app->release.tag, app->release.commit, "", asset->size);
    set_progress_done_ok(app);
}

static void run_download_esp32(UpdaterApp* app) {
    if(!app->esp32_assets_ready) {
        set_progress_error(app, "No matching assets");
        return;
    }
    ReleaseAsset* boot = &app->esp32_boot_asset;
    ReleaseAsset* part = &app->esp32_part_asset;
    ReleaseAsset* fw = &app->esp32_fw_asset;

    set_progress_total(app, boot->size + part->size + fw->size);

    const char* board_folder = k_updater_boards[app->board_index].folder;
    storage_simply_mkdir(app->storage, UPDATER_DATA_DIR);
    storage_simply_mkdir(app->storage, UPDATER_DATA_DIR "/downloads");
    snprintf(app->download_path, sizeof(app->download_path), "%s/downloads/%s", UPDATER_DATA_DIR, board_folder);
    storage_simply_mkdir(app->storage, app->download_path);

    char path_boot[UPDATER_PATH_LEN];
    char path_part[UPDATER_PATH_LEN];
    char path_fw[UPDATER_PATH_LEN];
    str_join2(path_boot, sizeof(path_boot), app->download_path, "/bootloader.bin");
    str_join2(path_part, sizeof(path_part), app->download_path, "/partitions.bin");
    str_join2(path_fw, sizeof(path_fw), app->download_path, "/firmware.bin");

    // See run_download_firmware() - no baud boost, stay at the fixed rate.
    char error_msg[UPDATER_STR_LEN];
    snprintf(app->download_name, sizeof(app->download_name), "%s", boot->name);
    bool success = download_one_retrying(app, boot, path_boot, error_msg, sizeof(error_msg));
    if(success) {
        snprintf(app->download_name, sizeof(app->download_name), "%s", part->name);
        success = download_one_retrying(app, part, path_part, error_msg, sizeof(error_msg));
    }
    if(success) {
        snprintf(app->download_name, sizeof(app->download_name), "%s", fw->name);
        success = download_one_retrying(app, fw, path_fw, error_msg, sizeof(error_msg));
    }

    if(!success) {
        storage_common_remove(app->storage, path_boot);
        storage_common_remove(app->storage, path_part);
        storage_common_remove(app->storage, path_fw);
        set_progress_error(app, error_msg);
        return;
    }

    update_meta_write(
        app->storage,
        app->download_path,
        app->flow,
        app->release.tag,
        app->release.commit,
        board_folder,
        boot->size + part->size + fw->size);
    set_progress_done_ok(app);
}

int32_t updater_worker_thread(void* context) {
    UpdaterApp* app = context;

    furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
    app->progress_done = false;
    app->progress_ok = false;
    app->progress_bytes = 0;
    app->progress_total = 0;
    app->progress_error[0] = '\0';
    furi_mutex_release(app->progress_mutex);

    if(app->stage == UpdaterStageCheck) {
        run_check(app);
    } else if(app->flow == UpdaterFlowFirmware) {
        run_download_firmware(app);
    } else {
        run_download_esp32(app);
    }

    app->worker_running = false;
    view_dispatcher_send_custom_event(app->view_dispatcher, UpdaterEventWorkerDone);
    return 0;
}
