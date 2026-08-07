#include "url_download.h"
#include "download.h"
#include "download_settings.h"
#include "download_queue.h"
#include "json_mini.h"
#include "strutil.h"

#include <storage/storage.h>
#include <string.h>
#include <stdio.h>

// Shared with download_progress_view.c, which fills the Connecting
// screen's bar based on elapsed-vs-this-budget - see app.h.
#define DL_TIMEOUT_MS            FOX_DOWNLOAD_CONNECT_TIMEOUT_MS
#define DL_CHUNK_POLL_SLICE_MS   150
#define DL_STREAM_FRAME_MAX      1024
#define ESP32_CANCEL_SETTLE_MS   600

typedef enum { WaitOk, WaitError, WaitTimeout, WaitCancelled } WaitResult;

// cancel_flag is checked every ~300ms (the same cadence esp_at_receive
// already polls at) instead of only being noticed between whole attempts
// in download_attempt_retrying - previously a Cancel press during a long
// wait (up to DL_TIMEOUT_MS, times however many retries were left) just
// sat there unacknowledged until the current wait resolved on its own.
// Pass NULL when the wait itself is part of already-cancelling cleanup,
// where a further cancel check doesn't mean anything.
static WaitResult wait_for_line_prefix(
    EspAt* esp_at,
    const char* tag,
    char* out_rest,
    size_t out_rest_size,
    uint32_t timeout_ms,
    volatile bool* cancel_flag) {
    EspAtMsg msg;
    uint32_t start = furi_get_tick();
    size_t tag_len = strlen(tag);
    while((furi_get_tick() - start) < timeout_ms) {
        if(cancel_flag && *cancel_flag) return WaitCancelled;
        if(!esp_at_receive(esp_at, &msg, 300)) continue;
        if(strncmp(msg.line, tag, tag_len) == 0) {
            if(out_rest && out_rest_size) str_copy(out_rest, out_rest_size, msg.line + tag_len);
            return WaitOk;
        }
        if(strncmp(msg.line, "[ERROR]", 7) == 0) {
            if(out_rest && out_rest_size) str_copy(out_rest, out_rest_size, msg.line);
            return WaitError;
        }
    }
    return WaitTimeout;
}

static void drain_stray_messages(EspAt* esp_at) {
    EspAtMsg msg;
    for(int i = 0; i < 8; i++) {
        if(!esp_at_receive(esp_at, &msg, 50)) break;
    }
}

static void set_progress_error(App* app, const char* msg) {
    furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
    app->download_progress_done = true;
    app->download_progress_ok = false;
    snprintf(app->download_progress_error, sizeof(app->download_progress_error), "%.63s", msg);
    furi_mutex_release(app->download_progress_mutex);
}

static void set_progress_done_ok(App* app) {
    furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
    app->download_progress_done = true;
    app->download_progress_ok = true;
    furi_mutex_release(app->download_progress_mutex);
}

static void set_progress_bytes(App* app, uint32_t n) {
    furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
    app->download_progress_bytes = n;
    furi_mutex_release(app->download_progress_mutex);
}

static void add_progress_bytes(App* app, uint32_t n) {
    furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
    app->download_progress_bytes += n;
    furi_mutex_release(app->download_progress_mutex);
}

static void set_progress_total(App* app, uint32_t total) {
    furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
    app->download_progress_total = total;
    furi_mutex_release(app->download_progress_mutex);
}

static void set_progress_attempt(App* app, uint8_t attempt, uint8_t max_attempts) {
    furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
    app->download_progress_attempt = attempt;
    app->download_progress_max_attempts = max_attempts;
    furi_mutex_release(app->download_progress_mutex);
}

static bool switch_esp32_baud(App* app, uint32_t new_baud) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "[BAUD/SET]%lu", (unsigned long)new_baud);
    esp_at_send(app->esp_at, cmd);
    char rest[16] = {0};
    if(wait_for_line_prefix(
           app->esp_at, "[BAUD/SET/SUCCESS]", rest, sizeof(rest), 2000, &app->download_cancel_requested) !=
       WaitOk) {
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
    return FOX_DOWNLOAD_BAUD;
}

void download_work_path(const char* final_path, char* out, size_t out_size) {
    snprintf(out, out_size, "%s.download", final_path);
}

// Reads exactly `len` raw bytes, polling in DL_CHUNK_POLL_SLICE_MS slices
// so a cancel request lands within one slice instead of waiting out the
// whole chunk_timeout_ms budget. Returns bytes actually read - short of
// `len` means either cancelled or the idle budget ran out with nothing
// (more) arriving.
static size_t
    read_raw_exact(App* app, uint8_t* out, size_t len, uint32_t chunk_timeout_ms) {
    size_t got = 0;
    uint32_t idle_start = furi_get_tick();
    while(got < len) {
        if(app->download_cancel_requested) break;
        uint32_t elapsed = furi_get_tick() - idle_start;
        if(elapsed >= chunk_timeout_ms) break;
        uint32_t remaining_budget = chunk_timeout_ms - elapsed;
        uint32_t slice =
            (remaining_budget < DL_CHUNK_POLL_SLICE_MS) ? remaining_budget : DL_CHUNK_POLL_SLICE_MS;
        got += esp_at_read_raw(app->esp_at, out + got, len - got, slice);
    }
    return got;
}

static bool download_attempt(App* app, char* error_msg, size_t error_msg_size) {
    drain_stray_messages(app->esp_at);
    app->download_connect_attempt_tick = furi_get_tick();

    // A brand new download doesn't know its own destination filename/path
    // until the very first successful connect tells us the URL is real
    // (download_derive_found_info below) - and by definition there's
    // nothing to resume yet on that first attempt. Once a path exists -
    // this same download's later retries, or a resumed interrupted
    // download that already has one from before - the usual on-disk
    // resume-offset detection applies as normal.
    bool have_path = (app->download_found_name[0] != '\0');

    char work_path[FOX_DOWNLOAD_PATH_MAX + 10];
    Storage* storage = furi_record_open(RECORD_STORAGE);
    uint32_t resume_offset = 0;
    uint32_t marker_total = 0;

    if(have_path) {
        download_work_path(app->download_path, work_path, sizeof(work_path));

        FileInfo work_info;
        if(storage_common_stat(storage, work_path, &work_info) == FSE_OK && work_info.size > 0) {
            resume_offset = (uint32_t)work_info.size;
        }

        if(resume_offset > 0) {
            char marker_url[FOX_TEXT_INPUT_BUFFER_MAX];
            char marker_path[FOX_DOWNLOAD_PATH_MAX];
            if(!download_state_load(
                   marker_url, sizeof(marker_url), marker_path, sizeof(marker_path), &marker_total) ||
               marker_total == 0 || strcmp(marker_url, app->download_url) != 0 ||
               strcmp(marker_path, app->download_path) != 0) {
                resume_offset = 0;
            }
        }
    }

    char start_cmd[FOX_TEXT_INPUT_BUFFER_MAX + 48];
    if(resume_offset > 0) {
        snprintf(
            start_cmd,
            sizeof(start_cmd),
            "[DOWNLOAD/START]{\"url\":\"%s\",\"offset\":%lu}",
            app->download_url,
            (unsigned long)resume_offset);
    } else {
        snprintf(start_cmd, sizeof(start_cmd), "[DOWNLOAD/START]{\"url\":\"%s\"}", app->download_url);
    }
    esp_at_send(app->esp_at, start_cmd);

    char rest[80] = {0};
    WaitResult start_wait = wait_for_line_prefix(
        app->esp_at,
        "[DOWNLOAD/START/SUCCESS]",
        rest,
        sizeof(rest),
        DL_TIMEOUT_MS,
        &app->download_cancel_requested);
    if(start_wait != WaitOk) {
        furi_record_close(RECORD_STORAGE);
        if(start_wait == WaitCancelled) {
            snprintf(error_msg, error_msg_size, "Cancelled");
        } else if(app->download_purpose == DownloadPurposeCatalogInstall && strstr(rest, "code=404")) {
            snprintf(error_msg, error_msg_size, "App's catalog API doesn't match firmware");
        } else {
            snprintf(error_msg, error_msg_size, "Download could not start");
        }
        return false;
    }

    app->download_connected = true;

    uint32_t live_size = 0;
    json_mini_get_uint(rest, "size", &live_size);

    if(!have_path) {
        char type[FOX_DOWNLOAD_TYPE_MAX] = "";
        json_mini_get_string(rest, "type", type, sizeof(type));
        download_derive_found_info(app, live_size, type);
        download_work_path(app->download_path, work_path, sizeof(work_path));
        // A brand new download, by definition, has nothing to resume yet.
        resume_offset = 0;
    }

    uint32_t expected_size;
    if(resume_offset > 0) {
        uint32_t expected_remaining = (marker_total > resume_offset) ? marker_total - resume_offset : 0;
        if(live_size != expected_remaining) {
            resume_offset = 0;
            expected_size = live_size;
        } else {
            expected_size = marker_total;
        }
    } else {
        expected_size = live_size;
    }
    if(expected_size > 0) {
        set_progress_total(app, expected_size);
        download_state_set_total(expected_size);
    }
    set_progress_bytes(app, resume_offset);

    File* file = storage_file_alloc(storage);
    FS_OpenMode open_mode = (resume_offset > 0) ? FSOM_OPEN_APPEND : FSOM_CREATE_ALWAYS;
    if(!storage_file_open(file, work_path, FSAM_WRITE, open_mode)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        snprintf(error_msg, error_msg_size, "Could not create file");
        return false;
    }

    esp_at_arm_raw_trigger(app->esp_at, "[DOWNLOAD/STREAM/BEGIN]");
    esp_at_send(app->esp_at, "[DOWNLOAD/STREAM]");
    WaitResult stream_wait = wait_for_line_prefix(
        app->esp_at,
        "[DOWNLOAD/STREAM/BEGIN]",
        NULL,
        0,
        DL_TIMEOUT_MS,
        &app->download_cancel_requested);
    if(stream_wait != WaitOk) {
        esp_at_disarm_raw_trigger(app->esp_at);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        snprintf(
            error_msg, error_msg_size, stream_wait == WaitCancelled ? "Cancelled" : "Stream did not start");
        return false;
    }

    static uint8_t stream_buf[DL_STREAM_FRAME_MAX];
    bool success = false;
    bool cancelled = false;
    bool stream_ok = true;
    bool got_terminator = false;
    uint32_t chunk_timeout_ms = (uint32_t)app->download_settings.timeout_sec * 1000;
    snprintf(error_msg, error_msg_size, "Download failed");

    // Every chunk on the wire from here is a 4-byte little-endian length
    // prefix followed by that many payload bytes, ending in a 4-byte
    // terminator (0 = finished cleanly, all-ones = the ESP32 gave up
    // mid-transfer - see DOWNLOAD_STREAM_END_MARKER/ERROR_MARKER in
    // http_bridge.cpp). Previously this side just counted bytes against
    // the size the ESP32 reported up front and had no way to tell a
    // stalled transfer from a slow one - a stall just looked like more
    // waiting, right up until this side's own separate, much longer
    // timeout finally gave up, and any diagnostic text the ESP32 tried to
    // print in the meantime got swallowed into the saved file as garbage
    // since this side was still treating everything as raw payload.
    while(true) {
        if(app->download_cancel_requested && !cancelled) {
            esp_at_send(app->esp_at, "[DOWNLOAD/CANCEL]");
            cancelled = true;
        }
        if(cancelled) break;

        uint8_t len_buf[4];
        size_t len_got = read_raw_exact(app, len_buf, sizeof(len_buf), chunk_timeout_ms);
        if(app->download_cancel_requested && len_got < sizeof(len_buf)) continue;
        if(len_got < sizeof(len_buf)) {
            snprintf(error_msg, error_msg_size, "Connection timed out");
            stream_ok = false;
            break;
        }

        uint32_t frame_len;
        memcpy(&frame_len, len_buf, sizeof(frame_len));

        if(frame_len == 0) {
            got_terminator = true;
            success = true;
            break;
        }
        if(frame_len == 0xFFFFFFFFUL) {
            got_terminator = true;
            stream_ok = false;
            break;
        }
        if(frame_len > sizeof(stream_buf)) {
            // The ESP32 never frames more than DL_STREAM_FRAME_MAX bytes
            // at a time - a corrupted/desynced length prefix reading as
            // something huge here would otherwise wait forever for bytes
            // that don't exist.
            snprintf(error_msg, error_msg_size, "Stream protocol error");
            stream_ok = false;
            break;
        }

        size_t chunk_got = read_raw_exact(app, stream_buf, frame_len, chunk_timeout_ms);
        if(app->download_cancel_requested && chunk_got < frame_len) continue;
        if(chunk_got < frame_len) {
            snprintf(error_msg, error_msg_size, "Connection timed out");
            stream_ok = false;
            break;
        }

        storage_file_write(file, stream_buf, chunk_got);
        add_progress_bytes(app, (uint32_t)chunk_got);
    }

    esp_at_end_raw(app->esp_at);

    EspAtMsg s_msg;
    if(cancelled) {
        wait_for_line_prefix(app->esp_at, "[DOWNLOAD/CANCEL/SUCCESS]", NULL, 0, 2000, NULL);
        snprintf(error_msg, error_msg_size, "Cancelled");
        furi_delay_ms(ESP32_CANCEL_SETTLE_MS);
    } else if(!stream_ok) {
        if(got_terminator) {
            // The ESP32 already knows it's over and is printing its own
            // [ERROR] line right behind the terminator - read that
            // instead of a generic message so the real reason (e.g.
            // "stream incomplete") reaches the user.
            if(esp_at_receive(app->esp_at, &s_msg, 3000) && strncmp(s_msg.line, "[ERROR]", 7) == 0) {
                str_copy(error_msg, error_msg_size, s_msg.line + 7);
                str_capitalize_first(error_msg);
            }
        } else {
            // Never got a terminator at all (protocol desync, or the
            // ESP32 itself is wedged) - tell it to give up rather than
            // leaving it mid-stream for the next attempt to trip over.
            esp_at_send(app->esp_at, "[DOWNLOAD/CANCEL]");
            wait_for_line_prefix(app->esp_at, "[DOWNLOAD/CANCEL/SUCCESS]", NULL, 0, 2000, NULL);
        }
        drain_stray_messages(app->esp_at);
        furi_delay_ms(ESP32_CANCEL_SETTLE_MS);
    } else if(esp_at_receive(app->esp_at, &s_msg, 3000) && strncmp(s_msg.line, "[ERROR]", 7) == 0) {
        // The clean-finish terminator raced an error the ESP32 only
        // detected after sending it (rare, but the trailing line is the
        // tie-breaker) - trust the line over the terminator.
        str_copy(error_msg, error_msg_size, s_msg.line + 7);
        str_capitalize_first(error_msg);
        success = false;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return success;
}

static bool download_attempt_retrying(App* app, char* error_msg, size_t error_msg_size) {
    uint8_t max_attempts = app->download_settings.retry_attempts;
    if(max_attempts == 0) max_attempts = 1;
    uint8_t lower_every = (uint8_t)((max_attempts + 3) / 4);
    if(lower_every == 0) lower_every = 1;

    uint32_t effective_baud = app->download_settings.baud;
    uint8_t fails = 0;

    for(uint8_t attempt = 0; attempt < max_attempts; attempt++) {
        set_progress_attempt(app, attempt + 1, max_attempts);
        if(attempt > 0) furi_delay_ms(500);
        if(app->download_cancel_requested) {
            snprintf(error_msg, error_msg_size, "Cancelled");
            return false;
        }

        bool switched = false;
        if(effective_baud != FOX_DOWNLOAD_BAUD) {
            switched = switch_esp32_baud(app, effective_baud);
            if(!switched) effective_baud = FOX_DOWNLOAD_BAUD;
        }

        bool ok = download_attempt(app, error_msg, error_msg_size);

        if(switched) {
            switch_esp32_baud(app, FOX_DOWNLOAD_BAUD);
        }

        if(ok) {
            if(effective_baud != app->download_settings.baud) {
                app->download_settings.baud = effective_baud;
                download_settings_save(app);
            }
            return true;
        }

        if(app->download_cancel_requested) {
            snprintf(error_msg, error_msg_size, "Cancelled");
            return false;
        }

        fails++;
        if(app->download_settings.auto_lower_baud && effective_baud > FOX_DOWNLOAD_BAUD &&
           (fails % lower_every) == 0) {
            effective_baud = lower_baud_step(effective_baud);
        }
    }
    return false;
}

int32_t download_worker_thread(void* context) {
    App* app = context;

    furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
    app->download_progress_done = false;
    app->download_progress_ok = false;
    app->download_progress_bytes = 0;
    app->download_progress_total = 0;
    app->download_progress_attempt = 0;
    app->download_progress_max_attempts = 0;
    app->download_progress_error[0] = '\0';
    furi_mutex_release(app->download_progress_mutex);
    app->download_connected = false;

    char error_msg[FOX_DOWNLOAD_ERR_MAX];
    bool success = download_attempt_retrying(app, error_msg, sizeof(error_msg));

    if(!success) {
        set_progress_error(app, error_msg);
    } else {
        // Every attempt opens and writes a file now (there's no more
        // check-only path that skips straight to a cancel) - always
        // rename the finished .download file into place on success. If
        // this was never confirmed by the user, download_unconfirmed_finished
        // (download.c, called from main.c) still owns the decision of
        // whether to keep it or delete it - it just does that at the
        // final path instead of the .download one.
        char work_path[FOX_DOWNLOAD_PATH_MAX + 10];
        download_work_path(app->download_path, work_path, sizeof(work_path));
        Storage* storage = furi_record_open(RECORD_STORAGE);
        storage_common_rename(storage, work_path, app->download_path);
        furi_record_close(RECORD_STORAGE);
        set_progress_done_ok(app);
    }

    app->download_worker_running = false;
    view_dispatcher_send_custom_event(app->view_dispatcher, FOX_DOWNLOAD_EVENT_WORKER_DONE);
    return 0;
}

void download_worker_free_if_done(App* app) {
    if(app->download_worker) {
        furi_thread_join(app->download_worker);
        furi_thread_free(app->download_worker);
        app->download_worker = NULL;
    }
}

void download_start_worker(App* app) {
    download_worker_free_if_done(app);
    app->download_cancel_requested = false;
    app->download_worker_running = true;
    app->download_worker =
        furi_thread_alloc_ex("FoxDownloadWorker", 4096, download_worker_thread, app);
    furi_thread_start(app->download_worker);
}
