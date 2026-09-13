#include "foxr_companion.h"

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <input/input_settings.h>
#include <storage/storage.h>
#include <notification/notification_app.h>
#include <locale/locale.h>
#include <gui/modules/fox_theme.h>
#include <power/power_service/power.h>
#include <bt/bt_service/bt.h>
#include <bt/bt_service/bt_settings_api_i.h>
#include <desktop/desktop.h>
#include <cli/cli_settings.h>
#include <flipper_format/flipper_format.h>
#include <applications/services/namechanger/namechanger.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

/* FoxLAB Companion - see foxr_companion.h for the overview. This file is
 * the whole Fox Remote ("FLPR") command surface: device/power/storage
 * info, file browsing, screen streaming, remote input, clock read/write,
 * reboot, and (task #54) notification/locale/menu-theme settings, all
 * built directly on this firmware's Storage/Gui/Input/Notification/Locale
 * APIs, answering plain
 * "[FLPR/...]" text lines relayed over from the browser by the ESP32 -
 * see the Fox Remote Protocol design doc for the full wire format and the
 * reasoning behind it (in particular: why this replaces the stock
 * Expansion/RPC protocol entirely, rather than trying to fix it further).
 *
 * Deliberately NOT used here: rpc_gui_screen_suppress.h's RAM-protection
 * hooks (confirmed present in targets/f7/api_symbols.csv, so linking
 * against them would be safe). They're skipped anyway because they'd be a
 * no-op for this app: rpc_gui_screen_stream_is_suppressed() only ever goes
 * true while some OTHER app (SubGhz Read) is both running AND has called
 * rpc_gui_screen_stream_set_suppressed(true) - but the Flipper OS only
 * ever runs one app at a time, so SubGhz Read can't be doing that at any
 * moment fox_lab's own framebuffer callback could possibly fire. There's
 * no scenario where this app's screen stream and SubGhz Read's RAM
 * pressure coexist.
 */

#define FOXR_MAX_NAME_LEN     255
#define FOXR_READ_CHUNK_RAW   900
#define FOXR_WRITE_CHUNK_RAW  400 /* decode buffer - matches the protocol's ~360-byte chunk budget with margin */
#define FOXR_SCREEN_MIN_INTERVAL_MS 500 /* throttle - the Flipper's screen can redraw far faster than this line-oriented UART protocol needs to keep up with for a mirror; capping cuts UART/CPU load during streaming */

/* Persistent activity log on the SD card - every command this companion
 * handles gets written here, in full, regardless of what the 6-line
 * in-memory ring buffer (foxr_companion_log_snapshot(), shown on the
 * FoxLAB Active screen - flpr_view.c) is currently able to display. The
 * on-screen view only ever shows the last few entries; this file is the
 * complete, unfiltered history for later review off the device. Two lines
 * per command, written together: a short "Terminal" line (exactly what
 * the on-screen log shows) and a longer "TextFile" line with everything
 * that's useful for debugging after the fact - a timestamp, the full raw
 * command as received, the full reply as sent, and a heap snapshot
 * (current free, total, and the all-time-low watermark since boot). */
#define FOXR_LOG_DIR  EXT_PATH("apps_data/fox_lab")
#define FOXR_LOG_FILE EXT_PATH("apps_data/fox_lab/flpr_log.txt")
/* Longest real reply that needs to fit uncut: [FLPR/INFO/OK]'s
 * hardware_name=<up to 32>|firmware_version=<up to 32> can run to ~110
 * chars on its own; 160 leaves real margin without being wasteful (this
 * buffer lives in FoxrCompanion, one instance per app session). */
#define FOXR_LAST_REPLY_MAX 160

typedef enum {
    FoxrScreenFlagTransmit = 1u << 0,
    FoxrScreenFlagExit = 1u << 1,
} FoxrScreenFlag;
#define FoxrScreenFlagAny (FoxrScreenFlagTransmit | FoxrScreenFlagExit)

struct FoxrCompanion {
    EspAt* esp_at;
    Gui* gui;
    Storage* storage;
    FuriPubSub* input_events;

    /* Activity log ring buffer for the FoxLAB Companion screen. */
    FuriMutex* log_mutex;
    char log_lines[FOXR_LOG_LINES][FOXR_LOG_LINE_MAX];
    size_t log_count;
    size_t log_next;
    uint32_t log_version;

    /* Most recent line actually sent back over the UART (via foxr_reply()/
     * foxr_replyf() below) - captured so the end-of-dispatch log entry can
     * report what actually happened, not just what was asked for. For a
     * multi-line reply (e.g. FILES/LIST's ITEM lines followed by one final
     * OK/ERR), this naturally ends up holding that last, outcome-bearing
     * line by the time dispatch finishes. */
    char last_reply[FOXR_LAST_REPLY_MAX];

    /* Write-session state - persists across separate WRITE/START, /CHUNK,
     * /END dispatch calls, unlike reads which are handled fully
     * synchronously within a single dispatch call. */
    File* write_file;
    bool write_active;

    /* Screen-stream state. */
    bool screen_active;
    FuriThread* screen_thread;
    uint8_t* screen_frame_buf;
    size_t screen_frame_size;
    char* screen_line_buf;
    size_t screen_b64_capacity; /* capacity of screen_line_buf *after* the "[FLPR/SCREEN/FRAME]" prefix */

    /* Input held-key bookkeeping - mirrors the stock RPC layer's own
     * (rpc_gui.c's RpcGuiSystem::input_key_counter/input_counter). */
    uint32_t input_key_counter[InputKeyMAX];
    uint32_t input_counter;

    /* Owned by App (app.h) - see foxr_companion_alloc()'s param comment in
     * the header. May be NULL (nothing to flag). */
    volatile bool* restart_pending_flag;
};

/* ── Base64 (self-contained - no dependency on an unverified toolbox header) ── */

static const char kFoxrB64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t foxr_b64_encode(const uint8_t* in, size_t in_len, char* out, size_t out_capacity) {
    size_t out_len = 0;
    size_t i = 0;
    while(i + 3 <= in_len) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | in[i + 2];
        if(out_len + 4 > out_capacity) return out_len;
        out[out_len++] = kFoxrB64Alphabet[(v >> 18) & 0x3F];
        out[out_len++] = kFoxrB64Alphabet[(v >> 12) & 0x3F];
        out[out_len++] = kFoxrB64Alphabet[(v >> 6) & 0x3F];
        out[out_len++] = kFoxrB64Alphabet[v & 0x3F];
        i += 3;
    }
    size_t rem = in_len - i;
    if(rem == 1 && out_len + 4 <= out_capacity) {
        uint32_t v = (uint32_t)in[i] << 16;
        out[out_len++] = kFoxrB64Alphabet[(v >> 18) & 0x3F];
        out[out_len++] = kFoxrB64Alphabet[(v >> 12) & 0x3F];
        out[out_len++] = '=';
        out[out_len++] = '=';
    } else if(rem == 2 && out_len + 4 <= out_capacity) {
        uint32_t v = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8);
        out[out_len++] = kFoxrB64Alphabet[(v >> 18) & 0x3F];
        out[out_len++] = kFoxrB64Alphabet[(v >> 12) & 0x3F];
        out[out_len++] = kFoxrB64Alphabet[(v >> 6) & 0x3F];
        out[out_len++] = '=';
    }
    if(out_len < out_capacity) out[out_len] = '\0';
    return out_len;
}

static int foxr_b64_val(char c) {
    if(c >= 'A' && c <= 'Z') return c - 'A';
    if(c >= 'a' && c <= 'z') return c - 'a' + 26;
    if(c >= '0' && c <= '9') return c - '0' + 52;
    if(c == '+') return 62;
    if(c == '/') return 63;
    return -1;
}

static size_t foxr_b64_decode(const char* in, size_t in_len, uint8_t* out, size_t out_capacity) {
    size_t out_len = 0;
    uint32_t buf = 0;
    int bits = 0;
    for(size_t i = 0; i < in_len; i++) {
        char c = in[i];
        if(c == '=' || c == '\0') break;
        int v = foxr_b64_val(c);
        if(v < 0) continue;
        buf = (buf << 6) | (uint32_t)v;
        bits += 6;
        if(bits >= 8) {
            bits -= 8;
            if(out_len < out_capacity) out[out_len] = (uint8_t)((buf >> bits) & 0xFF);
            out_len++;
        }
    }
    return out_len;
}

/* Formats a uint64_t as a plain decimal string, self-contained rather than
 * relying on vsnprintf's %llu - see foxr_handle_storage()'s comment for why
 * this file doesn't trust that conversion on this toolchain. `out_capacity`
 * should be at least 21 (20 digits + NUL, uint64_t's max) for any value to
 * come through untruncated; every call site below uses 24. */
static void foxr_u64_to_str(uint64_t value, char* out, size_t out_capacity) {
    char digits[20]; /* UINT64_MAX is 20 decimal digits, no NUL needed here */
    size_t n = 0;
    if(value == 0) {
        digits[n++] = '0';
    } else {
        while(value > 0 && n < sizeof(digits)) {
            digits[n++] = (char)('0' + (value % 10));
            value /= 10;
        }
    }
    size_t out_len = (n < out_capacity - 1) ? n : out_capacity - 1;
    for(size_t i = 0; i < out_len; i++) {
        out[i] = digits[n - 1 - i];
    }
    out[out_len] = '\0';
}

/* ── Reply helpers ─────────────────────────────────────────────────── */

static void foxr_capture_last_reply(FoxrCompanion* c, const char* text) {
    strncpy(c->last_reply, text, sizeof(c->last_reply) - 1);
    c->last_reply[sizeof(c->last_reply) - 1] = '\0';
}

static void foxr_reply(FoxrCompanion* c, const char* text) {
    foxr_capture_last_reply(c, text);
    esp_at_send(c->esp_at, text);
}

static void foxr_replyf(FoxrCompanion* c, const char* fmt, ...) {
    char buf[600];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    foxr_capture_last_reply(c, buf);
    esp_at_send(c->esp_at, buf);
}

static const char* foxr_fs_error_code(FS_Error err) {
    switch(err) {
    case FSE_OK:
        return "OK";
    case FSE_NOT_EXIST:
        return "NOTFOUND";
    case FSE_EXIST:
        return "EXIST";
    case FSE_INVALID_PARAMETER:
        return "INVALID";
    case FSE_DENIED:
        return "DENIED";
    case FSE_INVALID_NAME:
        return "INVALID";
    case FSE_INTERNAL:
        return "INTERNAL";
    case FSE_NOT_READY:
        return "NOTREADY";
    case FSE_ALREADY_OPEN:
        return "ALREADYOPEN";
    case FSE_NOT_IMPLEMENTED:
        return "NOTIMPL";
    default:
        return "ERROR";
    }
}

/* ── Activity log ──────────────────────────────────────────────────── */

static void foxr_log_add(FoxrCompanion* c, const char* text) {
    furi_mutex_acquire(c->log_mutex, FuriWaitForever);
    strncpy(c->log_lines[c->log_next], text, FOXR_LOG_LINE_MAX - 1);
    c->log_lines[c->log_next][FOXR_LOG_LINE_MAX - 1] = '\0';
    c->log_next = (c->log_next + 1) % FOXR_LOG_LINES;
    if(c->log_count < FOXR_LOG_LINES) c->log_count++;
    c->log_version++;
    furi_mutex_release(c->log_mutex);
}

uint32_t foxr_companion_log_version(FoxrCompanion* c) {
    return c->log_version;
}

void foxr_companion_log_snapshot(
    FoxrCompanion* c,
    char out[FOXR_LOG_LINES][FOXR_LOG_LINE_MAX],
    size_t* out_count) {
    furi_mutex_acquire(c->log_mutex, FuriWaitForever);
    size_t count = c->log_count;
    size_t start = (c->log_next + FOXR_LOG_LINES - count) % FOXR_LOG_LINES;
    for(size_t i = 0; i < count; i++) {
        size_t idx = (start + i) % FOXR_LOG_LINES;
        strncpy(out[i], c->log_lines[idx], FOXR_LOG_LINE_MAX - 1);
        out[i][FOXR_LOG_LINE_MAX - 1] = '\0';
    }
    *out_count = count;
    furi_mutex_release(c->log_mutex);
}

/* Appends both log lines for one command to the SD card log file
 * (FOXR_LOG_FILE). Opened, written, and closed fresh every call rather
 * than kept open for the session, so a completed entry is always actually
 * on the card (not sitting in an FS write cache) even if the app is later
 * killed uncleanly - one command per call here, not the screen-stream
 * frame rate, so the extra open/close overhead is a non-issue. Silently
 * does nothing if the card is missing/unmounted or the write fails - the
 * in-memory ring buffer (foxr_log_add() above) still works regardless, so
 * the on-screen log is never affected by SD card problems. */
static void foxr_log_persist(
    FoxrCompanion* c,
    const char* terminal_line,
    const char* request_line,
    const char* reply_line) {
    File* file = storage_file_alloc(c->storage);
    if(!storage_file_open(file, FOXR_LOG_FILE, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_free(file);
        return;
    }

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char line1[FOXR_LOG_LINE_MAX + 16];
    snprintf(line1, sizeof(line1), "LOG:Terminal:%s\r\n", terminal_line);
    storage_file_write(file, line1, strlen(line1));

    /* "free" and everything else useful for debugging after the fact,
     * per the user's request: a real timestamp, the exact request and
     * reply lines, and a heap snapshot (current free / total, plus the
     * all-time-low watermark since boot - the single most useful number
     * for tracking down a slow leak or a RAM ceiling like the one
     * subghz_garage runs close to). request_line can be as long as a full
     * FLPR line (ESP_AT_LINE_MAX, currently 512) - malloc'd rather than a
     * stack buffer since this whole call chain runs on the router's own
     * thread (esp_at_router.c), which has a deliberately small stack. */
    size_t line2_cap = ESP_AT_LINE_MAX + FOXR_LAST_REPLY_MAX + 128;
    char* line2 = malloc(line2_cap);
    snprintf(
        line2,
        line2_cap,
        "LOG:TextFile:[%04u-%02u-%02u %02u:%02u:%02u] req=%s reply=%s "
        "free=%zu total=%zu min_free=%zu\r\n",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second,
        request_line,
        reply_line,
        memmgr_get_free_heap(),
        memmgr_get_total_heap(),
        memmgr_get_minimum_free_heap());
    storage_file_write(file, line2, strlen(line2));
    free(line2);

    storage_file_close(file);
    storage_file_free(file);
}

/* Called once per dispatched [FLPR/...] command, after it's been fully
 * handled - see foxr_companion_dispatch() below. Logs both to the
 * in-memory ring buffer (what the FoxLAB Active screen shows live) and to
 * the persistent SD card file (the complete, unfiltered history) in one
 * call, so every single command that comes through is captured in both
 * places the same way - nothing added to the on-screen log without also
 * landing in the file, and nothing skipped just because the screen only
 * has room for a few lines. */
static void foxr_log_event(FoxrCompanion* c, const char* tag, const char* request_line) {
    const char* status = "?";
    if(strstr(c->last_reply, "/OK") != NULL) {
        status = "OK";
    } else if(strstr(c->last_reply, "/ERR]") != NULL) {
        status = "ERR";
    }

    /* Screen is narrow (FOXR_LOG_LINE_MAX=40) - drop the "FLPR/" prefix,
     * every logged tag has it, so it's implied rather than repeated on
     * every line. */
    const char* short_tag = (strncmp(tag, "FLPR/", 5) == 0) ? tag + 5 : tag;
    char terminal_line[FOXR_LOG_LINE_MAX];
    snprintf(terminal_line, sizeof(terminal_line), "%s %s", short_tag, status);

    foxr_log_add(c, terminal_line);
    foxr_log_persist(c, terminal_line, request_line, c->last_reply);
}

/* ── Device / power / storage info ────────────────────────────────── */

typedef struct {
    char hardware_name[32];
    char firmware_version[32];
    bool got_hw;
    bool got_fw;
} FoxrInfoCapture;

static void foxr_info_capture_cb(const char* key, const char* value, bool last, void* context) {
    UNUSED(last);
    FoxrInfoCapture* cap = context;
    if(strcmp(key, "hardware_name") == 0) {
        snprintf(cap->hardware_name, sizeof(cap->hardware_name), "%s", value);
        cap->got_hw = true;
    } else if(strcmp(key, "firmware_version") == 0) {
        snprintf(cap->firmware_version, sizeof(cap->firmware_version), "%s", value);
        cap->got_fw = true;
    }
}

static void foxr_handle_info(FoxrCompanion* c) {
    FoxrInfoCapture cap = {0};
    furi_hal_info_get(foxr_info_capture_cb, '_', &cap);
    if(!cap.got_hw) snprintf(cap.hardware_name, sizeof(cap.hardware_name), "Flipper");
    if(!cap.got_fw) snprintf(cap.firmware_version, sizeof(cap.firmware_version), "unknown");
    /* Defensive: a '|' in either value would break the reply's own field
     * separator - shouldn't happen for these two keys, but cheap to guard. */
    for(char* p = cap.hardware_name; *p; p++)
        if(*p == '|') *p = '_';
    for(char* p = cap.firmware_version; *p; p++)
        if(*p == '|') *p = '_';
    foxr_replyf(
        c,
        "[FLPR/INFO/OK]hardware_name=%s|firmware_version=%s",
        cap.hardware_name,
        cap.firmware_version);
}

typedef struct {
    char charge_level[16];
    bool got;
} FoxrPowerCapture;

static void foxr_power_capture_cb(const char* key, const char* value, bool last, void* context) {
    UNUSED(last);
    FoxrPowerCapture* cap = context;
    if(strcmp(key, "charge_level") == 0) {
        snprintf(cap->charge_level, sizeof(cap->charge_level), "%s", value);
        cap->got = true;
    }
}

static void foxr_handle_power(FoxrCompanion* c) {
    FoxrPowerCapture cap = {0};
    furi_hal_power_info_get(foxr_power_capture_cb, '_', &cap);
    if(!cap.got) snprintf(cap.charge_level, sizeof(cap.charge_level), "0");
    foxr_replyf(c, "[FLPR/POWER/OK]charge_level=%s", cap.charge_level);
}

static void foxr_handle_storage(FoxrCompanion* c, const char* path) {
    uint64_t total = 0, free_space = 0;
    FS_Error err = storage_common_fs_info(c->storage, path, &total, &free_space);
    if(err == FSE_OK) {
        /* Sent as KiB (uint32_t via %lu), not raw bytes (uint64_t via
         * %llu) - confirmed real-hardware symptom 2026-09-11: both SD and
         * Internal storage tiles showed a flat 0% used regardless of
         * actual free space (SD reported ~99% free by qFlipper over USB,
         * Internal reportedly down to ~40KB free) - consistent with a
         * 64-bit printf value coming through as 0 rather than a percentage-
         * math bug (the browser's usedPct calc is straightforward and was
         * double-checked). This firmware's own on-device Storage settings
         * screens hit the exact same 64-bit-value concern and both sidestep
         * it the same way: storage_settings_scene_internal_info.c casts to
         * `(uint32_t)(total_space / 1024)` + %lu, and storage_settings_
         * scene_sd_info.c's SDInfo struct stores kb_total/kb_free as %lu
         * from the start - neither uses %llu anywhere. Matching that
         * established, shipped convention here rather than trying to prove
         * or disprove 64-bit printf support on this toolchain directly. A
         * KiB count fits a uint32_t up into the TiB range, comfortably
         * beyond any Flipper storage size - getStorageInfo() on the browser
         * side multiplies back by 1024, so nothing downstream needs to know
         * the wire units changed. */
        foxr_replyf(
            c,
            "[FLPR/STORAGE/OK]%lu,%lu",
            (unsigned long)(total / 1024),
            (unsigned long)(free_space / 1024));
    } else {
        foxr_replyf(c, "[FLPR/STORAGE/ERR]%s", foxr_fs_error_code(err));
    }
}

/* ── Files ─────────────────────────────────────────────────────────── */

static void foxr_handle_files_list(FoxrCompanion* c, const char* path) {
    if(strcmp(path, "/") == 0) {
        static const char* const roots[] = {"any", "int", "ext"};
        for(size_t i = 0; i < COUNT_OF(roots); i++) {
            char name_b64[16];
            foxr_b64_encode((const uint8_t*)roots[i], strlen(roots[i]), name_b64, sizeof(name_b64));
            foxr_replyf(c, "[FLPR/FILES/LIST/ITEM]1,0,%s", name_b64);
        }
        foxr_reply(c, "[FLPR/FILES/LIST/OK]");
        return;
    }

    File* dir = storage_file_alloc(c->storage);
    if(!storage_dir_open(dir, path)) {
        foxr_replyf(c, "[FLPR/FILES/LIST/ERR]%s", foxr_fs_error_code(storage_file_get_error(dir)));
        storage_file_free(dir);
        return;
    }

    FileInfo info;
    /* Heap-allocated, not stack: this loop runs on the esp_at_router
     * thread (esp_at_router.c), which has to budget for every handler's
     * worst-case call depth - a combined 608 bytes of stack-resident name
     * buffers on top of that thread's already-tight frame stack was a
     * real contributor to a confirmed real-hardware stack overflow (see
     * esp_at_router.c's ESP_AT_ROUTER_THREAD_STACK comment), even though
     * the fix that actually resolved it was widening that thread's stack,
     * not this alone - trimming this handler's own footprint is cheap
     * extra margin now that the pattern's been found once. */
    char* name = malloc(FOXR_MAX_NAME_LEN + 1);
    char* name_b64 = malloc(352);
    char size_str[24]; /* exact byte count, not %llu - see foxr_handle_files_stat()'s comment */
    while(storage_dir_read(dir, &info, name, FOXR_MAX_NAME_LEN)) {
        foxr_b64_encode((const uint8_t*)name, strlen(name), name_b64, 352);
        foxr_u64_to_str(info.size, size_str, sizeof(size_str));
        foxr_replyf(
            c, "[FLPR/FILES/LIST/ITEM]%d,%s,%s", file_info_is_dir(&info) ? 1 : 0, size_str, name_b64);
    }
    free(name);
    free(name_b64);
    storage_dir_close(dir);
    storage_file_free(dir);
    foxr_reply(c, "[FLPR/FILES/LIST/OK]");
}

static void foxr_handle_files_stat(FoxrCompanion* c, const char* path) {
    FileInfo info;
    FS_Error err = storage_common_stat(c->storage, path, &info);
    if(err == FSE_OK) {
        /* %s + foxr_u64_to_str(), not %llu directly - see foxr_handle_
         * storage()'s comment. Unlike storage totals, a file's exact byte
         * size matters here (most Flipper files - .nfc, .rfid, .sub - are
         * under 1KB, so rounding to KiB the way storage totals do isn't an
         * option), hence the self-contained decimal conversion instead of
         * a pre-divided %lu. */
        char size_str[24];
        foxr_u64_to_str(info.size, size_str, sizeof(size_str));
        foxr_replyf(c, "[FLPR/FILES/STAT/OK]%d,%s", file_info_is_dir(&info) ? 1 : 0, size_str);
    } else {
        foxr_replyf(c, "[FLPR/FILES/STAT/ERR]%s", foxr_fs_error_code(err));
    }
}

static void foxr_handle_files_read_start(FoxrCompanion* c, const char* path) {
    File* file = storage_file_alloc(c->storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        foxr_replyf(
            c, "[FLPR/FILES/READ/START/ERR]%s", foxr_fs_error_code(storage_file_get_error(file)));
        storage_file_free(file);
        return;
    }

    uint64_t size = storage_file_size(file);
    /* exact byte count, not %llu - see foxr_handle_files_stat()'s comment */
    char size_str[24];
    foxr_u64_to_str(size, size_str, sizeof(size_str));
    foxr_replyf(c, "[FLPR/FILES/READ/START/OK]%s", size_str);

    const char* prefix = "[FLPR/FILES/READ/CHUNK]";
    size_t prefix_len = strlen(prefix);
    size_t b64_cap = ((FOXR_READ_CHUNK_RAW + 2) / 3) * 4 + 1;
    uint8_t* raw = malloc(FOXR_READ_CHUNK_RAW);
    char* line = malloc(prefix_len + b64_cap);
    memcpy(line, prefix, prefix_len + 1);

    bool ok = true;
    uint64_t remaining = size;
    while(remaining > 0) {
        size_t want = (remaining < FOXR_READ_CHUNK_RAW) ? (size_t)remaining : FOXR_READ_CHUNK_RAW;
        size_t got = storage_file_read(file, raw, want);
        if(got == 0) {
            ok = false;
            break;
        }
        foxr_b64_encode(raw, got, line + prefix_len, b64_cap);
        esp_at_send(c->esp_at, line);
        remaining -= got;
    }

    free(raw);
    free(line);

    if(ok) {
        foxr_reply(c, "[FLPR/FILES/READ/END]");
    } else {
        foxr_replyf(c, "[FLPR/FILES/READ/ERR]%s", foxr_fs_error_code(storage_file_get_error(file)));
    }

    storage_file_close(file);
    storage_file_free(file);
}

static void foxr_handle_files_write_start(FoxrCompanion* c, const char* path) {
    if(c->write_active) {
        storage_file_close(c->write_file);
        storage_file_free(c->write_file);
        c->write_file = NULL;
        c->write_active = false;
    }
    File* file = storage_file_alloc(c->storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        foxr_replyf(
            c, "[FLPR/FILES/WRITE/START/ERR]%s", foxr_fs_error_code(storage_file_get_error(file)));
        storage_file_free(file);
        return;
    }
    c->write_file = file;
    c->write_active = true;
    foxr_reply(c, "[FLPR/FILES/WRITE/START/OK]");
}

static void foxr_handle_files_write_chunk(FoxrCompanion* c, const char* b64_payload) {
    if(!c->write_active) {
        foxr_reply(c, "[FLPR/FILES/WRITE/CHUNK/ERR]NOTOPEN");
        return;
    }
    uint8_t raw[FOXR_WRITE_CHUNK_RAW];
    size_t len = foxr_b64_decode(b64_payload, strlen(b64_payload), raw, sizeof(raw));
    if(len > sizeof(raw)) len = sizeof(raw);
    size_t written = storage_file_write(c->write_file, raw, len);
    if(written == len) {
        foxr_reply(c, "[FLPR/FILES/WRITE/CHUNK/OK]");
    } else {
        foxr_replyf(
            c, "[FLPR/FILES/WRITE/CHUNK/ERR]%s", foxr_fs_error_code(storage_file_get_error(c->write_file)));
    }
}

static void foxr_handle_files_write_end(FoxrCompanion* c) {
    if(!c->write_active) {
        foxr_reply(c, "[FLPR/FILES/WRITE/END/ERR]NOTOPEN");
        return;
    }
    storage_file_close(c->write_file);
    storage_file_free(c->write_file);
    c->write_file = NULL;
    c->write_active = false;
    foxr_reply(c, "[FLPR/FILES/WRITE/END/OK]");
}

static void foxr_handle_files_delete(FoxrCompanion* c, const char* payload) {
    if(strlen(payload) < 2 || payload[1] != ',') {
        foxr_reply(c, "[FLPR/FILES/DELETE/ERR]INVALID");
        return;
    }
    bool recursive = payload[0] == '1';
    const char* path = payload + 2;

    FS_Error err = storage_common_remove(c->storage, path);
    if(err == FSE_DENIED && recursive) {
        bool ok = storage_simply_remove_recursive(c->storage, path);
        err = ok ? FSE_OK : FSE_INTERNAL;
    }
    if(err == FSE_OK || err == FSE_NOT_EXIST) {
        foxr_reply(c, "[FLPR/FILES/DELETE/OK]");
    } else {
        foxr_replyf(c, "[FLPR/FILES/DELETE/ERR]%s", foxr_fs_error_code(err));
    }
}

static void foxr_handle_files_mkdir(FoxrCompanion* c, const char* path) {
    FS_Error err = storage_common_mkdir(c->storage, path);
    if(err == FSE_OK) {
        foxr_reply(c, "[FLPR/FILES/MKDIR/OK]");
    } else {
        foxr_replyf(c, "[FLPR/FILES/MKDIR/ERR]%s", foxr_fs_error_code(err));
    }
}

static void foxr_handle_files_rename(FoxrCompanion* c, const char* payload) {
    const char* sep = strchr(payload, '|');
    if(!sep) {
        foxr_reply(c, "[FLPR/FILES/RENAME/ERR]INVALID");
        return;
    }
    char old_path[256];
    size_t old_len = (size_t)(sep - payload);
    if(old_len >= sizeof(old_path)) old_len = sizeof(old_path) - 1;
    memcpy(old_path, payload, old_len);
    old_path[old_len] = '\0';
    const char* new_path = sep + 1;

    FS_Error err = storage_common_rename(c->storage, old_path, new_path);
    if(err == FSE_OK) {
        foxr_reply(c, "[FLPR/FILES/RENAME/OK]");
    } else {
        foxr_replyf(c, "[FLPR/FILES/RENAME/ERR]%s", foxr_fs_error_code(err));
    }
}

/* ── Screen streaming ──────────────────────────────────────────────── */

static void
    foxr_screen_frame_callback(uint8_t* data, size_t size, CanvasOrientation orientation, void* context) {
    UNUSED(orientation);
    FoxrCompanion* c = context;
    if(!c->screen_frame_buf || size != c->screen_frame_size) return;
    memcpy(c->screen_frame_buf, data, size);
    if(c->screen_thread) {
        furi_thread_flags_set(furi_thread_get_id(c->screen_thread), FoxrScreenFlagTransmit);
    }
}

static int32_t foxr_screen_thread(void* context) {
    FoxrCompanion* c = context;
    const char* prefix = "[FLPR/SCREEN/FRAME]";
    size_t prefix_len = strlen(prefix);

    while(true) {
        uint32_t flags = furi_thread_flags_wait(FoxrScreenFlagAny, FuriFlagWaitAny, FuriWaitForever);

        if(flags & FoxrScreenFlagTransmit) {
            foxr_b64_encode(
                c->screen_frame_buf,
                c->screen_frame_size,
                c->screen_line_buf + prefix_len,
                c->screen_b64_capacity);
            esp_at_send(c->esp_at, c->screen_line_buf);
            /* Throttle: don't look for the next transmit request until the
             * minimum interval has passed, so a screen that redraws faster
             * than that (a blinking indicator, a live graph) coalesces
             * into one frame per interval instead of one per redraw - see
             * FOXR_SCREEN_MIN_INTERVAL_MS's comment above. */
            furi_delay_ms(FOXR_SCREEN_MIN_INTERVAL_MS);
        }

        if(flags & FoxrScreenFlagExit) break;
    }

    return 0;
}

static void foxr_handle_screen_stop(FoxrCompanion* c) {
    if(!c->screen_active) {
        foxr_reply(c, "[FLPR/SCREEN/STOP/OK]");
        return;
    }
    gui_remove_framebuffer_callback(c->gui, foxr_screen_frame_callback, c);
    furi_thread_flags_set(furi_thread_get_id(c->screen_thread), FoxrScreenFlagExit);
    furi_thread_join(c->screen_thread);
    furi_thread_free(c->screen_thread);
    c->screen_thread = NULL;
    free(c->screen_frame_buf);
    c->screen_frame_buf = NULL;
    free(c->screen_line_buf);
    c->screen_line_buf = NULL;
    c->screen_active = false;
    foxr_reply(c, "[FLPR/SCREEN/STOP/OK]");
}

static void foxr_handle_screen_start(FoxrCompanion* c) {
    if(c->screen_active) {
        foxr_reply(c, "[FLPR/SCREEN/START/OK]");
        return;
    }

    c->screen_frame_size = gui_get_framebuffer_size(c->gui);
    c->screen_frame_buf = malloc(c->screen_frame_size);
    memset(c->screen_frame_buf, 0, c->screen_frame_size);

    const char* prefix = "[FLPR/SCREEN/FRAME]";
    size_t prefix_len = strlen(prefix);
    c->screen_b64_capacity = ((c->screen_frame_size + 2) / 3) * 4 + 1;
    c->screen_line_buf = malloc(prefix_len + c->screen_b64_capacity);
    memcpy(c->screen_line_buf, prefix, prefix_len + 1);

    c->screen_active = true;
    c->screen_thread = furi_thread_alloc_ex("FoxrScreenTx", 1536, foxr_screen_thread, c);
    furi_thread_start(c->screen_thread);

    gui_add_framebuffer_callback(c->gui, foxr_screen_frame_callback, c);

    foxr_reply(c, "[FLPR/SCREEN/START/OK]");
}

/* ── Input ─────────────────────────────────────────────────────────── */

static const struct {
    const char* name;
    InputKey key;
} kFoxrKeys[] = {
    {"UP", InputKeyUp},
    {"DOWN", InputKeyDown},
    {"LEFT", InputKeyLeft},
    {"RIGHT", InputKeyRight},
    {"OK", InputKeyOk},
    {"BACK", InputKeyBack},
};

static const struct {
    const char* name;
    InputType type;
} kFoxrTypes[] = {
    {"PRESS", InputTypePress},
    {"RELEASE", InputTypeRelease},
    {"SHORT", InputTypeShort},
    {"LONG", InputTypeLong},
    {"REPEAT", InputTypeRepeat},
};

static bool foxr_parse_key(const char* s, InputKey* out) {
    for(size_t i = 0; i < COUNT_OF(kFoxrKeys); i++) {
        if(strcmp(s, kFoxrKeys[i].name) == 0) {
            *out = kFoxrKeys[i].key;
            return true;
        }
    }
    return false;
}

static bool foxr_parse_type(const char* s, InputType* out) {
    for(size_t i = 0; i < COUNT_OF(kFoxrTypes); i++) {
        if(strcmp(s, kFoxrTypes[i].name) == 0) {
            *out = kFoxrTypes[i].type;
            return true;
        }
    }
    return false;
}

static void foxr_handle_input(FoxrCompanion* c, const char* payload) {
    const char* comma = strchr(payload, ',');
    if(!comma) {
        foxr_reply(c, "[FLPR/INPUT/ERR]INVALID");
        return;
    }
    char key_buf[16] = {0};
    char type_buf[16] = {0};
    size_t key_len = (size_t)(comma - payload);
    if(key_len >= sizeof(key_buf)) key_len = sizeof(key_buf) - 1;
    memcpy(key_buf, payload, key_len);
    key_buf[key_len] = '\0';
    strncpy(type_buf, comma + 1, sizeof(type_buf) - 1);

    InputKey key;
    InputType type;
    if(!foxr_parse_key(key_buf, &key) || !foxr_parse_type(type_buf, &type)) {
        foxr_reply(c, "[FLPR/INPUT/ERR]INVALID");
        return;
    }

    InputEvent event = {
        .key = key,
        .type = type,
        .sequence_source = INPUT_SEQUENCE_SOURCE_SOFTWARE,
    };
    if(event.type == InputTypePress) {
        c->input_counter++;
        if(c->input_counter == 0) c->input_counter++;
        c->input_key_counter[event.key] = c->input_counter;
    }
    event.sequence_counter = c->input_key_counter[event.key];
    if(event.type == InputTypeRelease) {
        c->input_key_counter[event.key] = 0;
    }

    furi_pubsub_publish(c->input_events, &event);
    foxr_reply(c, "[FLPR/INPUT/OK]");
}

/* ── Settings: clock, reboot ──────────────────────────────────────── */

static void foxr_handle_datetime_get(FoxrCompanion* c) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    foxr_replyf(
        c,
        "[FLPR/DATETIME/OK]%u,%u,%u,%u,%u,%u,%u",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second,
        (unsigned)dt.weekday);
}

/* payload: "year,month,day,hour,minute,second,weekday" - same field order
 * as the GET reply above, so a caller can round-trip one straight into the
 * other without reshuffling. Validated with datetime_validate_datetime()
 * before it's allowed anywhere near the RTC - a malformed or out-of-range
 * value here would otherwise corrupt the clock silently. */
static void foxr_handle_datetime_set(FoxrCompanion* c, const char* payload) {
    unsigned year, month, day, hour, minute, second, weekday;
    int n = sscanf(
        payload, "%u,%u,%u,%u,%u,%u,%u", &year, &month, &day, &hour, &minute, &second, &weekday);
    if(n != 7) {
        foxr_reply(c, "[FLPR/DATETIME/SET/ERR]INVALID");
        return;
    }
    DateTime dt = {
        .hour = (uint8_t)hour,
        .minute = (uint8_t)minute,
        .second = (uint8_t)second,
        .day = (uint8_t)day,
        .month = (uint8_t)month,
        .year = (uint16_t)year,
        .weekday = (uint8_t)weekday,
    };
    if(!datetime_validate_datetime(&dt)) {
        foxr_reply(c, "[FLPR/DATETIME/SET/ERR]INVALID");
        return;
    }
    furi_hal_rtc_set_datetime(&dt);
    foxr_reply(c, "[FLPR/DATETIME/SET/OK]");
}

/* payload: "0" for a normal reboot, "1" to reboot straight into DFU (MCU
 * bootloader) mode - matches FuriHalRtcBootMode's own Normal=0/Dfu=1
 * numbering, so the browser's Settings tab can send its existing 0/1
 * convention unchanged.
 *
 * furi_hal_power_reset() never returns, so this function's caller
 * (foxr_companion_dispatch()) never gets back here to run its usual
 * end-of-command foxr_log_event() call - this handler logs itself, right
 * before the reset, so the reboot still shows up in both the on-screen
 * log and the persistent SD card file like every other command. tag/line
 * are threaded through from dispatch() for exactly that call. */
static void
    foxr_handle_reboot(FoxrCompanion* c, const char* tag, const char* line, const char* payload) {
    bool dfu = (payload[0] == '1');
    foxr_reply(c, "[FLPR/REBOOT/OK]");
    foxr_log_event(c, tag, line);
    /* Let the reply actually leave the UART - esp_at_send() writes are
     * synchronous, but the ESP32 side still needs a moment to relay the
     * line on to the browser before this device goes dark for the reset. */
    furi_delay_ms(100);
    furi_hal_rtc_set_boot_mode(dfu ? FuriHalRtcBootModeDfu : FuriHalRtcBootModeNormal);
    furi_hal_power_reset();
}

/* ── Settings: notification / locale / menu theme (task #54) ─────────
 *
 * Everything below wires up real device settings the browser's Settings
 * tab previously had no way to touch at all - see the notice that used to
 * sit in that tab's placeholder. Three independent groups, matching how
 * this firmware already keeps them (NotificationApp's own settings
 * struct, the separate Locale/RTC-flag service, and Fox.cfg's menu-theme
 * byte), each with its own SET command rather than one giant combined one
 * - so changing a menu theme, say, can never accidentally also resend a
 * stale locale snapshot. GET returns all three groups on one line so the
 * browser can render the whole tab from a single round trip; every SET
 * still range-checks its own payload before writing anything, same
 * INVALID-on-any-doubt approach as FLPR/DATETIME/SET.
 *
 * display_brightness/led_brightness/speaker_volume are stored as 0.0-1.0
 * floats on this firmware, but this wire format uses plain 0-100 integer
 * percents both ways - this file already avoids trusting this toolchain's
 * vsnprintf with tricky conversions (see foxr_u64_to_str()'s own comment,
 * about %llu specifically) and integer percents sidestep needing %f here
 * at all, not just the one call that bit list_storage before.
 *
 * ── task #75 (redone) extension ──────────────────────────────────────
 * Everything from here to foxr_handle_settings_alarm_set() below adds the
 * settings groups task #75's fresh audit of applications/settings/ found
 * still missing from the tab above: Night Shift + RGB Backlight (both
 * live on this same NotificationApp settings struct as the group above,
 * just deeper sub-screens of it), System (Sleep Method/File Naming/
 * Device Name), Input (button vibration), Bluetooth (on/off), Power
 * (Auto PowerOff/Limit Charge), Desktop display cosmetics (Battery View/
 * Show Clock/Midnight Format/WiFi icon/Battery+SD icons/Shell Color), and
 * Fox Alarm Clock's three global toggles. Same rules as above: GET
 * appends every new field onto ONE combined reply line (one round trip),
 * every SET stays its own small independently-validated command per
 * group.
 *
 * Deliberately NOT ported (task #75 audit, decided by design rather than
 * left for later): Custom Wallpaper, Main Menu Apps pins, and Favorite app
 * bindings - all need an on-device file browser this protocol has no
 * equivalent of; VGM Options - cosmetic colors for RPC screen-mirroring
 * only, not used by anything in this fork; Storage's Format/Benchmark/
 * Factory Reset - destructive actions, not settings; System's Log Level/
 * Device/Baud Rate, Debug flag, and Heap Trace - developer-only
 * diagnostics; Clock & Alarm's PWM/clock output and Expansion's Listen
 * UART - hardware wiring config, not user settings; the ESP32 UART channel
 * pin choice specifically - changing it remotely risks cutting the very
 * serial link this protocol runs over.
 *
 * The Fox Alarm Clock's individual alarm list (add/edit/delete alarms
 * with time/day-of-week/recurring) was originally left out of that list
 * pending "its own dedicated UI" - it has one now, see the ALARM/LIST,
 * ALARM/ADD, ALARM/EDIT, and ALARM/DELETE commands below, alongside the
 * pre-existing ALARM/SET for the 3 global toggles.
 *
 * PIN/lock security settings and everything nested under Security &
 * Privacy (Set/Change/Remove PIN, MAX Attempts, On Exceed - which can be
 * set to wipe the SD card, Advanced Security's lock-triggered BLE/GPIO/USB
 * disconnects, Lock Screen Display) were excluded here under that same
 * reasoning through task #75, but task #93 reversed that specific call by
 * explicit product decision - full parity with the on-device screen was
 * worth more than the transport risk. See the "Security & Privacy (task
 * #93)" section below for the actual commands and the PIN-transport
 * caveat spelled out there. */

/* Shared lookup tables mirroring the *_value[] arrays in
 * notification_settings_app.c / desktop_settings_scene_*.c / power_settings_
 * scene_start.c / input_settings_app.c - kept private to this file (no
 * dependency on any toolbox index-lookup header, matching this file's own
 * base64 section's stated preference for self-contained code) so GET can
 * translate a live stored value back into the same small index each SET
 * below accepts. */
static uint8_t foxr_index_of_u32(uint32_t val, const uint32_t* arr, uint8_t count) {
    for(uint8_t i = 0; i < count; i++) {
        if(arr[i] == val) return i;
    }
    return 0;
}
static uint8_t foxr_index_of_float(float val, const float* arr, uint8_t count) {
    for(uint8_t i = 0; i < count; i++) {
        if(arr[i] == val) return i;
    }
    return 0;
}

#define FOXR_NIGHT_SHIFT_COUNT 7
static const float foxr_night_shift_value[FOXR_NIGHT_SHIFT_COUNT] = {
    1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f};

#define FOXR_VIBRO_TOUCH_LEVEL_COUNT 10
static const uint32_t foxr_vibro_touch_level_value[FOXR_VIBRO_TOUCH_LEVEL_COUNT] =
    {0, 13, 16, 19, 21, 24, 27, 30, 33, 36};

#define FOXR_VIBRO_TOUCH_TRIGGER_COUNT 3
static const uint32_t foxr_vibro_touch_trigger_value[FOXR_VIBRO_TOUCH_TRIGGER_COUNT] = {
    (1u << InputTypePress),
    (1u << InputTypeRelease),
    (1u << InputTypePress) | (1u << InputTypeRelease)};

#define FOXR_AUTO_POWEROFF_COUNT 8
static const uint32_t foxr_auto_poweroff_value[FOXR_AUTO_POWEROFF_COUNT] =
    {0, 300000, 600000, 900000, 1800000, 2700000, 3600000, 5400000};

#define FOXR_LIMIT_CHARGE_COUNT 6
static const uint32_t foxr_limit_charge_value[FOXR_LIMIT_CHARGE_COUNT] = {0, 90, 85, 80, 75, 70};

/* Security & Privacy (task #93) - mirrors pin_menu.c's own auto_lock_delay_
 * value[]/s_max_attempts_labels[] exactly, so an index round-trips to the
 * same on-device meaning. */
#define FOXR_AUTO_LOCK_COUNT 9
static const uint32_t foxr_auto_lock_value[FOXR_AUTO_LOCK_COUNT] =
    {0, 10000, 15000, 30000, 60000, 90000, 120000, 300000, 600000};

#define FOXR_MAX_ATTEMPTS_COUNT 9
static const uint32_t foxr_max_attempts_value[FOXR_MAX_ATTEMPTS_COUNT] =
    {0, 3, 4, 5, 6, 7, 8, 9, 10};

static void foxr_handle_settings_get(FoxrCompanion* c) {
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    int contrast = notification->settings.contrast;
    unsigned backlight_pct =
        (unsigned)(notification->settings.display_brightness * 100.0f + 0.5f);
    unsigned led_pct = (unsigned)(notification->settings.led_brightness * 100.0f + 0.5f);
    unsigned volume_pct = (unsigned)(notification->settings.speaker_volume * 100.0f + 0.5f);
    unsigned delay_ms = notification->settings.display_off_delay_ms;
    unsigned vibro = notification->settings.vibro_on ? 1u : 0u;
    unsigned inversion = notification->settings.lcd_inversion ? 1u : 0u;

    unsigned night_shift_idx = foxr_index_of_float(
        notification->settings.night_shift, foxr_night_shift_value, FOXR_NIGHT_SHIFT_COUNT);
    unsigned night_shift_start = notification->settings.night_shift_start;
    unsigned night_shift_end = notification->settings.night_shift_end;

    unsigned rgb_installed = notification->settings.rgb.rgb_backlight_installed ? 1u : 0u;
    unsigned rgb_white_mode = notification->settings.rgb.white_backlight_mode ? 1u : 0u;
    unsigned rgb_led1 = notification->settings.rgb.led_2_color_index; /* human 1 = hw slot 2 */
    unsigned rgb_led2 = notification->settings.rgb.led_1_color_index;
    unsigned rgb_led3 = notification->settings.rgb.led_0_color_index;
    unsigned rgb_effect = notification->settings.rgb.rainbow_mode;
    unsigned rgb_speed_ms = notification->settings.rgb.rainbow_speed_ms;
    unsigned rgb_step = notification->settings.rgb.rainbow_step;
    unsigned rgb_saturation = notification->settings.rgb.rainbow_saturation;
    unsigned rgb_wave_wide = notification->settings.rgb.rainbow_wide;
    furi_record_close(RECORD_NOTIFICATION);

    unsigned time_fmt = (unsigned)locale_get_time_format();
    unsigned date_fmt = (unsigned)locale_get_date_format();
    unsigned units = (unsigned)locale_get_measurement_unit();
    unsigned hand = furi_hal_rtc_is_flag_set(FuriHalRtcFlagHandOrient) ? 1u : 0u;
    unsigned theme = fox_theme_get_style();
    unsigned sleep_method = furi_hal_rtc_is_flag_set(FuriHalRtcFlagLegacySleep) ? 1u : 0u;
    unsigned file_naming = furi_hal_rtc_is_flag_set(FuriHalRtcFlagDetailedFilename) ? 1u : 0u;

    InputSettings* input_settings = furi_record_open(RECORD_INPUT_SETTINGS);
    unsigned vibro_level_idx = foxr_index_of_u32(
        input_settings->vibro_touch_level, foxr_vibro_touch_level_value,
        FOXR_VIBRO_TOUCH_LEVEL_COUNT);
    unsigned vibro_trigger_idx = foxr_index_of_u32(
        input_settings->vibro_touch_trigger_mask, foxr_vibro_touch_trigger_value,
        FOXR_VIBRO_TOUCH_TRIGGER_COUNT);
    furi_record_close(RECORD_INPUT_SETTINGS);

    Bt* bt = furi_record_open(RECORD_BT);
    BtSettings bt_settings;
    bt_get_settings(bt, &bt_settings);
    unsigned bt_enabled = bt_settings.enabled ? 1u : 0u;
    furi_record_close(RECORD_BT);

    Power* power = furi_record_open(RECORD_POWER);
    PowerSettings power_settings;
    power_api_get_settings(power, &power_settings);
    unsigned auto_poweroff_idx = foxr_index_of_u32(
        power_settings.auto_poweroff_delay_ms, foxr_auto_poweroff_value,
        FOXR_AUTO_POWEROFF_COUNT);
    unsigned limit_charge_idx = foxr_index_of_u32(
        power_settings.charge_supress_percent, foxr_limit_charge_value, FOXR_LIMIT_CHARGE_COUNT);
    furi_record_close(RECORD_POWER);

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    DesktopSettings desktop_settings;
    desktop_api_get_settings(desktop, &desktop_settings);
    furi_record_close(RECORD_DESKTOP);
    unsigned battery_view = desktop_settings.displayBatteryPercentage;
    unsigned show_clock = desktop_settings.display_clock;
    unsigned midnight_fmt = desktop_settings.clock_midnight_zero;
    unsigned wifi_icon_hidden = desktop_settings.wifi_icon_hidden;
    unsigned statusbar_icons = desktop_settings.statusbar_show_icons;
    unsigned alarm_keep_backlight = desktop_settings.alarm_keep_backlight_all_night;
    unsigned alarm_beep = desktop_settings.alarm_beep_enabled;
    unsigned alarm_vibrate = desktop_settings.alarm_vibrate_enabled;

    CliSettings cli_settings;
    cli_settings_load(&cli_settings);
    unsigned shell_color_idx = cli_settings.shell_color_index;

    const char* device_name = furi_hal_version_get_name_ptr();

    foxr_replyf(
        c,
        "[FLPR/SETTINGS/OK]%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,"
        "%u,%u,%u,"
        "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,"
        "%u,%u,"
        "%u,%u,"
        "%u,"
        "%u,%u,"
        "%u,%u,%u,%u,%u,%u,"
        "%u,%u,%u,"
        "%s",
        contrast,
        backlight_pct,
        led_pct,
        volume_pct,
        delay_ms,
        vibro,
        inversion,
        time_fmt,
        date_fmt,
        units,
        hand,
        theme,
        night_shift_idx,
        night_shift_start,
        night_shift_end,
        rgb_installed,
        rgb_white_mode,
        rgb_led1,
        rgb_led2,
        rgb_led3,
        rgb_effect,
        rgb_speed_ms,
        rgb_step,
        rgb_saturation,
        rgb_wave_wide,
        sleep_method,
        file_naming,
        vibro_level_idx,
        vibro_trigger_idx,
        bt_enabled,
        auto_poweroff_idx,
        limit_charge_idx,
        battery_view,
        show_clock,
        midnight_fmt,
        wifi_icon_hidden,
        statusbar_icons,
        shell_color_idx,
        alarm_keep_backlight,
        alarm_beep,
        alarm_vibrate,
        device_name ? device_name : "");
}

/* payload: "contrast,backlight_pct,led_pct,volume_pct,delay_ms,vibro,inversion"
 * - same order/units as the notification-settings slice of the GET reply
 * above. `contrast` is -8..8 (LCD Contrast on the on-device UI); the three
 * *_pct fields are 0..100; `delay_ms` (LCD auto-dim delay) is capped at
 * 3600000 (60 min) as a sanity bound, not because the field itself means
 * anything past that; `vibro`/`inversion` are 0/1. Applies
 * sequence_display_backlight_force_on so a brightness change is visible
 * immediately (deliberately skips a blink/vibro pulse here - fine as
 * one-off feedback on the on-device UI's own single-field controls, but
 * this is one combined write and firing all three together on every
 * change, including ones that only touched volume or delay, would be
 * surprising) and always persists via notification_message_save_settings()
 * before replying, same as the on-device Settings UI does on its own exit. */
static void foxr_handle_settings_notif_set(FoxrCompanion* c, const char* payload) {
    int contrast;
    unsigned backlight_pct, led_pct, volume_pct, delay_ms, vibro, inversion;
    int n = sscanf(
        payload,
        "%d,%u,%u,%u,%u,%u,%u",
        &contrast,
        &backlight_pct,
        &led_pct,
        &volume_pct,
        &delay_ms,
        &vibro,
        &inversion);
    if(n != 7 || contrast < -8 || contrast > 8 || backlight_pct > 100 || led_pct > 100 ||
       volume_pct > 100 || delay_ms > 3600000u || vibro > 1 || inversion > 1) {
        foxr_reply(c, "[FLPR/SETTINGS/NOTIF/SET/ERR]INVALID");
        return;
    }

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification->settings.contrast = (int8_t)contrast;
    notification->settings.display_brightness = (float)backlight_pct / 100.0f;
    notification->settings.led_brightness = (float)led_pct / 100.0f;
    notification->settings.speaker_volume = (float)volume_pct / 100.0f;
    notification->settings.display_off_delay_ms = delay_ms;
    notification->settings.vibro_on = (vibro != 0);
    notification->settings.lcd_inversion = (inversion != 0);

    notification_message(notification, &sequence_display_backlight_force_on);
    notification_message_save_settings(notification);
    furi_record_close(RECORD_NOTIFICATION);

    foxr_reply(c, "[FLPR/SETTINGS/NOTIF/SET/OK]");
}

/* payload: "time_format,date_format,units,hand_orient" - each an index
 * into its own small enum: time_format 0=24h/1=12h (LocaleTimeFormat),
 * date_format 0=D/M/Y/1=M/D/Y/2=Y/M/D (LocaleDateFormat), units
 * 0=Metric/1=Imperial (LocaleMeasurementUnits), hand_orient 0=Righty/
 * 1=Lefty (FuriHalRtcFlagHandOrient). These four persist through the
 * Locale service/RTC flags themselves - no separate save call needed,
 * same as FLPR/DATETIME/SET's furi_hal_rtc_set_datetime(). */
static void foxr_handle_settings_locale_set(FoxrCompanion* c, const char* payload) {
    unsigned time_fmt, date_fmt, units, hand;
    int n = sscanf(payload, "%u,%u,%u,%u", &time_fmt, &date_fmt, &units, &hand);
    if(n != 4 || time_fmt > 1 || date_fmt > 2 || units > 1 || hand > 1) {
        foxr_reply(c, "[FLPR/SETTINGS/LOCALE/SET/ERR]INVALID");
        return;
    }
    locale_set_time_format((LocaleTimeFormat)time_fmt);
    locale_set_date_format((LocaleDateFormat)date_fmt);
    locale_set_measurement_unit((LocaleMeasurementUnits)units);
    if(hand) {
        furi_hal_rtc_set_flag(FuriHalRtcFlagHandOrient);
    } else {
        furi_hal_rtc_reset_flag(FuriHalRtcFlagHandOrient);
    }
    foxr_reply(c, "[FLPR/SETTINGS/LOCALE/SET/OK]");
}

/* payload: a single digit "0".."4" - fox_theme_set_style()'s own style
 * index (0=Classic, 1=Fox Theme, 2=Carousel, 3=Slider, 4=Tiny; see
 * gui/modules/fox_theme.h). This is the FoxFW-specific "menu theme" the
 * Settings tab's old placeholder notice named as one of the gaps -
 * fox_theme_set_style() persists straight to /int/Fox.cfg itself, no
 * separate save call needed. */
static void foxr_handle_settings_theme_set(FoxrCompanion* c, const char* payload) {
    unsigned style;
    int n = sscanf(payload, "%u", &style);
    if(n != 1 || style > 4) {
        foxr_reply(c, "[FLPR/SETTINGS/THEME/SET/ERR]INVALID");
        return;
    }
    fox_theme_set_style((uint8_t)style);
    foxr_reply(c, "[FLPR/SETTINGS/THEME/SET/OK]");
}

/* payload: "mode,start_min,end_min" - `mode` is an index 0..6 into the same
 * OFF/-10%/-20%/-30%/-40%/-50%/-60% list the on-device Night Shift item
 * shows (foxr_night_shift_value[] above); `start_min`/`end_min` are plain
 * minutes-since-midnight (0..1439) rather than snapped to the on-device
 * UI's fixed 30-minute-step list, so any exact time can be set from here.
 * Lives on the same NotificationApp settings struct as NOTIF/SET above,
 * just its own command so a Night Shift change can't accidentally re-send
 * a stale Display & Sound snapshot (or vice versa). */
static void foxr_handle_settings_nightshift_set(FoxrCompanion* c, const char* payload) {
    unsigned mode, start_min, end_min;
    int n = sscanf(payload, "%u,%u,%u", &mode, &start_min, &end_min);
    if(n != 3 || mode >= FOXR_NIGHT_SHIFT_COUNT || start_min > 1439 || end_min > 1439) {
        foxr_reply(c, "[FLPR/SETTINGS/NIGHTSHIFT/SET/ERR]INVALID");
        return;
    }
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification->settings.night_shift = foxr_night_shift_value[mode];
    notification->settings.night_shift_start = start_min;
    notification->settings.night_shift_end = end_min;
    notification_message_save_settings(notification);
    furi_record_close(RECORD_NOTIFICATION);
    foxr_reply(c, "[FLPR/SETTINGS/NIGHTSHIFT/SET/OK]");
}

/* FLPR/SETTINGS/RGB/COLORS - no payload. One-time lookup for the browser's
 * LED color dropdowns: rgb_backlight_get_color_count()/_get_color_text()
 * are this firmware's own live color table (desktop_settings_scene_rgb_
 * settings.c and notification_settings_app.c both call these same two
 * functions rather than keeping their own copy of the list), so fetching
 * it here instead of hardcoding names on the browser side can never drift
 * out of sync with what index numbers actually mean on this firmware.
 * Reply is a single pipe-joined line, e.g. "[.../OK]Black|White|Red|..." -
 * plain color names, no '|' or ',' possible in any of them, so no
 * escaping is needed (same reasoning FLPR/INFO's kv-pipe reply already
 * relies on elsewhere in this file). */
static void foxr_handle_settings_rgb_colors_get(FoxrCompanion* c) {
    char buf[400];
    size_t len = 0;
    uint8_t count = rgb_backlight_get_color_count();
    for(uint8_t i = 0; i < count; i++) {
        const char* name = rgb_backlight_get_color_text(i);
        size_t name_len = strlen(name);
        /* +1 for a leading '|' on every entry after the first, +1 for the
         * NUL this loop must always leave room for. */
        if(len + name_len + 2 > sizeof(buf)) break;
        if(i > 0) buf[len++] = '|';
        memcpy(buf + len, name, name_len);
        len += name_len;
    }
    buf[len] = '\0';
    foxr_replyf(c, "[FLPR/SETTINGS/RGB/COLORS/OK]%s", buf);
}

/* payload: "installed,white_mode,led1,led2,led3,effect,speed_ms,step,
 * saturation,wave_wide" - mirrors desktop_settings_scene_rgb_settings.c /
 * notification_settings_app.c's RGB Mod Settings sub-screen field-for-
 * field (both UIs write this same NotificationApp settings.rgb struct, so
 * this is a third equally-valid entry point onto it, not a separate
 * store). `led1`/`led2`/`led3` are indices into the RGB/COLORS list above
 * in on-device left-to-right order (this function does the human-order-
 * to-hardware-order swap itself, same as both on-device UIs do); `effect`
 * is 0=Off/1=Rainbow/2=Wave; `speed_ms` 100..1000; `step` 1..3;
 * `saturation` 1..255; `wave_wide` one of 30/40/50. Re-applies the live
 * LED state immediately (skipped whenever a rainbow/wave effect is
 * running, since rainbow_timer_starter()'s own timer callback already
 * repaints every tick - forcing a static-color write here too would just
 * race it) and always persists via notification_message_save_settings()
 * before replying. */
static void foxr_handle_settings_rgb_set(FoxrCompanion* c, const char* payload) {
    unsigned installed, white_mode, led1, led2, led3, effect, speed_ms, step, saturation,
        wave_wide;
    int n = sscanf(
        payload,
        "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
        &installed,
        &white_mode,
        &led1,
        &led2,
        &led3,
        &effect,
        &speed_ms,
        &step,
        &saturation,
        &wave_wide);
    uint8_t color_count = rgb_backlight_get_color_count();
    if(n != 10 || installed > 1 || white_mode > 1 || led1 >= color_count ||
       led2 >= color_count || led3 >= color_count || effect > 2 || speed_ms < 100 ||
       speed_ms > 1000 || step < 1 || step > 3 || saturation < 1 || saturation > 255 ||
       (wave_wide != 30 && wave_wide != 40 && wave_wide != 50)) {
        foxr_reply(c, "[FLPR/SETTINGS/RGB/SET/ERR]INVALID");
        return;
    }

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification->settings.rgb.rgb_backlight_installed = (installed != 0);
    notification->settings.rgb.white_backlight_mode = (white_mode != 0);
    notification->settings.rgb.led_2_color_index = (uint8_t)led1; /* human 1 = hw slot 2 */
    notification->settings.rgb.led_1_color_index = (uint8_t)led2;
    notification->settings.rgb.led_0_color_index = (uint8_t)led3;
    notification->settings.rgb.rainbow_mode = effect;
    notification->settings.rgb.rainbow_speed_ms = speed_ms;
    notification->settings.rgb.rainbow_step = step;
    notification->settings.rgb.rainbow_saturation = (uint8_t)saturation;
    notification->settings.rgb.rainbow_wide = wave_wide;

    set_rgb_backlight_installed_variable(installed != 0);
    set_rgb_backlight_white_mode_variable(white_mode != 0);

    if(!installed) {
        rgb_backlight_set_led_static_color(2, 0);
        rgb_backlight_set_led_static_color(1, 0);
        rgb_backlight_set_led_static_color(0, 0);
        SK6805_update();
        rainbow_timer_stop(notification);
    } else if(effect > 0) {
        rainbow_timer_starter(notification);
    } else {
        rainbow_timer_stop(notification);
        rgb_backlight_set_led_static_color(2, (uint8_t)led1);
        rgb_backlight_set_led_static_color(1, (uint8_t)led2);
        rgb_backlight_set_led_static_color(0, (uint8_t)led3);
        rgb_backlight_update(
            notification->settings.display_brightness * notification->current_night_shift);
    }

    notification_message_save_settings(notification);
    furi_record_close(RECORD_NOTIFICATION);
    foxr_reply(c, "[FLPR/SETTINGS/RGB/SET/OK]");
}

/* payload: "sleep_method,file_naming,device_name" - `sleep_method` 0=
 * Default/1=Legacy (FuriHalRtcFlagLegacySleep); `file_naming` 0=Default/
 * 1=Detailed (FuriHalRtcFlagDetailedFilename); `device_name` is the
 * remainder of the payload after the second comma, letters/digits only
 * (same validator as the on-device System Settings > Device Name field),
 * up to FURI_HAL_VERSION_ARRAY_NAME_LENGTH-1 chars, may be empty to reset
 * to the default hardware name. Mirrors system_settings.c's own Device
 * Name handling exactly - writes the same NAMECHANGER_PATH file (or
 * removes it, for an empty name); namechanger_srv only actually applies
 * it at boot, though.
 *
 * This used to reboot immediately after every successful save, same as
 * system_settings.c's own callback - but unlike that on-device screen,
 * this command is reached over the FoxLAB WiFi connection itself, and
 * furi_hal_power_reset() also power-cycles the attached ESP32 (it's
 * powered/reset via the Flipper's expansion port), which drops the very
 * AP + web server the caller is using to make this change. So instead:
 * the name is saved here (durably - that part isn't deferred), and if it
 * actually changed, `restart_pending_flag` is set instead of rebooting.
 * main.c's navigation_callback() checks that flag when the FoxLAB app is
 * about to close and detours to restart_confirm_view so the user can
 * choose "Restart now" or "Later" (next natural boot). Sleep method/file
 * naming still apply immediately, as before - they never needed the
 * reboot in the first place, that was purely for the name. */
static void foxr_handle_settings_system_set(FoxrCompanion* c, const char* payload) {
    unsigned sleep_method, file_naming;
    int consumed = 0;
    int n = sscanf(payload, "%u,%u%n", &sleep_method, &file_naming, &consumed);
    if(n != 2 || sleep_method > 1 || file_naming > 1 || payload[consumed] != ',') {
        foxr_reply(c, "[FLPR/SETTINGS/SYSTEM/SET/ERR]INVALID");
        return;
    }
    const char* name = payload + consumed + 1;
    size_t name_len = strlen(name);
    if(name_len >= FURI_HAL_VERSION_ARRAY_NAME_LENGTH) {
        foxr_reply(c, "[FLPR/SETTINGS/SYSTEM/SET/ERR]INVALID");
        return;
    }
    for(size_t i = 0; i < name_len; i++) {
        char ch = name[i];
        bool ok = (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= 'a' && ch <= 'z');
        if(!ok) {
            foxr_reply(c, "[FLPR/SETTINGS/SYSTEM/SET/ERR]INVALID");
            return;
        }
    }

    if(sleep_method) {
        furi_hal_rtc_set_flag(FuriHalRtcFlagLegacySleep);
    } else {
        furi_hal_rtc_reset_flag(FuriHalRtcFlagLegacySleep);
    }
    if(file_naming) {
        furi_hal_rtc_set_flag(FuriHalRtcFlagDetailedFilename);
    } else {
        furi_hal_rtc_reset_flag(FuriHalRtcFlagDetailedFilename);
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);

    /* Whether this actually changes what's showing, decided BEFORE the
     * write below touches anything - that's what decides if a restart is
     * worth flagging at all. `name_len == 0` (reset to default) is only a
     * real change if an override file is currently active - removing a
     * file that was never there changes nothing. Otherwise, furi_hal_
     * version_get_name_ptr() already reflects whatever override (if any)
     * was applied at last boot, so a direct compare against the requested
     * name catches "re-saving the same name" - which happens on every
     * System-card save, since the browser always resends whatever's
     * currently in the Device Name field alongside Sleep Method/File
     * Naming (see foxfw-lab.html) - without flagging a restart that
     * wouldn't actually change anything. */
    bool name_changed;
    if(name_len == 0) {
        FileInfo file_info;
        name_changed = (storage_common_stat(storage, NAMECHANGER_PATH, &file_info) == FSE_OK);
    } else {
        name_changed = (strcmp(furi_hal_version_get_name_ptr(), name) != 0);
    }

    bool name_saved = false;
    if(name_len == 0) {
        /* Empty name -> remove the override file to restore the real
         * hardware name, same as system_settings.c's own callback. */
        storage_simply_remove(storage, NAMECHANGER_PATH);
        name_saved = true;
    } else {
        FlipperFormat* file = flipper_format_file_alloc(storage);
        do {
            if(!flipper_format_file_open_always(file, NAMECHANGER_PATH)) break;
            if(!flipper_format_write_header_cstr(file, NAMECHANGER_HEADER, NAMECHANGER_VERSION))
                break;
            if(!flipper_format_write_string_cstr(file, "Name", name)) break;
            name_saved = true;
        } while(false);
        flipper_format_free(file);
    }
    furi_record_close(RECORD_STORAGE);

    if(!name_saved) {
        foxr_reply(c, "[FLPR/SETTINGS/SYSTEM/SET/ERR]IOFAIL");
        return;
    }

    if(name_changed && c->restart_pending_flag != NULL) {
        *c->restart_pending_flag = true;
    }
    /* Trailing digit tells the browser whether a restart is actually
     * pending now, so it only bothers the user with restart messaging
     * when there's really something to restart for (see foxfw-lab.html's
     * applyDeviceNameBtn handler). */
    foxr_replyf(c, "[FLPR/SETTINGS/SYSTEM/SET/OK]%d", name_changed ? 1 : 0);
}

/* payload: "vibro_level_idx,vibro_trigger_idx" - `vibro_level_idx` is an
 * index 0..9 (0=OFF, 1..9 = on-device "1".."9" strength labels -
 * foxr_vibro_touch_level_value[] above); `vibro_trigger_idx` is 0=Press/
 * 1=Release/2=Both. Mirrors input_settings_app.c: writes both the live
 * RECORD_INPUT_SETTINGS record (so the change is felt on the very next
 * button press, same as the on-device Input Settings screen) and the
 * on-disk copy via input_settings_save(). */
static void foxr_handle_settings_input_set(FoxrCompanion* c, const char* payload) {
    unsigned level_idx, trigger_idx;
    int n = sscanf(payload, "%u,%u", &level_idx, &trigger_idx);
    if(n != 2 || level_idx >= FOXR_VIBRO_TOUCH_LEVEL_COUNT ||
       trigger_idx >= FOXR_VIBRO_TOUCH_TRIGGER_COUNT) {
        foxr_reply(c, "[FLPR/SETTINGS/INPUT/SET/ERR]INVALID");
        return;
    }
    InputSettings* live = furi_record_open(RECORD_INPUT_SETTINGS);
    live->vibro_touch_level = foxr_vibro_touch_level_value[level_idx];
    live->vibro_touch_trigger_mask = foxr_vibro_touch_trigger_value[trigger_idx];
    InputSettings snapshot = *live;
    furi_record_close(RECORD_INPUT_SETTINGS);
    input_settings_save(&snapshot);
    foxr_reply(c, "[FLPR/SETTINGS/INPUT/SET/OK]");
}

/* payload: a single digit "0"/"1" - Bluetooth on/off (bt_settings_app.c's
 * own on/off toggle; "Unpair All Devices" is deliberately not exposed
 * here - it's a destructive action, not a setting). bt_set_settings()
 * persists this itself, same as bt_settings_app_free()'s own only
 * persistence step - no separate save call needed. */
static void foxr_handle_settings_bt_set(FoxrCompanion* c, const char* payload) {
    unsigned enabled;
    int n = sscanf(payload, "%u", &enabled);
    if(n != 1 || enabled > 1) {
        foxr_reply(c, "[FLPR/SETTINGS/BT/SET/ERR]INVALID");
        return;
    }
    Bt* bt = furi_record_open(RECORD_BT);
    BtSettings settings;
    bt_get_settings(bt, &settings);
    settings.enabled = (enabled != 0);
    bt_set_settings(bt, &settings);
    furi_record_close(RECORD_BT);
    foxr_reply(c, "[FLPR/SETTINGS/BT/SET/OK]");
}

/* payload: "auto_poweroff_idx,limit_charge_idx" - `auto_poweroff_idx` is
 * an index 0..7 into OFF/5/10/15/30/45/60/90 minutes
 * (foxr_auto_poweroff_value[] above); `limit_charge_idx` is an index 0..5
 * into OFF/90%/85%/80%/75%/70% (foxr_limit_charge_value[] above) - stops
 * charging once the battery reaches that percentage. Battery Info/
 * Reboot/Power OFF from the same on-device screen are deliberately not
 * exposed here (an info view and two action buttons, not settings; reboot
 * already exists as its own FLPR/REBOOT command). power_api_set_settings()
 * doesn't itself persist to storage - power_settings_app_free()'s comment
 * confirms it's purely a live push - so this calls power_settings_save()
 * first, same order power_settings_app.c's own on-exit path uses. */
static void foxr_handle_settings_power_set(FoxrCompanion* c, const char* payload) {
    unsigned poweroff_idx, charge_idx;
    int n = sscanf(payload, "%u,%u", &poweroff_idx, &charge_idx);
    if(n != 2 || poweroff_idx >= FOXR_AUTO_POWEROFF_COUNT || charge_idx >= FOXR_LIMIT_CHARGE_COUNT) {
        foxr_reply(c, "[FLPR/SETTINGS/POWER/SET/ERR]INVALID");
        return;
    }
    PowerSettings settings;
    settings.auto_poweroff_delay_ms = foxr_auto_poweroff_value[poweroff_idx];
    settings.charge_supress_percent = (uint8_t)foxr_limit_charge_value[charge_idx];
    power_settings_save(&settings);
    Power* power = furi_record_open(RECORD_POWER);
    power_api_set_settings(power, &settings);
    furi_record_close(RECORD_POWER);
    foxr_reply(c, "[FLPR/SETTINGS/POWER/SET/OK]");
}

/* payload: "battery_view,show_clock,midnight_format,wifi_icon_hidden,
 * statusbar_icons,shell_color_idx" - mirrors desktop_settings_scene_
 * start.c's own top-level field set (minus ESP32 UART - see this file's
 * earlier "deliberately NOT ported" note - and minus Menu Theme, which
 * already has its own THEME/SET command). `battery_view` 0..5 (Bar/%/
 * Inv.%/Retro3/Retro5/Bar%); `show_clock`/`wifi_icon_hidden`/
 * `statusbar_icons` are each 0/1 (note wifi_icon_hidden is inverted - 0
 * means the icon SHOWS - same polarity the struct field itself already
 * uses, see desktop_settings.h); `midnight_format` 0=show "12"/1=show
 * "0"; `shell_color_idx` 0..7, a CLI-only setting stored completely
 * separately (CliSettings, not DesktopSettings) but surfaced alongside
 * these because that's where the on-device UI puts it too. */
static void foxr_handle_settings_desktop_set(FoxrCompanion* c, const char* payload) {
    unsigned battery_view, show_clock, midnight_fmt, wifi_icon_hidden, statusbar_icons,
        shell_color_idx;
    int n = sscanf(
        payload,
        "%u,%u,%u,%u,%u,%u",
        &battery_view,
        &show_clock,
        &midnight_fmt,
        &wifi_icon_hidden,
        &statusbar_icons,
        &shell_color_idx);
    if(n != 6 || battery_view > 5 || show_clock > 1 || midnight_fmt > 1 || wifi_icon_hidden > 1 ||
       statusbar_icons > 1 || shell_color_idx > 7) {
        foxr_reply(c, "[FLPR/SETTINGS/DESKTOP/SET/ERR]INVALID");
        return;
    }

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    DesktopSettings settings;
    desktop_api_get_settings(desktop, &settings);
    settings.displayBatteryPercentage = (uint8_t)battery_view;
    settings.display_clock = (uint8_t)show_clock;
    settings.clock_midnight_zero = (uint8_t)midnight_fmt;
    settings.wifi_icon_hidden = (uint8_t)wifi_icon_hidden;
    settings.statusbar_show_icons = (uint8_t)statusbar_icons;
    desktop_settings_save(&settings);
    desktop_api_set_settings(desktop, &settings);
    furi_record_close(RECORD_DESKTOP);

    CliSettings cli_settings;
    cli_settings.shell_color_index = (uint8_t)shell_color_idx;
    cli_settings_save(&cli_settings);

    foxr_reply(c, "[FLPR/SETTINGS/DESKTOP/SET/OK]");
}

/* payload: "keep_backlight,beep,vibrate" - each 0/1. The three global Fox
 * Alarm Clock toggles from desktop_settings_scene_alarm_clock.c's own
 * screen; the alarm list itself (individual alarms' time/days/recurring/
 * active) is NOT exposed here - see this file's earlier "deliberately NOT
 * ported" note - only these three always-present toggles. */
static void foxr_handle_settings_alarm_set(FoxrCompanion* c, const char* payload) {
    unsigned keep_backlight, beep, vibrate;
    int n = sscanf(payload, "%u,%u,%u", &keep_backlight, &beep, &vibrate);
    if(n != 3 || keep_backlight > 1 || beep > 1 || vibrate > 1) {
        foxr_reply(c, "[FLPR/SETTINGS/ALARM/SET/ERR]INVALID");
        return;
    }
    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    DesktopSettings settings;
    desktop_api_get_settings(desktop, &settings);
    settings.alarm_keep_backlight_all_night = (uint8_t)keep_backlight;
    settings.alarm_beep_enabled = (uint8_t)beep;
    settings.alarm_vibrate_enabled = (uint8_t)vibrate;
    desktop_settings_save(&settings);
    desktop_api_set_settings(desktop, &settings);
    furi_record_close(RECORD_DESKTOP);
    foxr_reply(c, "[FLPR/SETTINGS/ALARM/SET/OK]");
}

/* ── Fox Alarm Clock: individual alarm list (task #96) ────────────────
 *
 * Full CRUD over DesktopSettings.alarms[]/alarm_count (desktop_settings.h)
 * - the array desktop_settings_scene_alarm_clock.c's own on-device list
 * edits, alongside the 3 global toggles ALARM/SET above already covers.
 * alarms[0..alarm_count-1] are always contiguous (no holes) - matches how
 * the on-device screen itself manages the list, so DELETE shifts
 * everything after the removed slot down by one rather than leaving a gap.
 *
 * payload: none. Replies with every alarm on one combined line (same
 * one-round-trip style as the other GET commands in this file) rather
 * than FILES/LIST's per-item stream - at most FOX_ALARM_MAX_COUNT (8) of
 * these, small enough that a combined reply is simpler for the browser to
 * parse (see getAlarmList() in foxfw-lab.html) without needing FILES/
 * LIST's separate ITEM-stream handling.
 *
 * Format: "<count>|hour,minute,days_mask,active,recurring|..." - one
 * pipe-separated group per alarm, in on-device list order (index 0
 * first, which is also each alarm's ADD/EDIT/DELETE index). */
static void foxr_handle_settings_alarm_list(FoxrCompanion* c) {
    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    DesktopSettings settings;
    desktop_api_get_settings(desktop, &settings);
    furi_record_close(RECORD_DESKTOP);

    char line[32 + FOX_ALARM_MAX_COUNT * 24];
    int off =
        snprintf(line, sizeof(line), "[FLPR/SETTINGS/ALARM/LIST/OK]%u", settings.alarm_count);
    for(uint8_t i = 0; i < settings.alarm_count && off > 0 && (size_t)off < sizeof(line); i++) {
        const FoxAlarm* a = &settings.alarms[i];
        off += snprintf(
            line + off,
            sizeof(line) - (size_t)off,
            "|%u,%u,%u,%u,%u",
            a->hour,
            a->minute,
            a->days_mask,
            a->active,
            a->recurring);
    }
    foxr_reply(c, line);
}

/* payload: "hour,minute,days_mask,active,recurring" - appends a new alarm
 * to the end of the list. `hour` 0-23, `minute` 0-59, `days_mask` a
 * FOX_ALARM_DAY_*-bit OR-mask (0-127, only meaningful when recurring=1),
 * `active`/`recurring` each 0/1. Replies ERR FULL once the list already
 * has FOX_ALARM_MAX_COUNT (8) alarms - same limit as the on-device
 * screen, there's no "make room" flow on either side of this protocol. */
static void foxr_handle_settings_alarm_add(FoxrCompanion* c, const char* payload) {
    unsigned hour, minute, days_mask, active, recurring;
    int n = sscanf(payload, "%u,%u,%u,%u,%u", &hour, &minute, &days_mask, &active, &recurring);
    if(n != 5 || hour > 23 || minute > 59 || days_mask > 127 || active > 1 || recurring > 1) {
        foxr_reply(c, "[FLPR/SETTINGS/ALARM/ADD/ERR]INVALID");
        return;
    }

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    DesktopSettings settings;
    desktop_api_get_settings(desktop, &settings);
    if(settings.alarm_count >= FOX_ALARM_MAX_COUNT) {
        furi_record_close(RECORD_DESKTOP);
        foxr_reply(c, "[FLPR/SETTINGS/ALARM/ADD/ERR]FULL");
        return;
    }
    FoxAlarm* a = &settings.alarms[settings.alarm_count];
    a->hour = (uint8_t)hour;
    a->minute = (uint8_t)minute;
    a->days_mask = (uint8_t)days_mask;
    a->active = (uint8_t)active;
    a->recurring = (uint8_t)recurring;
    uint8_t new_index = settings.alarm_count;
    settings.alarm_count++;
    desktop_settings_save(&settings);
    desktop_api_set_settings(desktop, &settings);
    furi_record_close(RECORD_DESKTOP);

    foxr_replyf(c, "[FLPR/SETTINGS/ALARM/ADD/OK]%u", new_index);
}

/* payload: "index,hour,minute,days_mask,active,recurring" - overwrites an
 * existing alarm in place. Same field validation as ADD, plus `index`
 * must name an existing slot (0 <= index < alarm_count). */
static void foxr_handle_settings_alarm_edit(FoxrCompanion* c, const char* payload) {
    unsigned index, hour, minute, days_mask, active, recurring;
    int n = sscanf(
        payload, "%u,%u,%u,%u,%u,%u", &index, &hour, &minute, &days_mask, &active, &recurring);
    if(n != 6 || hour > 23 || minute > 59 || days_mask > 127 || active > 1 || recurring > 1) {
        foxr_reply(c, "[FLPR/SETTINGS/ALARM/EDIT/ERR]INVALID");
        return;
    }

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    DesktopSettings settings;
    desktop_api_get_settings(desktop, &settings);
    if(index >= settings.alarm_count) {
        furi_record_close(RECORD_DESKTOP);
        foxr_reply(c, "[FLPR/SETTINGS/ALARM/EDIT/ERR]NOTFOUND");
        return;
    }
    FoxAlarm* a = &settings.alarms[index];
    a->hour = (uint8_t)hour;
    a->minute = (uint8_t)minute;
    a->days_mask = (uint8_t)days_mask;
    a->active = (uint8_t)active;
    a->recurring = (uint8_t)recurring;
    desktop_settings_save(&settings);
    desktop_api_set_settings(desktop, &settings);
    furi_record_close(RECORD_DESKTOP);

    foxr_reply(c, "[FLPR/SETTINGS/ALARM/EDIT/OK]");
}

/* payload: "index" - removes one alarm, shifting every later alarm down
 * one slot to close the gap (alarms[0..alarm_count-1] stay contiguous). */
static void foxr_handle_settings_alarm_delete(FoxrCompanion* c, const char* payload) {
    unsigned index;
    int n = sscanf(payload, "%u", &index);
    if(n != 1) {
        foxr_reply(c, "[FLPR/SETTINGS/ALARM/DELETE/ERR]INVALID");
        return;
    }

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    DesktopSettings settings;
    desktop_api_get_settings(desktop, &settings);
    if(index >= settings.alarm_count) {
        furi_record_close(RECORD_DESKTOP);
        foxr_reply(c, "[FLPR/SETTINGS/ALARM/DELETE/ERR]NOTFOUND");
        return;
    }
    for(unsigned i = index; i + 1 < settings.alarm_count; i++) {
        settings.alarms[i] = settings.alarms[i + 1];
    }
    settings.alarm_count--;
    desktop_settings_save(&settings);
    desktop_api_set_settings(desktop, &settings);
    furi_record_close(RECORD_DESKTOP);

    foxr_reply(c, "[FLPR/SETTINGS/ALARM/DELETE/OK]");
}

/* ── Security & Privacy (task #93) ────────────────────────────────────
 *
 * Everything in applications/settings/desktop_settings/scenes/desktop_
 * settings_scene_pin_menu.c and its two sub-screens (disconnect_services.c
 * "Advanced Security", lock_display.c "Lock Screen Display"), including PIN
 * management and the "On Exceed" Format-SD trigger - a deliberate reversal
 * of this file's earlier "Deliberately NOT ported" note above (task #54/
 * #75 both excluded this category outright). Kept split into three
 * commands rather than one combined GET/SET the way DESKTOP/ALARM above
 * are, because PIN changes need a current-PIN check the other 14 fields
 * don't, and folding that into one big payload would mean re-sending (and
 * re-validating) an unrelated PIN on every single toggle flip:
 *   - FLPR/SETTINGS/SECURITY (GET) - all 15 fields, one round trip.
 *   - FLPR/SETTINGS/SECURITY/SET - the 14 non-PIN fields.
 *   - FLPR/SETTINGS/SECURITY/PIN/SET - set or change the PIN.
 *   - FLPR/SETTINGS/SECURITY/PIN/CLEAR - remove the PIN.
 *
 * PIN transport note: PINs travel over this UART link and then over the
 * FoxLAB AP's WebSocket relay as plain digit strings - there is no
 * end-to-end encryption beyond whatever the AP's own WiFi security
 * provides (the FoxLAB AP password is fixed and shown on-screen). This
 * command family exists anyway per an explicit product decision to expose
 * full parity with the on-device Security & Privacy screen; the browser
 * side surfaces the same warning next to the PIN fields. */

/* 2-byte-per-digit encoding, must stay identical to desktop_settings_scene_
 * pin_setup.c's pin_encode_k1/k2 and desktop_settings_scene_pin_auth.c's
 * pin_auth_k1/k2 (both comment that they mirror the lock screen's own copy
 * too) - not shared via a header because none of those call sites are
 * reachable from here without a much bigger include, and this pair is
 * tiny/stable: it's just each digit's base-4 split (k1 = d/4, k2 = d%4). */
static const uint8_t foxr_pin_encode_k1[10] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2};
static const uint8_t foxr_pin_encode_k2[10] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1};

/* `digits` need not be null-terminated - `len` bounds it (this is called
 * with sub-spans of a larger comma-separated payload). Rejects anything
 * that isn't a plain '0'-'9' string of DESKTOP_PIN_CODE_MIN_LEN..MAX_LEN
 * digits, same bounds the on-device numeric-pin view itself enforces. */
static bool foxr_pin_digits_to_code(const char* digits, size_t len, DesktopPinCode* out) {
    if(len < DESKTOP_PIN_CODE_MIN_LEN || len > DESKTOP_PIN_CODE_MAX_LEN) return false;
    memset(out, 0, sizeof(DesktopPinCode));
    for(size_t i = 0; i < len; i++) {
        if(digits[i] < '0' || digits[i] > '9') return false;
        uint8_t d = (uint8_t)(digits[i] - '0');
        out->data[out->length++] = (char)foxr_pin_encode_k1[d];
        out->data[out->length++] = (char)foxr_pin_encode_k2[d];
    }
    return true;
}

static void foxr_handle_settings_security_get(FoxrCompanion* c) {
    unsigned pin_is_set = desktop_pin_code_is_set() ? 1u : 0u;

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    DesktopSettings settings;
    desktop_api_get_settings(desktop, &settings);
    furi_record_close(RECORD_DESKTOP);

    unsigned max_attempts_idx = foxr_index_of_u32(
        (uint32_t)settings.pin_max_attempts, foxr_max_attempts_value, FOXR_MAX_ATTEMPTS_COUNT);
    unsigned auto_lock_idx = foxr_index_of_u32(
        settings.auto_lock_delay_ms, foxr_auto_lock_value, FOXR_AUTO_LOCK_COUNT);

    foxr_replyf(
        c,
        "[FLPR/SETTINGS/SECURITY/OK]%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
        pin_is_set,
        max_attempts_idx,
        (unsigned)settings.pin_exceed_action,
        (unsigned)settings.lock_on_lock_enabled,
        (unsigned)settings.lock_disconnect_ble,
        (unsigned)settings.lock_disconnect_gpio,
        (unsigned)settings.lock_usb_level,
        (unsigned)settings.lock_show_time,
        (unsigned)settings.lock_show_seconds,
        (unsigned)settings.lock_show_date,
        (unsigned)settings.lock_show_statusbar,
        (unsigned)settings.lock_unlock_prompt,
        (unsigned)settings.allow_poweroff_locked,
        auto_lock_idx,
        (unsigned)settings.usb_inhibit_auto_lock);
}

/* payload: "max_attempts_idx,exceed_action,on_lock_enabled,disconnect_ble,
 * disconnect_gpio,usb_level,show_time,show_seconds,show_date,
 * show_statusbar,unlock_prompt,poweroff_locked,auto_lock_idx,usb_inhibit" -
 * 14 fields, same order the GET reply above uses (minus pin_is_set, which
 * isn't writable here - see PIN/SET and PIN/CLEAR). `max_attempts_idx` is
 * an index 0..8 into foxr_max_attempts_value[] (No Limit/3../10);
 * `exceed_action` 0=Lock only/1=Format SD; `usb_level` 0=Off/1=CLI+RPC
 * Block/2=Full Disconnect (LockUsbLevel); `auto_lock_idx` an index 0..8
 * into foxr_auto_lock_value[]; every other field is a plain 0/1 toggle. */
static void foxr_handle_settings_security_set(FoxrCompanion* c, const char* payload) {
    unsigned max_attempts_idx, exceed_action, on_lock_enabled, disconnect_ble, disconnect_gpio,
        usb_level, show_time, show_seconds, show_date, show_statusbar, unlock_prompt,
        poweroff_locked, auto_lock_idx, usb_inhibit;
    int n = sscanf(
        payload,
        "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
        &max_attempts_idx,
        &exceed_action,
        &on_lock_enabled,
        &disconnect_ble,
        &disconnect_gpio,
        &usb_level,
        &show_time,
        &show_seconds,
        &show_date,
        &show_statusbar,
        &unlock_prompt,
        &poweroff_locked,
        &auto_lock_idx,
        &usb_inhibit);
    if(n != 14 || max_attempts_idx >= FOXR_MAX_ATTEMPTS_COUNT || exceed_action > 1 ||
       on_lock_enabled > 1 || disconnect_ble > 1 || disconnect_gpio > 1 || usb_level > 2 ||
       show_time > 1 || show_seconds > 1 || show_date > 1 || show_statusbar > 1 ||
       unlock_prompt > 1 || poweroff_locked > 1 || auto_lock_idx >= FOXR_AUTO_LOCK_COUNT ||
       usb_inhibit > 1) {
        foxr_reply(c, "[FLPR/SETTINGS/SECURITY/SET/ERR]INVALID");
        return;
    }

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    DesktopSettings settings;
    desktop_api_get_settings(desktop, &settings);
    settings.pin_max_attempts = (uint8_t)foxr_max_attempts_value[max_attempts_idx];
    settings.pin_exceed_action = (uint8_t)exceed_action;
    settings.lock_on_lock_enabled = (uint8_t)on_lock_enabled;
    settings.lock_disconnect_ble = (uint8_t)disconnect_ble;
    settings.lock_disconnect_gpio = (uint8_t)disconnect_gpio;
    settings.lock_usb_level = (uint8_t)usb_level;
    settings.lock_show_time = (uint8_t)show_time;
    settings.lock_show_seconds = (uint8_t)show_seconds;
    settings.lock_show_date = (uint8_t)show_date;
    settings.lock_show_statusbar = (uint8_t)show_statusbar;
    settings.lock_unlock_prompt = (uint8_t)unlock_prompt;
    settings.allow_poweroff_locked = (uint8_t)poweroff_locked;
    settings.auto_lock_delay_ms = foxr_auto_lock_value[auto_lock_idx];
    settings.usb_inhibit_auto_lock = (uint8_t)usb_inhibit;
    desktop_settings_save(&settings);
    desktop_api_set_settings(desktop, &settings);
    furi_record_close(RECORD_DESKTOP);
    foxr_reply(c, "[FLPR/SETTINGS/SECURITY/SET/OK]");
}

/* payload: "current_pin,new_pin" - `current_pin` is the digit string for
 * whatever PIN is already active, or EMPTY (nothing before the comma) when
 * none is set yet - mirrors the on-device flow, which only re-prompts for
 * the current PIN on Change PIN, not on the very first Set PIN. `new_pin`
 * is 1-10 digits, required. Both are parsed as literal digit strings, not
 * numbers - sscanf's %u would silently drop a leading zero, which changes
 * which PIN this actually is - so this splits on the comma by hand. */
static void foxr_handle_settings_security_pin_set(FoxrCompanion* c, const char* payload) {
    const char* comma = strchr(payload, ',');
    if(!comma) {
        foxr_reply(c, "[FLPR/SETTINGS/SECURITY/PIN/SET/ERR]INVALID");
        return;
    }
    size_t current_len = (size_t)(comma - payload);
    const char* new_pin = comma + 1;
    size_t new_len = strlen(new_pin);

    if(desktop_pin_code_is_set()) {
        if(current_len == 0) {
            foxr_reply(c, "[FLPR/SETTINGS/SECURITY/PIN/SET/ERR]AUTHREQUIRED");
            return;
        }
        DesktopPinCode current_code;
        if(!foxr_pin_digits_to_code(payload, current_len, &current_code)) {
            foxr_reply(c, "[FLPR/SETTINGS/SECURITY/PIN/SET/ERR]INVALID");
            return;
        }
        if(!desktop_pin_code_check(&current_code)) {
            foxr_reply(c, "[FLPR/SETTINGS/SECURITY/PIN/SET/ERR]WRONGPIN");
            return;
        }
    }

    DesktopPinCode new_code;
    if(!foxr_pin_digits_to_code(new_pin, new_len, &new_code)) {
        foxr_reply(c, "[FLPR/SETTINGS/SECURITY/PIN/SET/ERR]INVALID");
        return;
    }

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    desktop_api_set_pin(desktop, &new_code);
    furi_record_close(RECORD_DESKTOP);
    foxr_reply(c, "[FLPR/SETTINGS/SECURITY/PIN/SET/OK]");
}

/* payload: the current PIN's digit string, required - Remove PIN always
 * needs proof of the existing PIN, same as the on-device flow (pin_auth.c
 * gates pin_disable.c the same way). */
static void foxr_handle_settings_security_pin_clear(FoxrCompanion* c, const char* payload) {
    if(!desktop_pin_code_is_set()) {
        foxr_reply(c, "[FLPR/SETTINGS/SECURITY/PIN/CLEAR/ERR]NOTSET");
        return;
    }
    size_t len = strlen(payload);
    if(len == 0) {
        foxr_reply(c, "[FLPR/SETTINGS/SECURITY/PIN/CLEAR/ERR]AUTHREQUIRED");
        return;
    }
    DesktopPinCode current_code;
    if(!foxr_pin_digits_to_code(payload, len, &current_code)) {
        foxr_reply(c, "[FLPR/SETTINGS/SECURITY/PIN/CLEAR/ERR]INVALID");
        return;
    }
    if(!desktop_pin_code_check(&current_code)) {
        foxr_reply(c, "[FLPR/SETTINGS/SECURITY/PIN/CLEAR/ERR]WRONGPIN");
        return;
    }
    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    desktop_api_clear_pin(desktop);
    furi_record_close(RECORD_DESKTOP);
    foxr_reply(c, "[FLPR/SETTINGS/SECURITY/PIN/CLEAR/OK]");
}

/* ── Session teardown ──────────────────────────────────────────────── */

static void foxr_teardown_active_state(FoxrCompanion* c) {
    for(int key = 0; key < InputKeyMAX; key++) {
        if(c->input_key_counter[key] != 0) {
            InputEvent event = {
                .key = (InputKey)key,
                .type = InputTypeRelease,
                .sequence_source = INPUT_SEQUENCE_SOURCE_SOFTWARE,
                .sequence_counter = c->input_key_counter[key],
            };
            furi_pubsub_publish(c->input_events, &event);
            c->input_key_counter[key] = 0;
        }
    }
    if(c->screen_active) foxr_handle_screen_stop(c);
    if(c->write_active) {
        storage_file_close(c->write_file);
        storage_file_free(c->write_file);
        c->write_file = NULL;
        c->write_active = false;
    }
}

static void foxr_handle_session_end(FoxrCompanion* c) {
    foxr_teardown_active_state(c);
    foxr_reply(c, "[FLPR/SESSION/END/OK]");
}

/* ── Dispatch ──────────────────────────────────────────────────────── */

void foxr_companion_dispatch(void* context, const EspAtMsg* msg) {
    FoxrCompanion* c = context;
    const char* line = msg->line;

    const char* close = strchr(line, ']');
    if(!close || close == line) return;
    size_t tag_len = (size_t)(close - line - 1);
    char tag[48];
    if(tag_len >= sizeof(tag)) tag_len = sizeof(tag) - 1;
    memcpy(tag, line + 1, tag_len);
    tag[tag_len] = '\0';
    const char* payload = close + 1;

    /* Cleared up front so a hypothetical future tag that reaches the end
     * of the chain below without ever calling foxr_reply()/foxr_replyf()
     * logs as an empty reply (status "?") instead of showing whatever the
     * previous command happened to send. */
    c->last_reply[0] = '\0';

    if(strcmp(tag, "FLPR/PING") == 0) {
        foxr_reply(c, "[FLPR/PING/OK]");
    } else if(strcmp(tag, "FLPR/INFO") == 0) {
        foxr_handle_info(c);
    } else if(strcmp(tag, "FLPR/POWER") == 0) {
        foxr_handle_power(c);
    } else if(strcmp(tag, "FLPR/STORAGE") == 0) {
        foxr_handle_storage(c, payload);
    } else if(strcmp(tag, "FLPR/FILES/LIST") == 0) {
        foxr_handle_files_list(c, payload);
    } else if(strcmp(tag, "FLPR/FILES/STAT") == 0) {
        foxr_handle_files_stat(c, payload);
    } else if(strcmp(tag, "FLPR/FILES/READ/START") == 0) {
        foxr_handle_files_read_start(c, payload);
    } else if(strcmp(tag, "FLPR/FILES/WRITE/START") == 0) {
        foxr_handle_files_write_start(c, payload);
    } else if(strcmp(tag, "FLPR/FILES/WRITE/CHUNK") == 0) {
        foxr_handle_files_write_chunk(c, payload);
    } else if(strcmp(tag, "FLPR/FILES/WRITE/END") == 0) {
        foxr_handle_files_write_end(c);
    } else if(strcmp(tag, "FLPR/FILES/DELETE") == 0) {
        foxr_handle_files_delete(c, payload);
    } else if(strcmp(tag, "FLPR/FILES/MKDIR") == 0) {
        foxr_handle_files_mkdir(c, payload);
    } else if(strcmp(tag, "FLPR/FILES/RENAME") == 0) {
        foxr_handle_files_rename(c, payload);
    } else if(strcmp(tag, "FLPR/SCREEN/START") == 0) {
        foxr_handle_screen_start(c);
    } else if(strcmp(tag, "FLPR/SCREEN/STOP") == 0) {
        foxr_handle_screen_stop(c);
    } else if(strcmp(tag, "FLPR/INPUT") == 0) {
        foxr_handle_input(c, payload);
    } else if(strcmp(tag, "FLPR/DATETIME") == 0) {
        foxr_handle_datetime_get(c);
    } else if(strcmp(tag, "FLPR/DATETIME/SET") == 0) {
        foxr_handle_datetime_set(c, payload);
    } else if(strcmp(tag, "FLPR/REBOOT") == 0) {
        foxr_handle_reboot(c, tag, line, payload);
        return; /* unreachable in practice - furi_hal_power_reset() doesn't
                  * return - but keeps this function's control flow honest
                  * and skips the redundant foxr_log_event() call below,
                  * which foxr_handle_reboot() already made itself. */
    } else if(strcmp(tag, "FLPR/SETTINGS") == 0) {
        foxr_handle_settings_get(c);
    } else if(strcmp(tag, "FLPR/SETTINGS/NOTIF/SET") == 0) {
        foxr_handle_settings_notif_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/LOCALE/SET") == 0) {
        foxr_handle_settings_locale_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/THEME/SET") == 0) {
        foxr_handle_settings_theme_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/NIGHTSHIFT/SET") == 0) {
        foxr_handle_settings_nightshift_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/RGB/COLORS") == 0) {
        foxr_handle_settings_rgb_colors_get(c);
    } else if(strcmp(tag, "FLPR/SETTINGS/RGB/SET") == 0) {
        foxr_handle_settings_rgb_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/SYSTEM/SET") == 0) {
        foxr_handle_settings_system_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/INPUT/SET") == 0) {
        foxr_handle_settings_input_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/BT/SET") == 0) {
        foxr_handle_settings_bt_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/POWER/SET") == 0) {
        foxr_handle_settings_power_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/DESKTOP/SET") == 0) {
        foxr_handle_settings_desktop_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/ALARM/SET") == 0) {
        foxr_handle_settings_alarm_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/ALARM/LIST") == 0) {
        foxr_handle_settings_alarm_list(c);
    } else if(strcmp(tag, "FLPR/SETTINGS/ALARM/ADD") == 0) {
        foxr_handle_settings_alarm_add(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/ALARM/EDIT") == 0) {
        foxr_handle_settings_alarm_edit(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/ALARM/DELETE") == 0) {
        foxr_handle_settings_alarm_delete(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/SECURITY") == 0) {
        foxr_handle_settings_security_get(c);
    } else if(strcmp(tag, "FLPR/SETTINGS/SECURITY/SET") == 0) {
        foxr_handle_settings_security_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/SECURITY/PIN/SET") == 0) {
        foxr_handle_settings_security_pin_set(c, payload);
    } else if(strcmp(tag, "FLPR/SETTINGS/SECURITY/PIN/CLEAR") == 0) {
        foxr_handle_settings_security_pin_clear(c, payload);
    } else if(strcmp(tag, "FLPR/SESSION/END") == 0) {
        foxr_handle_session_end(c);
    }
    /* Unrecognized [FLPR/...] tags are silently ignored - forward compat. */

    /* Logged last, not first: by now c->last_reply holds whatever was
     * actually sent back (see foxr_reply()/foxr_replyf() above), so this
     * one call captures both the request and its real outcome - see
     * foxr_log_event()'s own header comment. Runs for every dispatched
     * command, "[FLPR/SCREEN/FRAME]" pushes and "[FLPR/FILES/READ/CHUNK]"
     * streamed payloads excepted (those aren't dispatched commands, they
     * never reach this function at all - see esp_at_router.h/foxr_handle_
     * files_read_start()), so a remote-control or file-transfer session
     * can't flood this log with per-frame/per-chunk noise. */
    foxr_log_event(c, tag, line);
}

/* ── Alloc / free ──────────────────────────────────────────────────── */

FoxrCompanion* foxr_companion_alloc(EspAt* esp_at, Gui* gui, volatile bool* restart_pending_flag) {
    FoxrCompanion* c = malloc(sizeof(FoxrCompanion));
    memset(c, 0, sizeof(FoxrCompanion));
    c->esp_at = esp_at;
    c->gui = gui;
    c->restart_pending_flag = restart_pending_flag;
    c->storage = furi_record_open(RECORD_STORAGE);
    c->input_events = furi_record_open(RECORD_INPUT_EVENTS);
    c->log_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    /* Ensure the persistent log's directory exists before the first
     * foxr_log_persist() call tries to append to it - storage_common_
     * mkdir() on an already-existing directory just returns FSE_EXIST,
     * which is fine to ignore here. */
    storage_common_mkdir(c->storage, FOXR_LOG_DIR);

    return c;
}

void foxr_companion_free(FoxrCompanion* c) {
    foxr_teardown_active_state(c);
    furi_mutex_free(c->log_mutex);
    furi_record_close(RECORD_INPUT_EVENTS);
    furi_record_close(RECORD_STORAGE);
    free(c);
}
