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

#define FOXR_MAX_NAME_LEN     255
#define FOXR_READ_CHUNK_RAW   900
#define FOXR_WRITE_CHUNK_RAW  400
#define FOXR_SCREEN_MIN_INTERVAL_MS 500

#define FOXR_LOG_DIR  EXT_PATH("apps_data/fox_lab")
#define FOXR_LOG_FILE EXT_PATH("apps_data/fox_lab/flpr_log.txt")

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

    FuriMutex* log_mutex;
    char log_lines[FOXR_LOG_LINES][FOXR_LOG_LINE_MAX];
    size_t log_count;
    size_t log_next;
    uint32_t log_version;

    char last_reply[FOXR_LAST_REPLY_MAX];

    File* write_file;
    bool write_active;

    bool screen_active;
    FuriThread* screen_thread;
    uint8_t* screen_frame_buf;
    size_t screen_frame_size;
    char* screen_line_buf;
    size_t screen_b64_capacity;

    uint32_t input_key_counter[InputKeyMAX];
    uint32_t input_counter;

    volatile bool* restart_pending_flag;
};

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

static void foxr_u64_to_str(uint64_t value, char* out, size_t out_capacity) {
    char digits[20];
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

static void foxr_log_event(FoxrCompanion* c, const char* tag, const char* request_line) {
    const char* status = "?";
    if(strstr(c->last_reply, "/OK") != NULL) {
        status = "OK";
    } else if(strstr(c->last_reply, "/ERR]") != NULL) {
        status = "ERR";
    }

    const char* short_tag = (strncmp(tag, "FLPR/", 5) == 0) ? tag + 5 : tag;
    char terminal_line[FOXR_LOG_LINE_MAX];
    snprintf(terminal_line, sizeof(terminal_line), "%s %s", short_tag, status);

    foxr_log_add(c, terminal_line);
    foxr_log_persist(c, terminal_line, request_line, c->last_reply);
}

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

        foxr_replyf(
            c,
            "[FLPR/STORAGE/OK]%lu,%lu",
            (unsigned long)(total / 1024),
            (unsigned long)(free_space / 1024));
    } else {
        foxr_replyf(c, "[FLPR/STORAGE/ERR]%s", foxr_fs_error_code(err));
    }
}

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

    char* name = malloc(FOXR_MAX_NAME_LEN + 1);
    char* name_b64 = malloc(352);
    char size_str[24];
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
    furi_kernel_lock();
    furi_thread_free(c->screen_thread);
    c->screen_thread = NULL;
    free(c->screen_frame_buf);
    c->screen_frame_buf = NULL;
    free(c->screen_line_buf);
    c->screen_line_buf = NULL;
    furi_kernel_unlock();
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

static void
    foxr_handle_reboot(FoxrCompanion* c, const char* tag, const char* line, const char* payload) {
    bool dfu = (payload[0] == '1');
    foxr_reply(c, "[FLPR/REBOOT/OK]");
    foxr_log_event(c, tag, line);

    furi_delay_ms(100);
    furi_hal_rtc_set_boot_mode(dfu ? FuriHalRtcBootModeDfu : FuriHalRtcBootModeNormal);
    furi_hal_power_reset();
}

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
    unsigned rgb_led1 = notification->settings.rgb.led_2_color_index;
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

static void foxr_handle_settings_rgb_colors_get(FoxrCompanion* c) {
    char buf[400];
    size_t len = 0;
    uint8_t count = rgb_backlight_get_color_count();
    for(uint8_t i = 0; i < count; i++) {
        const char* name = rgb_backlight_get_color_text(i);
        size_t name_len = strlen(name);

        if(len + name_len + 2 > sizeof(buf)) break;
        if(i > 0) buf[len++] = '|';
        memcpy(buf + len, name, name_len);
        len += name_len;
    }
    buf[len] = '\0';
    foxr_replyf(c, "[FLPR/SETTINGS/RGB/COLORS/OK]%s", buf);
}

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
    notification->settings.rgb.led_2_color_index = (uint8_t)led1;
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

    bool name_changed;
    if(name_len == 0) {
        FileInfo file_info;
        name_changed = (storage_common_stat(storage, NAMECHANGER_PATH, &file_info) == FSE_OK);
    } else {
        name_changed = (strcmp(furi_hal_version_get_name_ptr(), name) != 0);
    }

    bool name_saved = false;
    if(name_len == 0) {

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

    foxr_replyf(c, "[FLPR/SETTINGS/SYSTEM/SET/OK]%d", name_changed ? 1 : 0);
}

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

static const uint8_t foxr_pin_encode_k1[10] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2};
static const uint8_t foxr_pin_encode_k2[10] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1};

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
        return;

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

    foxr_log_event(c, tag, line);
}

FoxrCompanion* foxr_companion_alloc(EspAt* esp_at, Gui* gui, volatile bool* restart_pending_flag) {
    FoxrCompanion* c = malloc(sizeof(FoxrCompanion));
    memset(c, 0, sizeof(FoxrCompanion));
    c->esp_at = esp_at;
    c->gui = gui;
    c->restart_pending_flag = restart_pending_flag;
    c->storage = furi_record_open(RECORD_STORAGE);
    c->input_events = furi_record_open(RECORD_INPUT_EVENTS);
    c->log_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

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
