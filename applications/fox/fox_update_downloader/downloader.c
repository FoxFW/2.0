#include "downloader.h"
#include "github_release.h"
#include "version_util.h"
#include "update_meta.h"
#include "json_mini.h"
#include "strutil.h"
#include "installer.h"

#include <toolbox/version.h>
#include <furi_hal_version.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define DL_TIMEOUT_MS            15000
#define DL_CHUNK_POLL_SLICE_MS   150
#define RELEASE_CHECK_TIMEOUT_MS 45000
#define DL_STREAM_FRAME_MAX      1024

#define ESP32_CANCEL_SETTLE_MS 600

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

static void progress_marker_path(const char* dest_path, char* out, size_t out_size) {
    snprintf(out, out_size, "%s.progress", dest_path);
}

static void write_progress_marker(
    Storage* storage, const char* dest_path, const char* tag, uint32_t total_size) {
    char marker_path[UPDATER_PATH_LEN];
    progress_marker_path(dest_path, marker_path, sizeof(marker_path));
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, marker_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[UPDATER_STR_LEN + 16];
        int len = snprintf(buf, sizeof(buf), "%s\n%lu\n", tag, (unsigned long)total_size);
        storage_file_write(f, buf, (uint16_t)len);
    }
    storage_file_close(f);
    storage_file_free(f);
}

static bool read_progress_marker(
    Storage* storage,
    const char* dest_path,
    char* tag_out,
    size_t tag_out_size,
    uint32_t* total_out) {
    char marker_path[UPDATER_PATH_LEN];
    progress_marker_path(dest_path, marker_path, sizeof(marker_path));
    File* f = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(f, marker_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[UPDATER_STR_LEN + 16] = {0};
        uint16_t got = storage_file_read(f, buf, sizeof(buf) - 1);
        if(got > 0) {
            buf[got] = '\0';
            char* nl = strchr(buf, '\n');
            if(nl) {
                *nl = '\0';
                str_copy(tag_out, tag_out_size, buf);
                *total_out = (uint32_t)strtoul(nl + 1, NULL, 10);
                ok = true;
            }
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}

static void clear_progress_marker(Storage* storage, const char* dest_path) {
    char marker_path[UPDATER_PATH_LEN];
    progress_marker_path(dest_path, marker_path, sizeof(marker_path));
    storage_common_remove(storage, marker_path);
}

static uint32_t resumable_offset_for(UpdaterApp* app, const char* dest_path, uint32_t* out_total) {
    char marker_tag[UPDATER_STR_LEN];
    uint32_t marker_total = 0;
    if(!read_progress_marker(app->storage, dest_path, marker_tag, sizeof(marker_tag), &marker_total)) {
        return 0;
    }
    if(strcmp(marker_tag, app->release.tag) != 0) {
        clear_progress_marker(app->storage, dest_path);
        return 0;
    }
    FileInfo file_info;
    if(storage_common_stat(app->storage, dest_path, &file_info) != FSE_OK) {
        clear_progress_marker(app->storage, dest_path);
        return 0;
    }
    if(file_info.size == 0 || file_info.size >= marker_total) {
        clear_progress_marker(app->storage, dest_path);
        return 0;
    }
    *out_total = marker_total;
    return (uint32_t)file_info.size;
}

bool updater_has_partial_download(
    UpdaterApp* app,
    UpdaterFlow flow,
    char* tag_out,
    size_t tag_out_size,
    char* asset_path_out,
    size_t asset_path_out_size) {
    uint32_t total = 0;

    if(flow == UpdaterFlowFirmware) {
        char dest_path[UPDATER_PATH_LEN];
        snprintf(
            dest_path, sizeof(dest_path), "%s/downloads/%s", UPDATER_DATA_DIR, FOXFW_TAR_ASSET_NAME);
        if(read_progress_marker(app->storage, dest_path, tag_out, tag_out_size, &total)) {
            str_copy(asset_path_out, asset_path_out_size, dest_path);
            return true;
        }
        return false;
    }

    const char* board_folder = k_updater_boards[app->board_index].folder;
    static const char* k_names[3] = {"bootloader.bin", "partitions.bin", "firmware.bin"};
    for(uint8_t i = 0; i < 3; i++) {
        char dest_path[UPDATER_PATH_LEN];
        snprintf(
            dest_path,
            sizeof(dest_path),
            "%s/downloads/%s/%s",
            UPDATER_DATA_DIR,
            board_folder,
            k_names[i]);
        if(read_progress_marker(app->storage, dest_path, tag_out, tag_out_size, &total)) {
            snprintf(
                asset_path_out, asset_path_out_size, "%s/downloads/%s", UPDATER_DATA_DIR, board_folder);
            return true;
        }
    }
    return false;
}

static bool switch_esp32_baud(UpdaterApp* app, uint32_t new_baud) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "[BAUD/SET]%lu", (unsigned long)new_baud);
    esp_at_send(app->esp_at, cmd);
    char rest[16] = {0};
    if(wait_for_line_prefix(app->esp_at, "[BAUD/SET/SUCCESS]", rest, sizeof(rest), 2000) != WaitOk) {
        return false;
    }
    furi_delay_ms(50);
    esp_at_set_baud(app->esp_at, new_baud);
    furi_delay_ms(80);
    esp_at_flush_rx(app->esp_at);
    drain_stray_messages(app->esp_at);
    return true;
}

static uint32_t lower_baud_step(uint32_t baud) {
    if(baud >= 921600U) return 460800U;
    if(baud >= 460800U) return 230400U;
    return UPDATER_BAUD;
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

    if(!github_release_check(
           app->esp_at, repo, false, false, &app->release, RELEASE_CHECK_TIMEOUT_MS, app)) {
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

        ReleaseAsset* asset = &app->release.assets[0];
        snprintf(asset->name, sizeof(asset->name), "%s", FOXFW_TAR_ASSET_NAME);
        snprintf(
            asset->url,
            sizeof(asset->url),
            "https://github.com/%s/releases/download/%s/%s",
            FOXFW_REPO,
            app->release.tag,
            FOXFW_TAR_ASSET_NAME);
        asset->size = 0;
        app->release.asset_count = 1;
        app->matched_asset = 0;

        app->result = update_available ? UpdaterResultUpdateAvailable : UpdaterResultUpToDate;
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

    uint32_t marker_total = 0;
    uint32_t resume_offset = resumable_offset_for(app, dest_path, &marker_total);

    char start_cmd[UPDATER_URL_LEN + 48];
    if(resume_offset > 0) {
        snprintf(
            start_cmd,
            sizeof(start_cmd),
            "[DOWNLOAD/START]{\"url\":\"%s\",\"offset\":%lu}",
            asset->url,
            (unsigned long)resume_offset);
    } else {
        snprintf(start_cmd, sizeof(start_cmd), "[DOWNLOAD/START]{\"url\":\"%s\"}", asset->url);
    }
    esp_at_send(app->esp_at, start_cmd);

    char rest[64] = {0};
    if(wait_for_line_prefix(
           app->esp_at, "[DOWNLOAD/START/SUCCESS]", rest, sizeof(rest), DL_TIMEOUT_MS) != WaitOk) {
        snprintf(error_msg, error_msg_size, "Download could not start");
        if(out_bytes) *out_bytes = resume_offset;
        return false;
    }

    uint32_t live_size = 0;
    json_mini_get_uint(rest, "size", &live_size);

    uint32_t expected_size;
    if(resume_offset > 0) {
        expected_size = marker_total;
        uint32_t expected_remaining = marker_total - resume_offset;
        if(live_size != expected_remaining) {
            resume_offset = 0;
            clear_progress_marker(app->storage, dest_path);
            expected_size = (live_size > 0) ? live_size : asset->size;
        }
    } else {
        expected_size = (live_size > 0) ? live_size : asset->size;
    }

    if(asset->size == 0 && expected_size > 0) {
        add_progress_total(app, expected_size);
        asset->size = expected_size;
    }

    File* file = storage_file_alloc(app->storage);
    FS_OpenMode open_mode = (resume_offset > 0) ? FSOM_OPEN_APPEND : FSOM_CREATE_ALWAYS;
    if(!storage_file_open(file, dest_path, FSAM_WRITE, open_mode)) {
        storage_file_free(file);
        snprintf(error_msg, error_msg_size, "Could not create file");
        if(out_bytes) *out_bytes = 0;
        return false;
    }

    // Arm the raw-mode trigger BEFORE asking the peer to start streaming, so
    // the esp_at worker thread flips into raw mode itself the instant it
    // recognises the BEGIN line - synchronously, with no gap in which the
    // first bytes of the binary stream could be mistaken for text and lost.
    // (Previously esp_at_begin_raw() was only called after this function had
    // already woken up from wait_for_line_prefix(), which left a real race
    // window - the peer starts sending raw bytes right after the BEGIN line,
    // and the worker thread could consume several of them as "text" before
    // this thread got scheduled again, silently corrupting the start of the
    // downloaded file every time it lost the race.)
    esp_at_arm_raw_trigger(app->esp_at, "[DOWNLOAD/STREAM/BEGIN]");
    esp_at_send(app->esp_at, "[DOWNLOAD/STREAM]");
    if(wait_for_line_prefix(
           app->esp_at, "[DOWNLOAD/STREAM/BEGIN]", NULL, 0, DL_TIMEOUT_MS) != WaitOk) {
        esp_at_disarm_raw_trigger(app->esp_at);
        storage_file_close(file);
        storage_file_free(file);
        snprintf(error_msg, error_msg_size, "Stream did not start");
        if(out_bytes) *out_bytes = resume_offset;
        return false;
    }
    // By the time wait_for_line_prefix() returns WaitOk here, raw_mode is
    // already true - the worker thread set it itself while processing the
    // very same BEGIN line, before it could touch any subsequent byte.

    static uint8_t stream_buf[DL_STREAM_FRAME_MAX];
    bool success = false;
    bool cancelled = false;
    bool stream_ok = true;
    uint32_t bytes_this_file = 0;
    uint32_t chunk_timeout_ms = (uint32_t)app->settings.timeout_sec * 1000;
    uint32_t remaining_total = (expected_size > resume_offset) ? (expected_size - resume_offset) : 0;
    snprintf(error_msg, error_msg_size, "Download failed");

    while(expected_size == 0 || bytes_this_file < remaining_total) {
        if(app->cancel_requested && !cancelled) {
            esp_at_send(app->esp_at, "[DOWNLOAD/CANCEL]");
            cancelled = true;
        }
        if(cancelled) break;

        size_t want = sizeof(stream_buf);
        if(expected_size > 0) {
            uint32_t remaining = remaining_total - bytes_this_file;
            if(remaining < want) want = remaining;
        }

        size_t got = 0;
        uint32_t idle_start = furi_get_tick();
        while(true) {
            if(app->cancel_requested) break;
            uint32_t elapsed = furi_get_tick() - idle_start;
            if(elapsed >= chunk_timeout_ms) break;
            uint32_t remaining_budget = chunk_timeout_ms - elapsed;
            uint32_t slice = (remaining_budget < DL_CHUNK_POLL_SLICE_MS) ? remaining_budget :
                                                                            DL_CHUNK_POLL_SLICE_MS;
            got = esp_at_read_raw(app->esp_at, stream_buf, want, slice);
            if(got > 0) break;
        }

        if(got == 0) {
            if(app->cancel_requested) continue;
            if(expected_size == 0) break;
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

        esp_at_send(app->esp_at, "[DOWNLOAD/CANCEL]");
        wait_for_line_prefix(app->esp_at, "[DOWNLOAD/CANCEL/SUCCESS]", NULL, 0, 2000);
        drain_stray_messages(app->esp_at);
        furi_delay_ms(ESP32_CANCEL_SETTLE_MS);
    } else if(cancelled) {
        wait_for_line_prefix(app->esp_at, "[DOWNLOAD/CANCEL/SUCCESS]", NULL, 0, 2000);
        snprintf(error_msg, error_msg_size, "Cancelled");
        furi_delay_ms(ESP32_CANCEL_SETTLE_MS);
    } else if(expected_size > 0 && bytes_this_file >= remaining_total) {

        if(esp_at_receive(app->esp_at, &s_esp_msg, 3000) &&
           strncmp(s_esp_msg.line, "[ERROR]", 7) == 0) {
            str_copy(error_msg, error_msg_size, s_esp_msg.line + 7);
            str_capitalize_first(error_msg);
        } else {
            success = true;
        }
    } else {
        if(!esp_at_receive(app->esp_at, &s_esp_msg, 3000)) {
            snprintf(error_msg, error_msg_size, "No response after download");

            esp_at_send(app->esp_at, "[DOWNLOAD/CANCEL]");
            wait_for_line_prefix(app->esp_at, "[DOWNLOAD/CANCEL/SUCCESS]", NULL, 0, 2000);
            furi_delay_ms(ESP32_CANCEL_SETTLE_MS);
        } else if(strncmp(s_esp_msg.line, "[ERROR]", 7) == 0) {
            str_copy(error_msg, error_msg_size, s_esp_msg.line + 7);
            str_capitalize_first(error_msg);
        } else {
            snprintf(error_msg, error_msg_size, "Unexpected response");
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    uint32_t total_on_disk = resume_offset + bytes_this_file;
    if(out_bytes) *out_bytes = total_on_disk;

    if(total_on_disk > 0) {
        write_progress_marker(app->storage, dest_path, app->release.tag, expected_size);
    }
    return success;
}

static bool download_one_retrying(
    UpdaterApp* app,
    ReleaseAsset* asset,
    const char* dest_path,
    char* error_msg,
    size_t error_msg_size) {
    char marker_tag[UPDATER_STR_LEN];
    uint32_t marker_total = 0;
    if(read_progress_marker(app->storage, dest_path, marker_tag, sizeof(marker_tag), &marker_total) &&
       strcmp(marker_tag, app->release.tag) == 0) {
        FileInfo file_info;
        if(storage_common_stat(app->storage, dest_path, &file_info) == FSE_OK &&
           file_info.size > 0 && (uint32_t)file_info.size == marker_total) {
            if(asset->size == 0 && marker_total > 0) {
                add_progress_total(app, marker_total);
                asset->size = marker_total;
            }
            add_progress_bytes(app, marker_total);
            return true;
        }
    }

    uint32_t pre_marker_total = 0;
    uint32_t pre_existing = resumable_offset_for(app, dest_path, &pre_marker_total);
    if(pre_existing > 0) add_progress_bytes(app, pre_existing);

    uint8_t max_attempts = app->settings.retry_attempts;
    if(max_attempts == 0) max_attempts = 1;
    uint8_t lower_every = (uint8_t)((max_attempts + 3) / 4);
    if(lower_every == 0) lower_every = 1;

    uint32_t effective_baud = app->settings.baud;
    uint8_t fails = 0;

    for(uint8_t attempt = 0; attempt < max_attempts; attempt++) {
        if(attempt > 0) furi_delay_ms(500);

        bool switched = false;
        if(effective_baud != UPDATER_BAUD) {
            switched = switch_esp32_baud(app, effective_baud);
            if(!switched) effective_baud = UPDATER_BAUD;
        }

        uint32_t bytes_on_disk = 0;
        bool ok = download_one(app, asset, dest_path, error_msg, error_msg_size, &bytes_on_disk);

        if(switched) {
            switch_esp32_baud(app, UPDATER_BAUD);
        }

        if(ok) {
            if(effective_baud != app->settings.baud) {
                app->settings.baud = effective_baud;
                updater_settings_save(app);
            }
            return true;
        }

        if(app->cancel_requested) {
            return false;
        }

        fails++;
        if(app->settings.auto_lower_baud && effective_baud > UPDATER_BAUD &&
           (fails % lower_every) == 0) {
            effective_baud = lower_baud_step(effective_baud);
        }
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

    char error_msg[UPDATER_STR_LEN];
    bool success = download_one_retrying(app, asset, app->download_path, error_msg, sizeof(error_msg));

    if(!success) {
        set_progress_error(app, error_msg);
        return;
    }

    app->progress_phase = ProgressPhaseVerify;

    char validate_dir[UPDATER_PATH_LEN];
    snprintf(validate_dir, sizeof(validate_dir), "%s/validate", UPDATER_DATA_DIR);
    char manifest_path[UPDATER_PATH_LEN] = {0};
    char verify_error[UPDATER_STR_LEN];
    bool verified = installer_verify_firmware_package(
        app->storage,
        app->download_path,
        validate_dir,
        manifest_path,
        sizeof(manifest_path),
        verify_error,
        sizeof(verify_error),
        app);

    if(!verified) {
        storage_common_remove(app->storage, app->download_path);
        clear_progress_marker(app->storage, app->download_path);
        set_progress_error(app, verify_error[0] ? verify_error : "Corrupt download - try again");
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

static void run_install_firmware(UpdaterApp* app) {
    char error[UPDATER_STR_LEN];
    if(!installer_install_firmware(app, error, sizeof(error))) {
        set_progress_error(app, error);
        return;
    }
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
    } else if(app->stage == UpdaterStageInstall) {
        app->progress_phase = ProgressPhaseInstall;
        run_install_firmware(app);
    } else if(app->flow == UpdaterFlowFirmware) {
        app->progress_phase = ProgressPhaseDownload;
        run_download_firmware(app);
    } else {
        app->progress_phase = ProgressPhaseDownload;
        run_download_esp32(app);
    }

    app->worker_running = false;
    view_dispatcher_send_custom_event(app->view_dispatcher, UpdaterEventWorkerDone);
    return 0;
}
