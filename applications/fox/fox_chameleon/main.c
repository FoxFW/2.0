#include "app.h"
#include "chameleon_protocol.h"
#include "key_dictionary.h"

#include <storage/storage.h>
#include <furi_hal_rtc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#define CHAMELEON_DUMP_BLOCK_COUNT 64

/* Caps the in-memory Terminal transcript (app->log) - the SD copy at
   app->terminal_log_path is never trimmed, only this RAM copy the
   Terminal view actually renders from, purely to bound memory use on a
   Flipper over a long session (e.g. a full dictionary search can emit a
   couple hundred "tried N/M" lines on its own). */
#define FOX_TERMINAL_LOG_MAX_CHARS 4000

static void action_check_esp32(App* app);
static void check_button_callback(GuiButtonType result, InputType type, void* context);
static void render_main_menu(App* app);
static void ensure_dir_path(Storage* storage, const char* path);
static void app_render_log(App* app);

typedef struct {
    FuriHalSerialId serial_id;
    const char* label;
} PinOption;

/* 15/16 (LPUART) is offered alongside the more common 13/14 (USART) pair
   per Flipper's own documented "two hardware UARTs" - see README.md for
   the caveat on FuriHalBusLPUART1 specifically. */
static const PinOption pin_options[] = {
    {FuriHalSerialIdUsart, "13/14 (USART)"},
    {FuriHalSerialIdLpuart, "15/16 (LPUART)"},
};
#define PIN_OPTION_COUNT (sizeof(pin_options) / sizeof(pin_options[0]))

static const uint32_t baud_options[] =
    {9600, 19200, 38400, 57600, 74880, 115200, 230400, 460800, 921600};
#define BAUD_OPTION_COUNT (sizeof(baud_options) / sizeof(baud_options[0]))
#define BAUD_OPTION_DEFAULT_INDEX 5 /* 115200, Fox ESP32 Firmware's stock default */

/* Largest real response this app decodes today is a 16-byte block read
   (26-byte frame); this leaves a healthy margin without needing
   ESP_AT_LINE_MAX (esp_at.h) to grow to accommodate it hex-encoded -
   "NOTIFY:" (7 chars) + 48 bytes hex-encoded (96 chars) = 103, safely
   under that 128-byte line limit. */
#define CHAMELEON_RESPONSE_BUFFER_MAX 48

static void hex_encode(const uint8_t* data, size_t length, FuriString* out) {
    furi_string_reset(out);
    for(size_t i = 0; i < length; i++) {
        furi_string_cat_printf(out, "%02X", data[i]);
    }
}

static size_t hex_decode(const char* hex, uint8_t* out, size_t out_capacity) {
    size_t len = strlen(hex);
    if(len == 0 || len % 2 != 0) return 0;
    size_t byte_count = len / 2;
    if(byte_count > out_capacity) return 0;

    for(size_t i = 0; i < byte_count; i++) {
        unsigned value = 0;
        if(sscanf(hex + i * 2, "%2x", &value) != 1) return 0;
        out[i] = (uint8_t)value;
    }
    return byte_count;
}

/* Parses one BLESCAN result line, e.g.:
     FOUND:d0:94:de:71:58:fc type:RANDOM(1) rssi:-81 name:ChameleonUltra
   into its MAC, RSSI, and name fields. Returns false (and touches
   nothing) if line isn't a FOUND: line or is missing a field this app
   actually uses - malformed input is simply not a candidate, not
   something to guess at. */
static bool parse_found_line(
    const char* line,
    char* mac_out,
    size_t mac_out_capacity,
    int* rssi_out,
    char* name_out,
    size_t name_out_capacity) {
    if(strncmp(line, "FOUND:", 6) != 0) return false;
    const char* mac_start = line + 6;

    const char* space = strchr(mac_start, ' ');
    if(space == NULL) return false;
    size_t mac_len = (size_t)(space - mac_start);
    if(mac_len == 0 || mac_len >= mac_out_capacity) return false;
    memcpy(mac_out, mac_start, mac_len);
    mac_out[mac_len] = '\0';

    const char* rssi_tag = strstr(space, "rssi:");
    if(rssi_tag == NULL) return false;
    *rssi_out = atoi(rssi_tag + 5);

    const char* name_tag = strstr(rssi_tag, "name:");
    if(name_tag == NULL) return false;
    strncpy(name_out, name_tag + 5, name_out_capacity - 1);
    name_out[name_out_capacity - 1] = '\0';

    return true;
}

/* Low-level: clears just the in-memory transcript. Only
   app_terminal_start_session() below calls this now - individual actions
   used to call it directly (wiping the screen on every button press),
   which is exactly what the user asked to stop: the Terminal now persists
   everything for the whole session, appending rather than resetting. */
static void app_log_reset(App* app) {
    furi_string_reset(app->log);
}

/* Appends one line to the SD copy of the current Terminal session -
   opened, written, and closed again on every call rather than kept open
   for the session's duration, specifically so the file on disk is always
   fully up to date even if the app crashes or is yanked off mid-session,
   per the user's "so this file will be up to date if the app suddenly
   closes" requirement. A no-op if no session is active yet (
   terminal_log_path empty) - see app_terminal_start_session(). */
static void app_terminal_log_line(App* app, const char* line) {
    if(furi_string_size(app->terminal_log_path) == 0) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(
           file, furi_string_get_cstr(app->terminal_log_path), FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_write(file, line, strlen(line));
        storage_file_write(file, "\n", 1);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* Starts a new Terminal session: clears the on-screen transcript and
   points app->terminal_log_path at a freshly dated file under
   FOX_CHAMELEON_LOG_DIR, which app_log() (below) will create on its very
   first write via FSOM_OPEN_APPEND. Called from action_start() (the
   ESP32 Firmware AT/OK bring-up), from app_alloc() when the
   launch-time auto-probe finds a working default, and again from
   action_connect() (each fresh Chameleon Ultra connect attempt) - see
   the comments at those call sites for why each gets its own
   session/file rather than just app launch getting one. */
static void app_terminal_start_session(App* app) {
    app_log_reset(app);
    app->terminal_scroll = 0;

    DateTime now;
    furi_hal_rtc_get_datetime(&now);

    FuriString* filename = furi_string_alloc_printf(
        "%s/%04u%02u%02u-%02u%02u%02u.txt",
        FOX_CHAMELEON_LOG_DIR,
        now.year,
        now.month,
        now.day,
        now.hour,
        now.minute,
        now.second);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    ensure_dir_path(storage, FOX_CHAMELEON_LOG_DIR);
    furi_record_close(RECORD_STORAGE);

    furi_string_set(app->terminal_log_path, filename);
    furi_string_free(filename);
}

static void app_log(App* app, const char* fmt, ...) {
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if(furi_string_size(app->log) > 0) furi_string_cat(app->log, "\n");
    furi_string_cat(app->log, buffer);

    if(furi_string_size(app->log) > FOX_TERMINAL_LOG_MAX_CHARS) {
        size_t excess = furi_string_size(app->log) - FOX_TERMINAL_LOG_MAX_CHARS;
        size_t cut = furi_string_search_char(app->log, '\n', excess);
        cut = (cut == FURI_STRING_FAILURE) ? excess : (cut + 1);
        furi_string_right(app->log, cut);
    }

    app_terminal_log_line(app, buffer);
}

static void app_render_log(App* app) {
    /* SIZE_MAX here just means "as far down as it goes" - terminal_draw_cb()
       clamps this against the real wrapped-line count on the very next
       draw. This is what makes "everything new I do just takes us back to
       this same page with the new stuff appended down the bottom" true
       even if the user had scrolled up to read history first. */
    app->terminal_scroll = (size_t)-1;
    app->current_view = FoxChameleonViewTerminal;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewTerminal);
}

/* config.txt is a flat key=value file, one pair per line:
     mac=AA:BB:CC:DD:EE:FF
     service=6e400001-b5a3-f393-e0a9-e50e24dcca9e
     write_char=6e400002-b5a3-f393-e0a9-e50e24dcca9e
     notify_char=6e400003-b5a3-f393-e0a9-e50e24dcca9e
   See README.md for where these UUIDs come from. */
static void app_load_config(App* app) {
    furi_string_reset(app->chameleon_mac);
    furi_string_reset(app->gatt_service_uuid);
    furi_string_reset(app->gatt_write_char_uuid);
    furi_string_reset(app->gatt_notify_char_uuid);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, FOX_CHAMELEON_CONFIG_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buffer[512];
        uint16_t read = storage_file_read(file, buffer, sizeof(buffer) - 1);
        buffer[read] = '\0';

        char* line = strtok(buffer, "\r\n");
        while(line != NULL) {
            char key[32] = {0};
            char value[64] = {0};
            if(sscanf(line, "%31[^=]=%63s", key, value) == 2) {
                if(strcmp(key, "mac") == 0) {
                    furi_string_set(app->chameleon_mac, value);
                } else if(strcmp(key, "service") == 0) {
                    furi_string_set(app->gatt_service_uuid, value);
                } else if(strcmp(key, "write_char") == 0) {
                    furi_string_set(app->gatt_write_char_uuid, value);
                } else if(strcmp(key, "notify_char") == 0) {
                    furi_string_set(app->gatt_notify_char_uuid, value);
                }
            }
            line = strtok(NULL, "\r\n");
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static bool app_expect_ok(App* app, uint32_t timeout_ms) {
    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + timeout_ms;

    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;

        app_log(app, "%s", msg.line);
        if(strcmp(msg.line, "OK") == 0) return true;
        if(strcmp(msg.line, "ERROR") == 0) return false;
    }
    return false;
}

/* Waits for a NOTIFY:<hex> line and decodes it into decode_buffer,
   which parsed->data will point into afterward - decode_buffer must be
   supplied and kept alive by the caller for as long as parsed is used,
   not owned by this function. An earlier version of this function used
   a local buffer for that decode and returned a ChameleonFrame pointing
   into it - undefined behavior the moment this function returned, only
   going unnoticed because nothing happened to run between that return
   and the caller reading parsed->data. Fixed by moving the buffer to
   the caller's own stack frame, where its lifetime actually matches
   parsed's. */
static bool app_await_chameleon_response(
    App* app,
    uint8_t* decode_buffer,
    size_t decode_buffer_capacity,
    ChameleonFrame* parsed,
    uint32_t timeout_ms) {
    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + timeout_ms;

    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;

        if(strncmp(msg.line, "NOTIFY:", 7) == 0) {
            size_t decoded_len = hex_decode(msg.line + 7, decode_buffer, decode_buffer_capacity);
            if(decoded_len == 0) {
                /* Either an odd number of hex digits, an invalid hex
                   character, or more bytes than decode_buffer_capacity -
                   log the raw line so a UART framing/corruption problem
                   is distinguishable from a real protocol bug just by
                   reading the log afterward. */
                app_log(app, "Malformed response frame");
                app_log(app, "hex decode failed, raw line:");
                app_log(app, "%.100s", msg.line);
                return false;
            }
            ChameleonFrameError parse_error = ChameleonFrameOk;
            if(!chameleon_parse_frame_ex(decode_buffer, decoded_len, parsed, &parse_error)) {
                app_log(app, "Malformed response frame");
                app_log(app, "%s", chameleon_frame_error_str(parse_error));
                app_log(app, "%u bytes decoded:", (unsigned)decoded_len);
                FuriString* rehex = furi_string_alloc();
                hex_encode(decode_buffer, decoded_len, rehex);
                app_log(app, "%.100s", furi_string_get_cstr(rehex));
                furi_string_free(rehex);
                return false;
            }
            return true;
        }
        app_log(app, "%s", msg.line);
    }

    app_log(app, "No response from Chameleon");
    return false;
}

/* Shared write-then-wait sequence: hex-encodes a pre-built Chameleon
   frame, sends it as BLEWRITE:<hex>, waits for OK, then waits for the
   resulting NOTIFY:<hex>. Every action that talks to the Chameleon goes
   through this. decode_buffer/decode_buffer_capacity are forwarded to
   app_await_chameleon_response() - see its comment for why the caller
   owns that buffer rather than this function. */
static bool app_write_command_and_await(
    App* app,
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* decode_buffer,
    size_t decode_buffer_capacity,
    ChameleonFrame* parsed) {
    FuriString* hex = furi_string_alloc();
    hex_encode(frame, frame_len, hex);

    FuriString* cmd = furi_string_alloc_printf("BLEWRITE:%s", furi_string_get_cstr(hex));
    furi_string_free(hex);
    esp_at_send(app->esp_at, furi_string_get_cstr(cmd));
    furi_string_free(cmd);

    if(!app_expect_ok(app, 3000)) {
        app_log(app, "Write failed");
        return false;
    }

    return app_await_chameleon_response(app, decode_buffer, decode_buffer_capacity, parsed, 4000);
}

/* Runs at launch, and again whenever the user presses Retry on the
   failure screen. Plain AT answered with a plain OK is specific enough
   to tell a real, responsive Fox ESP32 Firmware module apart from
   silence or nothing connected at all. Until this succeeds once (or is
   deliberately skipped - see the Skip button below), the submenu is
   never reachable. */
#define RAW_CAPTURE_DIR "/ext/apps_data/fox_chameleon/debug"
#define RAW_CAPTURE_ROW_LEN 16

/* Drains whatever esp_at_raw_capture_start() has been collecting since
   it was last called, and writes it as a hex dump (offset, hex bytes,
   printable-ASCII sidebar) to RAW_CAPTURE_DIR/filename on the SD card.
   Unlike the on-screen log, this shows every byte that arrived - not
   just the lines the parser recognized - which is the point: it exists
   to answer "what did this UART actually send back", not "what did our
   parser make of it". */
/* storage_common_mkdir() creating only the deepest folder in one call is
   not something this codebase had actually confirmed handles missing
   parent directories - and /ext/apps_data/fox_chameleon/ itself is never
   created anywhere else in this app (config.txt is only ever read, never
   written), so on a fresh SD card that parent folder may not exist yet
   at all. This walks the path and creates every level in order, which
   works regardless of whether the single-call version is recursive, and
   is a no-op (safe to call repeatedly) for levels that already exist. */
static void ensure_dir_path(Storage* storage, const char* path) {
    char buffer[128];
    strncpy(buffer, path, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    for(size_t i = 1; buffer[i] != '\0'; i++) {
        if(buffer[i] == '/') {
            buffer[i] = '\0';
            storage_common_mkdir(storage, buffer);
            buffer[i] = '/';
        }
    }
    storage_common_mkdir(storage, buffer);
}

/* Fills in service/write_char/notify_char with the confirmed-working
   NUS defaults (see app.h) wherever config.txt didn't already specify
   its own - called once at startup, right after app_load_config(), so
   BLESVC/BLECHAR always have something real to try even on a config.txt
   that's never been touched. */
static void app_ensure_config_defaults(App* app) {
    if(furi_string_size(app->gatt_service_uuid) == 0) {
        furi_string_set(app->gatt_service_uuid, FOX_CHAMELEON_DEFAULT_SERVICE_UUID);
    }
    if(furi_string_size(app->gatt_write_char_uuid) == 0) {
        furi_string_set(app->gatt_write_char_uuid, FOX_CHAMELEON_DEFAULT_WRITE_CHAR_UUID);
    }
    if(furi_string_size(app->gatt_notify_char_uuid) == 0) {
        furi_string_set(app->gatt_notify_char_uuid, FOX_CHAMELEON_DEFAULT_NOTIFY_CHAR_UUID);
    }
}

/* Writes the app's current mac/service/write_char/notify_char to
   config.txt, creating /ext/apps_data/fox_chameleon/ first if it
   doesn't exist yet - this is what makes config.txt something the app
   maintains, rather than something the user is expected to hand-create
   before first use. Called once at startup (so a fresh install gets a
   fully populated file immediately, defaults included) and again
   whenever a MAC is discovered or chosen via the candidate list in
   action_scan_for_chameleons(). */
static void app_save_config(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    ensure_dir_path(storage, FOX_CHAMELEON_CONFIG_DIR);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, FOX_CHAMELEON_CONFIG_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* content = furi_string_alloc_printf(
            "mac=%s\nservice=%s\nwrite_char=%s\nnotify_char=%s\n",
            furi_string_get_cstr(app->chameleon_mac),
            furi_string_get_cstr(app->gatt_service_uuid),
            furi_string_get_cstr(app->gatt_write_char_uuid),
            furi_string_get_cstr(app->gatt_notify_char_uuid));
        storage_file_write(
            file, furi_string_get_cstr(content), (uint16_t)furi_string_size(content));
        furi_string_free(content);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static bool dump_raw_capture_to_sd(App* app, const char* filename, const char* header) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    ensure_dir_path(storage, RAW_CAPTURE_DIR);
    File* file = storage_file_alloc(storage);

    FuriString* path = furi_string_alloc_printf("%s/%s", RAW_CAPTURE_DIR, filename);
    bool opened =
        storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS);
    furi_string_free(path);

    if(opened) {
        char line[96];
        size_t n = (size_t)snprintf(line, sizeof(line), "%.90s\n\n", header);
        storage_file_write(file, line, (uint16_t)n);

        uint8_t buf[RAW_CAPTURE_ROW_LEN];
        size_t total = 0;
        size_t got;
        while((got = esp_at_raw_capture_read(app->esp_at, buf, sizeof(buf), 50)) > 0) {
            size_t pos = (size_t)snprintf(line, sizeof(line), "%04X  ", (unsigned)total);
            for(size_t i = 0; i < RAW_CAPTURE_ROW_LEN; i++) {
                if(i < got) {
                    pos += (size_t)snprintf(line + pos, sizeof(line) - pos, "%02X ", buf[i]);
                } else {
                    pos += (size_t)snprintf(line + pos, sizeof(line) - pos, "   ");
                }
            }
            pos += (size_t)snprintf(line + pos, sizeof(line) - pos, " ");
            for(size_t i = 0; i < got && pos < sizeof(line) - 2; i++) {
                char c = (char)buf[i];
                line[pos++] = (c >= 32 && c < 127) ? c : '.';
            }
            line[pos++] = '\n';
            storage_file_write(file, line, (uint16_t)pos);
            total += got;
        }

        if(total == 0) {
            const char* msg = "No bytes captured - nothing arrived on this UART during this attempt.\n";
            storage_file_write(file, msg, (uint16_t)strlen(msg));
        } else {
            char footer[48];
            size_t fn =
                (size_t)snprintf(footer, sizeof(footer), "\n%u bytes total.\n", (unsigned)total);
            storage_file_write(file, footer, (uint16_t)fn);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return opened;
}

static void log_dump_result(App* app, bool saved, const char* filename) {
    if(saved) {
        app_log(app, "Raw dump: debug/%s", filename);
    } else {
        app_log(app, "Could not save debug/%s", filename);
        app_log(app, "Check the SD card is");
        app_log(app, "inserted and writable.");
    }
}

static void action_check_esp32(App* app) {
    app_log(app, "Checking for ESP32...");
    app_render_log(app);

    esp_at_send(app->esp_at, "AT");
    app->esp32_detected = app_expect_ok(app, 2500);

    if(app->esp32_detected) {
        app->current_view = FoxChameleonViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewMenu);
        return;
    }

    bool dumped =
        dump_raw_capture_to_sd(app, "check_esp32_raw.txt", "Raw bytes seen while waiting for an AT reply.");

    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget,
        2,
        2,
        AlignLeft,
        AlignTop,
        FontSecondary,
        dumped ? "This app requires an\n"
                 "external ESP32 running\n"
                 "Fox ESP32 Firmware,\n"
                 "wired to the\n"
                 "selected GPIO pins.\n"
                 "\n"
                 "No response. Raw dump\n"
                 "in debug/ on SD card."
               : "This app requires an\n"
                 "external ESP32 running\n"
                 "Fox ESP32 Firmware,\n"
                 "wired to the\n"
                 "selected GPIO pins.\n"
                 "\n"
                 "No response, and the\n"
                 "debug dump failed too\n"
                 "- check the SD card.");
    widget_add_button_element(app->widget, GuiButtonTypeCenter, "Retry", check_button_callback, app);
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Skip", check_button_callback, app);
    app->current_view = FoxChameleonViewMessage;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewMessage);
}

static void check_button_callback(GuiButtonType result, InputType type, void* context) {
    App* app = context;
    if(type != InputTypeShort) return;

    if(result == GuiButtonTypeCenter) {
        action_check_esp32(app);
    } else if(result == GuiButtonTypeLeft) {
        /* Deliberately overrides a check that didn't pass, for cases
           where the ESP32 may genuinely be receiving commands without
           being able to reply on this wiring (see README - a real,
           evidenced possibility for some boards, not just a hopeful
           guess) - Connect and other actions will still each report
           their own real success or failure and still write a raw
           capture dump, so proceeding here doesn't hide anything, it
           just stops assuming silence during the startup check means
           nothing else is worth trying. */
        app->esp32_detected = true;
        app->current_view = FoxChameleonViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewMenu);
    }
}

/* The BLECONN -> BLESVC -> BLECHAR sequence, reusable regardless of
   whether mac came from a saved config.txt or was just chosen from a
   scan. service/write_char/notify_char are guaranteed non-empty by
   app_ensure_config_defaults() (called once at startup), so unlike an
   earlier version of this function, there's no missing-config case to
   handle here anymore - it's always got something real to try. */
static bool action_connect_with_mac(App* app, const char* mac) {
    FuriString* cmd = furi_string_alloc_printf("BLECONN:%s", mac);
    esp_at_send(app->esp_at, furi_string_get_cstr(cmd));
    furi_string_free(cmd);

    app_log(app, "Connecting to Chameleon,");
    app_log(app, "can take up to 45s...");
    app_render_log(app);

    /* Fox ESP32 Firmware tries BLE_ADDR_TYPE_PUBLIC then
       BLE_ADDR_TYPE_RANDOM in sequence (fox_esp32_firmware's BLECONN
       handler), and real-world reports for this exact BLE library
       document individual connect() attempts taking up to 30-60 seconds
       to fail on a marginal link - two such attempts back to back can
       comfortably exceed a much shorter wait. A real connect that
       eventually succeeded once showed up as a clean timeout here -
       zero bytes at all in connect_raw.txt, not an ERROR - because an
       earlier, shorter timeout gave up before the firmware had
       finished trying. */
    app->ble_connected = app_expect_ok(app, 45000);

    if(!app->ble_connected) {
        app_log(app, "Connection failed");
        bool dumped = dump_raw_capture_to_sd(
            app, "connect_raw.txt", "Raw bytes seen during the full Connect sequence.");
        log_dump_result(app, dumped, "connect_raw.txt");
        render_main_menu(app);
        app_render_log(app);
        return false;
    }

    cmd = furi_string_alloc_printf("BLESVC:%s", furi_string_get_cstr(app->gatt_service_uuid));
    esp_at_send(app->esp_at, furi_string_get_cstr(cmd));
    furi_string_free(cmd);
    if(!app_expect_ok(app, 3000)) {
        app_log(app, "Service not found");
        bool dumped = dump_raw_capture_to_sd(
            app, "connect_raw.txt", "Raw bytes seen during the full Connect sequence.");
        log_dump_result(app, dumped, "connect_raw.txt");
        app->ble_connected = false;
        render_main_menu(app);
        app_render_log(app);
        return false;
    }

    cmd = furi_string_alloc_printf(
        "BLECHAR:%s,%s",
        furi_string_get_cstr(app->gatt_write_char_uuid),
        furi_string_get_cstr(app->gatt_notify_char_uuid));
    esp_at_send(app->esp_at, furi_string_get_cstr(cmd));
    furi_string_free(cmd);
    bool chars_ok = app_expect_ok(app, 3000);

    app_log(app, chars_ok ? "Connected" : "Characteristics not found");
    bool dumped =
        dump_raw_capture_to_sd(app, "connect_raw.txt", "Raw bytes seen during the full Connect sequence.");
    if(!chars_ok) {
        log_dump_result(app, dumped, "connect_raw.txt");
        app->ble_connected = false;
    }
    /* render_main_menu() here matters now, not just cosmetically - it's
       what reveals (on success) or keeps hidden (on failure) the whole
       Get/Read/Dump command block, which render_main_menu() now only
       shows while ble_connected is true. */
    render_main_menu(app);
    app_render_log(app);
    return app->ble_connected;
}

/* View's draw callback signature only gives access to the tiny model
   allocated for it, not the context set via view_set_context() - only
   the input callback gets that. Fox File Browser's Favourites view
   (ffb.c) hits the same constraint and works around it with exactly
   this pattern: a static pointer set once at app_alloc() time, read
   from inside the draw callback. The model itself is a throwaway
   uint8_t, touched only to ask the view system to redraw - the real
   data (candidates/candidate_selected/candidate_scroll) lives on App,
   reached through this pointer instead. */
static App* s_candidate_view_app = NULL;

#define CANDIDATE_ROW_HEADER_H 14
#define CANDIDATE_ROW_H        22
#define CANDIDATE_ROW_VIS      2

static void candidate_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_candidate_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Select a Chameleon");

    if(app->candidate_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "None found");
        return;
    }

    for(size_t i = app->candidate_scroll;
        i < app->candidate_count && (i - app->candidate_scroll) < CANDIDATE_ROW_VIS;
        i++) {
        int row = (int)(i - app->candidate_scroll);
        int ry = CANDIDATE_ROW_HEADER_H + row * CANDIDATE_ROW_H;
        int by = ry + 1;
        int bh = CANDIDATE_ROW_H - 2;
        bool selected = (i == app->candidate_selected);

        if(selected) {
            canvas_draw_rbox(canvas, 2, by, 124, bh, 3);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, 2, by, 124, bh, 3);
        }

        char line1[40];
        snprintf(line1, sizeof(line1), "Chameleon Ultra [%ddBm]", app->candidates[i].rssi);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, by + 5, AlignCenter, AlignCenter, line1);

        char line2[32];
        snprintf(line2, sizeof(line2), "MAC: %.23s", app->candidates[i].mac);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, by + 15, AlignCenter, AlignCenter, line2);

        canvas_set_color(canvas, ColorBlack);
    }

    /* Scroll indicator on the right edge, only drawn when there's
       actually more than fits on screen - same geometry as Fox File
       Browser's ffv_draw_scroll(). */
    if(app->candidate_count > CANDIDATE_ROW_VIS) {
        int available_h = 64 - CANDIDATE_ROW_HEADER_H;
        int bar_h = (int)(available_h * CANDIDATE_ROW_VIS / app->candidate_count);
        if(bar_h < 3) bar_h = 3;
        int bar_y =
            CANDIDATE_ROW_HEADER_H + (int)(available_h * app->candidate_scroll / app->candidate_count);
        canvas_draw_box(canvas, 125, bar_y, 3, bar_h);
    }
}

/* Shared by both the "exactly one candidate" auto-path and the
   "user picked one from the list" path in candidate_input_cb() below. */
static void candidate_select(App* app, size_t index) {
    if(index >= app->candidate_count) return;
    furi_string_set(app->chameleon_mac, app->candidates[index].mac);
    app_save_config(app);
    action_connect_with_mac(app, app->candidates[index].mac);
}

static bool candidate_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(app->candidate_count == 0) return false;

    switch(event->key) {
    case InputKeyUp:
        if(app->candidate_selected > 0) {
            app->candidate_selected--;
            if(app->candidate_selected < app->candidate_scroll) {
                app->candidate_scroll = app->candidate_selected;
            }
        } else {
            app->candidate_selected = app->candidate_count - 1;
            app->candidate_scroll = (app->candidate_count > CANDIDATE_ROW_VIS) ?
                                         app->candidate_count - CANDIDATE_ROW_VIS :
                                         0;
        }
        with_view_model(app->candidate_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        if(app->candidate_selected + 1 < app->candidate_count) {
            app->candidate_selected++;
            if(app->candidate_selected >= app->candidate_scroll + CANDIDATE_ROW_VIS) {
                app->candidate_scroll = app->candidate_selected - CANDIDATE_ROW_VIS + 1;
            }
        } else {
            app->candidate_selected = 0;
            app->candidate_scroll = 0;
        }
        with_view_model(app->candidate_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyOk:
    case InputKeyRight:
        candidate_select(app, app->candidate_selected);
        return true;
    case InputKeyBack:
    case InputKeyLeft:
        return false; /* navigation_callback sends this back to the menu */
    default:
        return false;
    }
}

/* --- Terminal: the persistent, scrollable log/status view. ---
   Same "static App* pointer set once, read from the draw callback"
   pattern as candidate_draw_cb() above, for the same reason - the draw
   callback only gets the tiny throwaway model allocated for it, not the
   context set via view_set_context() (only the input callback gets
   that). */
static App* s_terminal_view_app = NULL;

#define TERMINAL_HEADER_H         10
#define TERMINAL_MAX_WRAPPED_LINES 256
#define TERMINAL_MEASURE_BUF_MAX  136
#define TERMINAL_HIDE_BTN_H       11

/* How many characters conservatively fit in max_width, using 'W' (a wide
   glyph) as the worst case. This trades a little unused right margin for
   a wrapping implementation simple enough to be confident is correct
   without a real device to test it on - see the top-level project notes
   on "verify first" and not guessing at unfamiliar APIs; pixel-perfect
   greedy word-wrap against canvas_string_width() per character was the
   first draft of this and was a lot more code for a benefit that doesn't
   matter on a screen this small. */
static size_t terminal_chars_per_line(Canvas* canvas, int32_t max_width) {
    uint16_t w = canvas_string_width(canvas, "W");
    if(w == 0) w = 6;
    size_t n = (size_t)(max_width / w);
    return n < 4 ? 4 : n;
}

typedef struct {
    uint16_t offset;
    uint16_t length;
} TerminalWrapLine;

/* Wraps `text` (app->log's underlying buffer, '\n'-separated logical
   lines) into `out`, each entry an {offset,length} slice into `text`
   itself - no copies. Each logical line is chopped into chars_per_line-
   sized chunks, preferring to break on the last space within a chunk
   (skipping that space) so words aren't split when a nearby space is
   available; a chunk with no space at all (e.g. a raw hex dump line) is
   hard-broken at exactly chars_per_line. An empty logical line still
   produces one empty row, so blank app_log() separators remain visible
   as blank rows rather than disappearing. If wrapping would produce more
   rows than out_capacity, the OLDEST rows are the ones left off (the
   loop simply stops appending once out_capacity is hit) - the tail
   (newest content, what app_render_log() scrolls to by default) always
   makes it in. */
static size_t terminal_wrap_log(
    const char* text,
    size_t text_len,
    size_t chars_per_line,
    TerminalWrapLine* out,
    size_t out_capacity) {
    size_t count = 0;
    size_t line_start = 0;

    while(line_start <= text_len && count < out_capacity) {
        size_t line_end = line_start;
        while(line_end < text_len && text[line_end] != '\n') line_end++;

        if(line_end == line_start) {
            out[count].offset = (uint16_t)line_start;
            out[count].length = 0;
            count++;
        } else {
            size_t pos = line_start;
            while(pos < line_end && count < out_capacity) {
                size_t remaining = line_end - pos;
                size_t take = remaining < chars_per_line ? remaining : chars_per_line;
                size_t chunk_end = pos + take;

                if(take == chars_per_line && chunk_end < line_end) {
                    size_t min_break = pos + (chars_per_line / 3);
                    for(size_t i = chunk_end; i > pos && i > min_break; i--) {
                        if(text[i - 1] == ' ') {
                            chunk_end = i - 1;
                            break;
                        }
                    }
                }

                out[count].offset = (uint16_t)pos;
                out[count].length = (uint16_t)(chunk_end - pos);
                count++;
                pos = chunk_end;
                if(pos < line_end && text[pos] == ' ') pos++;
            }
        }

        if(line_end == text_len) break;
        line_start = line_end + 1;
    }

    return count;
}

/* Small hand-drawn left-pointing arrow (a shaft plus a two-line
   arrowhead) - vector primitives rather than a text glyph or icon
   asset, so it doesn't depend on a Unicode arrow being present in this
   app's fonts, or on bundled icon resources this app doesn't currently
   link against. (x, y) is the arrow's tip (leftmost point), vertically
   centered on y. Every stroke is drawn twice, offset by 1px, to fake a
   bolder line - canvas_draw_line() has no width parameter of its own.
   Occupies roughly a 9x8px box from the tip. */
#define TERMINAL_BACK_ARROW_W 9
static void terminal_draw_back_arrow(Canvas* canvas, int32_t x, int32_t y) {
    for(int32_t dy = 0; dy < 2; dy++) {
        canvas_draw_line(canvas, x, y + dy, x + 6, y + dy);
        canvas_draw_line(canvas, x, y + dy, x + 3, y - 3 + dy);
        canvas_draw_line(canvas, x, y + dy, x + 3, y + 3 + dy);
    }
}

static void terminal_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_terminal_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);

    /* Permanent black header - the whole point is that the instant one
       of these pages comes up mid-command, its look alone tells the user
       "this is just info, Back will dismiss it", per the user's own
       request. "TERMINAL" is the only thing on this row now (the old
       "<- to Hide" text moved to the bottom-right button below), so it's
       centered rather than left-aligned. */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, TERMINAL_HEADER_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, TERMINAL_HEADER_H / 2, AlignCenter, AlignCenter, "TERMINAL");
    canvas_set_color(canvas, ColorBlack);

    const char* text = furi_string_get_cstr(app->log);
    size_t text_len = furi_string_size(app->log);

    int32_t max_width = 122; /* leaves room for the scrollbar on the right */
    size_t chars_per_line = terminal_chars_per_line(canvas, max_width);

    static TerminalWrapLine lines[TERMINAL_MAX_WRAPPED_LINES];
    size_t total = terminal_wrap_log(text, text_len, chars_per_line, lines, TERMINAL_MAX_WRAPPED_LINES);

    size_t line_height = canvas_current_font_height(canvas);
    if(line_height == 0) line_height = 8;
    size_t content_top = TERMINAL_HEADER_H + 1;
    size_t content_height = 64 - content_top;
    size_t visible_rows = content_height / line_height;
    if(visible_rows == 0) visible_rows = 1;

    size_t max_scroll = total > visible_rows ? total - visible_rows : 0;
    if(app->terminal_scroll > max_scroll) app->terminal_scroll = max_scroll;

    for(size_t row = 0; row < visible_rows && (app->terminal_scroll + row) < total; row++) {
        const TerminalWrapLine* wl = &lines[app->terminal_scroll + row];
        char buf[TERMINAL_MEASURE_BUF_MAX];
        size_t n = wl->length < (TERMINAL_MEASURE_BUF_MAX - 1) ? wl->length :
                                                                  (TERMINAL_MEASURE_BUF_MAX - 1);
        memcpy(buf, text + wl->offset, n);
        buf[n] = '\0';
        int32_t y = (int32_t)(content_top + row * line_height + line_height - 1);
        canvas_draw_str(canvas, 2, y, buf);
    }

    /* Scrollbar with a position dot - same visual language as
       candidate_draw_cb()'s scroll indicator above and Fox File
       Browser's ffv_draw_scroll(), just spanning the whole content area
       instead of two rows. Only drawn when there's actually more than
       fits on screen. */
    if(total > visible_rows) {
        int32_t bar_x = 126;
        int32_t bar_top = (int32_t)content_top;
        int32_t bar_h = (int32_t)content_height;
        canvas_draw_line(canvas, bar_x, bar_top, bar_x, bar_top + bar_h);

        int32_t dot_h = bar_h * (int32_t)visible_rows / (int32_t)total;
        if(dot_h < 3) dot_h = 3;
        int32_t dot_y =
            bar_top + (bar_h - dot_h) * (int32_t)app->terminal_scroll / (int32_t)max_scroll;
        canvas_draw_box(canvas, bar_x - 1, dot_y, 3, dot_h);
    }

    /* Bottom-right "Hide" button - drawn last, on top of the text rows
       and clear of the scrollbar (which occupies x >= 125), per the
       user's request to move "<- to Hide" out of the header. The white
       fill first clears whatever text pixels might otherwise be
       underneath before the border/icon/label are drawn - the user
       confirmed by eye that the wrapped log text never actually reaches
       this corner in practice, but clearing first makes that robust
       either way.

       Width is measured from the actual "Hide" text rather than a fixed
       guess, same approach terminal_chars_per_line() already uses for
       wrapping - this is what guarantees a real, consistent gap between
       the arrow icon and the label instead of them touching, regardless
       of the exact pixel width FontSecondary renders "Hide" at. */
    {
        canvas_set_font(canvas, FontSecondary);
        uint16_t hide_text_w = canvas_string_width(canvas, "Hide");

        int32_t pad = 3;
        int32_t icon_gap = 4; /* space between the arrow and "Hide" */
        int32_t btn_w = pad + TERMINAL_BACK_ARROW_W + icon_gap + (int32_t)hide_text_w + pad;

        int32_t btn_x2 = 124;
        int32_t btn_x1 = btn_x2 - btn_w;
        int32_t btn_y2 = 63;
        int32_t btn_y1 = btn_y2 - TERMINAL_HIDE_BTN_H;

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, btn_x1, btn_y1, btn_w, TERMINAL_HIDE_BTN_H);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, btn_x1, btn_y1, btn_w, TERMINAL_HIDE_BTN_H, 2);

        terminal_draw_back_arrow(canvas, btn_x1 + pad, btn_y1 + TERMINAL_HIDE_BTN_H / 2);

        canvas_draw_str_aligned(
            canvas, btn_x2 - pad, btn_y1 + TERMINAL_HIDE_BTN_H / 2, AlignRight, AlignCenter, "Hide");
    }
}

static bool terminal_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        if(app->terminal_scroll > 0) app->terminal_scroll--;
        with_view_model(app->terminal_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        /* Over-incrementing past the real max is harmless -
           terminal_draw_cb() clamps it against the actual wrapped-line
           count on the very next draw, same trick app_render_log() uses
           with SIZE_MAX to mean "scroll to bottom". */
        app->terminal_scroll++;
        with_view_model(app->terminal_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyBack:
    case InputKeyLeft:
        return false; /* navigation_callback hides the Terminal */
    default:
        return false;
    }
}

/* Runs BLESCAN, keeps only results whose advertised name contains
   "Chameleon", and either connects automatically (exactly one match),
   shows the selectable two-line list above (more than one), or reports
   nothing found (zero). Whichever MAC is settled on - automatic or
   chosen - is saved to config.txt before connecting, which is what
   makes config.txt something this app maintains rather than something
   the user has to prepare by hand. */
static void action_scan_for_chameleons(App* app) {
    app_log(app, "Scanning for Chameleon");
    app_log(app, "Ultra devices nearby...");
    app_render_log(app);

    esp_at_send(app->esp_at, "BLESCAN");

    app->candidate_count = 0;
    EspAtMsg msg;
    /* fox_esp32_firmware's BLE_SCAN_SECONDS is 5; this leaves margin for
       the FOUND: lines themselves to arrive and be processed. */
    uint32_t deadline = furi_get_tick() + 8000;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        if(strcmp(msg.line, "SCANDONE") == 0) break;

        char mac[24];
        int rssi = 0;
        char name[32];
        if(parse_found_line(msg.line, mac, sizeof(mac), &rssi, name, sizeof(name)) &&
           strstr(name, "Chameleon") != NULL && app->candidate_count < CHAMELEON_CANDIDATE_MAX) {
            strncpy(
                app->candidates[app->candidate_count].mac,
                mac,
                sizeof(app->candidates[0].mac) - 1);
            app->candidates[app->candidate_count].mac[sizeof(app->candidates[0].mac) - 1] = '\0';
            app->candidates[app->candidate_count].rssi = rssi;
            app->candidate_count++;
        }
    }

    if(app->candidate_count == 0) {
        app_log(app, "No Chameleon Ultra");
        app_log(app, "devices found nearby.");
        app_render_log(app);
        return;
    }

    if(app->candidate_count == 1) {
        candidate_select(app, 0);
        return;
    }

    app->candidate_selected = 0;
    app->candidate_scroll = 0;
    app->current_view = FoxChameleonViewCandidateMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewCandidateMenu);
}

/* Sends BLEDISC and waits briefly, but only if a Chameleon Ultra is
   currently connected - shared by Connect to C.U and Search for C.U
   below, both of which need a clean slate before trying a new link.
   Real-hardware behavior for sending BLECONN while already connected to
   a different device was never confirmed against the Fox ESP32
   Firmware, so disconnecting first is this project's own defensive choice, not a
   verified requirement. render_main_menu() re-hides the Get/Read/Dump
   command block the instant ble_connected flips false, rather than
   leaving it visible-but-guaranteed-to-fail until some other action
   happens to redraw the menu. */
static void action_disconnect_current_cu_if_any(App* app) {
    if(!app->ble_connected) return;
    esp_at_send(app->esp_at, "BLEDISC");
    app_expect_ok(app, 3000);
    app->ble_connected = false;
    render_main_menu(app);
}

static void action_connect(App* app) {
    /* Fresh Terminal session per the user's spec: "dateandtime connection
       started" - a new dated log file, and the on-screen scrollback
       starts clean for this connect attempt (see
       app_terminal_start_session() for the other places this happens -
       app_alloc()'s launch-time auto-probe and action_start(), i.e. the
       ESP32 Firmware bring-up phase, each of which gets its
       own session/file too). Everything from here on (BLEINIT, BLECONN,
       BLESVC, BLECHAR, every subsequent menu command run against this
       connection) just appends. Only reached from the main menu's
       "Connect" item, which only shows while ble_initialized is false -
       see MenuIndex's comment in app.h - so this always represents a
       genuinely fresh BLE session, never a re-run against an
       already-initialized one. */
    app_terminal_start_session(app);
    app_log(app, "Starting BLE link");
    app_render_log(app);

    /* Restarted here, not left running continuously since Start was
       pressed, so the dump at the end of this function reflects only
       this Connect attempt's traffic - not stale bytes from the AT
       check or an earlier failed Connect press. */
    esp_at_raw_capture_start(app->esp_at);

    esp_at_send(app->esp_at, "AT");
    app_expect_ok(app, 2000);

    esp_at_send(app->esp_at, "BLEINIT");
    if(!app_expect_ok(app, 3000)) {
        app_log(app, "BLE init failed");
        bool dumped = dump_raw_capture_to_sd(app, "connect_raw.txt", "Raw bytes seen during AT / BLEINIT.");
        log_dump_result(app, dumped, "connect_raw.txt");
        app_render_log(app);
        return;
    }
    app->ble_initialized = true;
    render_main_menu(app);

    if(furi_string_size(app->chameleon_mac) > 0) {
        if(action_connect_with_mac(app, furi_string_get_cstr(app->chameleon_mac))) {
            return;
        }
        app_log(app, "Saved MAC didn't work -");
        app_log(app, "scanning instead...");
        app_render_log(app);
    }

    action_scan_for_chameleons(app);
}

/* "Connect to C.U" in the Connection submenu - reconnects using whatever
   MAC is currently saved in config.txt, without re-running BLEINIT
   (already done - that's what makes this item reachable at all, see
   MenuIndex's comment in app.h). Does NOT fall back to scanning if no
   MAC is saved; Search for C.U is the explicit way to do that. */
static void action_reconnect_saved(App* app) {
    if(furi_string_size(app->chameleon_mac) == 0) {
        app_log(app, "No saved Chameleon Ultra");
        app_log(app, "MAC. Use Search for C.U");
        app_log(app, "instead.");
        app_render_log(app);
        return;
    }

    action_disconnect_current_cu_if_any(app);
    action_connect_with_mac(app, furi_string_get_cstr(app->chameleon_mac));
}

/* "Search for C.U" in the Connection submenu - re-runs the same BLESCAN
   flow as the initial Connect (auto-connects on exactly one match,
   otherwise shows the selectable list; see action_scan_for_chameleons()).
   With only one Chameleon Ultra nearby, this ends up doing the same
   thing as Connect to C.U above - expected, per the user's own
   description of these two items, not a bug. */
static void action_search_for_cu(App* app) {
    action_disconnect_current_cu_if_any(app);
    action_scan_for_chameleons(app);
}

/* "Disconnect BLE" in the Connection submenu - turns BLE off for this
   session: BLEDISC if still connected, then clears ble_connected and
   ble_initialized so item 0 on the main menu reverts to "Connect".
   Deliberately does NOT free the esp_at/UART session - the ESP32 stays
   claimed, so pressing Connect afterward works immediately with no need
   to revisit Settings or press Start again. Full teardown + exiting the
   app entirely is the separate "Disconnect GPIO & Exit" item at the bottom
   of the main menu - see action_disconnect_and_quit(). */
static void action_disconnect_ble(App* app) {
    action_disconnect_current_cu_if_any(app);

    app->ble_initialized = false;
    render_main_menu(app);

    app_log(app, "BLE turned off.");
    app_log(app, "Press Connect to start");
    app_log(app, "a new BLE session.");
    app->current_view = FoxChameleonViewMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewMenu);
    app_render_log(app);
}

static void action_open_connection_menu(App* app) {
    app->current_view = FoxChameleonViewConnectionMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewConnectionMenu);
}

static void connection_submenu_callback(void* context, uint32_t index) {
    App* app = context;
    switch(index) {
    case ConnectionMenuIndexConnectToCU:
        action_reconnect_saved(app);
        break;
    case ConnectionMenuIndexSearchForCU:
        action_search_for_cu(app);
        break;
    case ConnectionMenuIndexDisconnectBLE:
        action_disconnect_ble(app);
        break;
    default:
        break;
    }
}

/* "Disconnect GPIO & Exit" at the bottom of the main menu - the one action in
   this app that actually exits: BLEDISC if connected, frees the whole
   esp_at/UART session, then stops the view dispatcher, which unwinds
   fox_chameleon_main() straight through app_free() and back to the
   Flipper's app list. Distinct from Disconnect BLE above, which only
   ever resets BLE state and keeps the app (and the ESP32 claim) running. */
static void action_disconnect_and_quit(App* app) {
    /* app_log() already writes each line straight to the SD log file as
       it's called (see app_terminal_log_line()), so this is captured
       there even though nothing ever renders it on screen again - the
       app is about to close. */
    app_log(app, "Disconnecting and closing");
    app_log(app, "Fox Chameleon...");

    if(app->esp_at != NULL) {
        if(app->ble_connected) {
            esp_at_send(app->esp_at, "BLEDISC");
            app_expect_ok(app, 3000);
        }
        esp_at_free(app->esp_at);
        app->esp_at = NULL;
    }

    view_dispatcher_stop(app->view_dispatcher);
}

/* Sends a zero-argument Chameleon command built by `builder`, then waits
   for and displays its response through `formatter`. Every read-only
   diagnostic and scan command in the main menu goes through this. */
static void action_chameleon_command_ex(
    App* app,
    size_t (*builder)(uint8_t*, size_t),
    const char* label,
    void (*formatter)(const ChameleonFrame*, char*, size_t)) {
    if(!app->ble_connected) {
        app_log(app, "Not connected");
        app_render_log(app);
        return;
    }

    uint8_t frame[32];
    size_t frame_len = builder(frame, sizeof(frame));
    if(frame_len == 0) {
        app_log(app, "Failed to build frame");
        app_render_log(app);
        return;
    }

    app_log(app, "%s", label);
    app_render_log(app);

    ChameleonFrame parsed;
    uint8_t decode_buffer[CHAMELEON_RESPONSE_BUFFER_MAX];
    if(!app_write_command_and_await(
           app, frame, frame_len, decode_buffer, sizeof(decode_buffer), &parsed)) {
        app_render_log(app);
        return;
    }

    char text[80];
    formatter(&parsed, text, sizeof(text));
    app_log(app, "status 0x%04X", parsed.status);
    app_log(app, "%s", text);
    app_render_log(app);
}

static void
    action_chameleon_command(App* app, size_t (*builder)(uint8_t*, size_t), const char* label) {
    action_chameleon_command_ex(app, builder, label, chameleon_format_response);
}

static size_t build_change_mode_reader(uint8_t* out, size_t out_capacity) {
    return chameleon_build_change_device_mode(0x01, out, out_capacity);
}

static size_t build_change_mode_emulator(uint8_t* out, size_t out_capacity) {
    return chameleon_build_change_device_mode(0x00, out, out_capacity);
}

static size_t build_read_slot_block0(uint8_t* out, size_t out_capacity) {
    return chameleon_build_mf1_read_emu_block(0, 1, out, out_capacity);
}

static void format_response_as_uid_block(const ChameleonFrame* frame, char* out, size_t out_capacity) {
    if(frame->data_len < 7) {
        snprintf(out, out_capacity, "block too short (%u bytes)", frame->data_len);
        return;
    }
    chameleon_format_uid_block(frame->data, out, out_capacity);
}

/* Slot switching needs a parameter, so it doesn't fit the builder-
   function-pointer signature action_chameleon_command_ex() uses; this is
   otherwise the same write/await/format sequence. */
static void action_set_active_slot(App* app, uint8_t slot) {
    if(!app->ble_connected) {
        app_log(app, "Not connected");
        app_render_log(app);
        return;
    }

    uint8_t frame[16];
    size_t frame_len = chameleon_build_set_active_slot(slot, frame, sizeof(frame));
    if(frame_len == 0) {
        app_log(app, "Failed to build frame");
        app_render_log(app);
        return;
    }

    app_log(app, "Switching to slot %u", slot);
    app_render_log(app);

    ChameleonFrame parsed;
    uint8_t decode_buffer[CHAMELEON_RESPONSE_BUFFER_MAX];
    if(!app_write_command_and_await(
           app, frame, frame_len, decode_buffer, sizeof(decode_buffer), &parsed)) {
        app_render_log(app);
        return;
    }

    char text[80];
    chameleon_format_response(&parsed, text, sizeof(text));
    app_log(app, "status 0x%04X", parsed.status);
    app_log(app, "%s", text);
    app_render_log(app);
}

/* Switches to reader mode, scans for a card, then tries each key from
   the Flipper's own NFC dictionaries against sector 0 key A using
   MF1_READ_ONE_BLOCK - which authenticates and reads in the same round
   trip. A wrong key gets an empty response body rather than 16 bytes of
   data: that's the documented general rule for this protocol ("if the
   response status is different than [the success values], the response
   data is empty"), and it's what this function checks for success,
   rather than the response's status code. The exact numeric value the
   firmware uses for its auth-succeeded status was not confirmed from
   source for this project - see FEATURES.md - so this deliberately
   avoids depending on it. */
static void action_read_card_with_dictionary(App* app) {
    if(!app->ble_connected) {
        app_log(app, "Not connected");
        app_render_log(app);
        return;
    }

    KeyDictionary* dictionary = malloc(sizeof(KeyDictionary));
    size_t loaded = key_dictionary_load(dictionary);
    if(loaded == 0) {
        app_log(app, "No dictionary keys found.");
        app_log(app, "Expected on SD card at:");
        app_log(app, "/nfc/assets/mf_classic_dict.nfc");
        app_render_log(app);
        free(dictionary);
        return;
    }

    app_log(app, "Loaded %u keys", (unsigned)loaded);
    app_render_log(app);

    uint8_t mode_frame[16];
    size_t mode_len = chameleon_build_change_device_mode(0x01, mode_frame, sizeof(mode_frame));
    ChameleonFrame mode_response;
    uint8_t mode_decode_buffer[CHAMELEON_RESPONSE_BUFFER_MAX];
    if(mode_len == 0 ||
       !app_write_command_and_await(
           app, mode_frame, mode_len, mode_decode_buffer, sizeof(mode_decode_buffer), &mode_response)) {
        app_log(app, "Could not switch to reader mode");
        app_render_log(app);
        free(dictionary);
        return;
    }

    app_log(app, "Scanning for a card...");
    app_render_log(app);

    uint8_t scan_frame[16];
    size_t scan_len = chameleon_build_hf14a_scan(scan_frame, sizeof(scan_frame));
    ChameleonFrame scan_response;
    uint8_t scan_decode_buffer[CHAMELEON_RESPONSE_BUFFER_MAX];
    if(scan_len == 0 ||
       !app_write_command_and_await(
           app, scan_frame, scan_len, scan_decode_buffer, sizeof(scan_decode_buffer), &scan_response) ||
       scan_response.data_len < 1) {
        app_log(app, "No card detected");
        app_render_log(app);
        free(dictionary);
        return;
    }

    app_log(app, "Card found. Trying keys");
    app_log(app, "on sector 0 key A...");
    app_render_log(app);

    bool found = false;
    uint8_t found_key[KEY_DICTIONARY_KEY_LEN];
    uint8_t block_data[16];
    uint8_t key_decode_buffer[CHAMELEON_RESPONSE_BUFFER_MAX];

    for(size_t i = 0; i < dictionary->count; i++) {
        uint8_t frame[16];
        size_t frame_len = chameleon_build_mf1_read_one_block(
            0x60, 0, dictionary->keys[i], frame, sizeof(frame));
        if(frame_len == 0) continue;

        ChameleonFrame parsed;
        if(!app_write_command_and_await(
               app, frame, frame_len, key_decode_buffer, sizeof(key_decode_buffer), &parsed)) {
            app_log(app, "Lost the card mid-search");
            app_render_log(app);
            free(dictionary);
            return;
        }

        if(parsed.data_len >= 16) {
            found = true;
            memcpy(found_key, dictionary->keys[i], KEY_DICTIONARY_KEY_LEN);
            memcpy(block_data, parsed.data, 16);
            break;
        }

        if(i % 10 == 0) {
            app_log(app, "tried %u/%u", (unsigned)i, (unsigned)dictionary->count);
            app_render_log(app);
        }
    }

    free(dictionary);

    if(!found) {
        app_log(app, "No dictionary key opened");
        app_log(app, "sector 0 key A");
        app_render_log(app);
        return;
    }

    char uid_text[80];
    chameleon_format_uid_block(block_data, uid_text, sizeof(uid_text));

    app_log(
        app,
        "Key %02X%02X%02X%02X%02X%02X",
        found_key[0],
        found_key[1],
        found_key[2],
        found_key[3],
        found_key[4],
        found_key[5]);
    app_log(app, "%s", uid_text);
    app_render_log(app);
}

/* Reads all 64 blocks of the active slot's Mifare Classic 1K emulation
   memory (16 bytes each, one block per round trip - see the comment on
   chameleon_build_mf1_read_emu_block()) and writes them in order to
   FOX_CHAMELEON_DUMP_FILE, overwriting whatever was there before. */
static void action_dump_slot_to_sd(App* app) {
    if(!app->ble_connected) {
        app_log(app, "Not connected");
        app_render_log(app);
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    /* See ensure_dir_path() above action_check_esp32() - a single
       storage_common_mkdir() call only creates the deepest folder, and
       the fox_chameleon parent directory is never created anywhere else
       in this app. */
    ensure_dir_path(storage, FOX_CHAMELEON_DUMP_DIR);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, FOX_CHAMELEON_DUMP_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        app_log(app, "Could not open dump file");
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        app_render_log(app);
        return;
    }

    app_log(app, "Dumping slot (%u blocks)", CHAMELEON_DUMP_BLOCK_COUNT);
    app_render_log(app);

    bool ok = true;
    uint8_t dump_decode_buffer[CHAMELEON_RESPONSE_BUFFER_MAX];
    for(uint16_t block = 0; block < CHAMELEON_DUMP_BLOCK_COUNT; block++) {
        uint8_t frame[16];
        size_t frame_len = chameleon_build_mf1_read_emu_block((uint8_t)block, 1, frame, sizeof(frame));
        if(frame_len == 0) {
            app_log(app, "Failed to build read for block %u", block);
            ok = false;
            break;
        }

        ChameleonFrame parsed;
        if(!app_write_command_and_await(
               app, frame, frame_len, dump_decode_buffer, sizeof(dump_decode_buffer), &parsed)) {
            app_log(app, "Stopped at block %u", block);
            ok = false;
            break;
        }

        if(parsed.data_len < 16) {
            app_log(app, "Short reply at block %u", block);
            ok = false;
            break;
        }

        uint16_t written = storage_file_write(file, parsed.data, 16);
        if(written != 16) {
            app_log(app, "SD write failed at block %u", block);
            ok = false;
            break;
        }

        if(block % 8 == 0) {
            app_log(app, "block %u/%u", block, CHAMELEON_DUMP_BLOCK_COUNT);
            app_render_log(app);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    app_log(app, ok ? "Saved to:" : "Dump incomplete.");
    app_log(app, FOX_CHAMELEON_DUMP_FILE);
    app_render_log(app);
}

static void action_open_slot_menu(App* app) {
    app->current_view = FoxChameleonViewSlotMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewSlotMenu);
}

static void slot_submenu_callback(void* context, uint32_t index) {
    App* app = context;
    action_set_active_slot(app, (uint8_t)index);
}

static void submenu_callback(void* context, uint32_t index) {
    App* app = context;
    switch(index) {
    case MenuIndexConnect:
        /* Same slot, two meanings - keyed on ble_initialized, not
           ble_connected, so it doesn't flip back to "Connect" (and risk
           an unwanted auto-reconnect) just because the current Chameleon
           Ultra got disconnected while BLE itself is still up. See
           app.h's comment on MenuIndex and render_main_menu() below. */
        if(app->ble_initialized) {
            action_open_connection_menu(app);
        } else {
            action_connect(app);
        }
        break;
    case MenuIndexGetVersion:
        action_chameleon_command(
            app, chameleon_build_get_app_version, "Requesting firmware version");
        break;
    case MenuIndexGetGitVersion:
        action_chameleon_command(app, chameleon_build_get_git_version, "Requesting git version");
        break;
    case MenuIndexGetBattery:
        action_chameleon_command(app, chameleon_build_get_battery_info, "Requesting battery info");
        break;
    case MenuIndexGetSlot:
        action_chameleon_command(app, chameleon_build_get_active_slot, "Requesting active slot");
        break;
    case MenuIndexGetModel:
        action_chameleon_command(app, chameleon_build_get_device_model, "Requesting device model");
        break;
    case MenuIndexGetEnabledSlots:
        action_chameleon_command(
            app, chameleon_build_get_enabled_slots, "Requesting enabled slots");
        break;
    case MenuIndexGetChipId:
        action_chameleon_command(app, chameleon_build_get_device_chip_id, "Requesting chip ID");
        break;
    case MenuIndexGetAddress:
        action_chameleon_command(
            app, chameleon_build_get_device_address, "Requesting BLE address");
        break;
    case MenuIndexSelectSlot:
        action_open_slot_menu(app);
        break;
    case MenuIndexEnterReaderMode:
        action_chameleon_command(app, build_change_mode_reader, "Switching to reader mode");
        break;
    case MenuIndexEnterEmulatorMode:
        action_chameleon_command(app, build_change_mode_emulator, "Switching to emulator mode");
        break;
    case MenuIndexGetDeviceMode:
        action_chameleon_command(app, chameleon_build_get_device_mode, "Requesting device mode");
        break;
    case MenuIndexDetectMifareSupport:
        action_chameleon_command(
            app, chameleon_build_mf1_detect_support, "Checking Mifare Classic support");
        break;
    case MenuIndexScanCard:
        action_chameleon_command_ex(
            app, chameleon_build_hf14a_scan, "Scanning for a card", chameleon_format_hf14a_scan);
        break;
    case MenuIndexReadSlotBlock0:
        action_chameleon_command_ex(
            app, build_read_slot_block0, "Reading slot block 0", format_response_as_uid_block);
        break;
    case MenuIndexReadCardWithDictionary:
        action_read_card_with_dictionary(app);
        break;
    case MenuIndexDumpSlotToSd:
        action_dump_slot_to_sd(app);
        break;
    case MenuIndexDisconnectAndQuit:
        action_disconnect_and_quit(app);
        break;
    default:
        break;
    }
}

/* Builds/rebuilds the main menu's item list from current state. Called
   once at startup (not yet initialized, so item 0 reads "Connect", and
   ble_connected is false, so only item 0 and Disconnect GPIO & Exit show) and
   again every time ble_initialized or ble_connected changes
   (action_connect(), action_disconnect_ble(),
   action_disconnect_current_cu_if_any(), action_connect_with_mac()).

   Item 0's label is keyed on ble_initialized specifically - "Connect"
   before BLE is up, "Connection" (opening the Connection submenu) once
   it is, regardless of whether a specific Chameleon Ultra is currently
   linked - which is exactly what keeps this item from silently
   reverting to "Connect" (and risking an unwanted auto-reconnect) right
   after a disconnect. See MenuIndex's comment in app.h.

   Everything else (Get firmware version ... Dump slot to SD) is keyed
   on ble_connected: those all require an actual linked Chameleon Ultra
   to do anything useful, so they're hidden entirely while disconnected
   rather than left visible-but-guaranteed-to-say-"Not connected". Only
   item 0 and Disconnect GPIO & Exit are ever shown while disconnected.

   submenu_reset() doesn't support editing a single row in place, so the
   whole list is rebuilt from scratch each time this runs. */
static void render_main_menu(App* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Fox Chameleon");
    submenu_add_item(
        app->submenu,
        app->ble_initialized ? "Connection" : "Connect",
        MenuIndexConnect,
        submenu_callback,
        app);

    if(app->ble_connected) {
        submenu_add_item(
            app->submenu, "Get firmware version", MenuIndexGetVersion, submenu_callback, app);
        submenu_add_item(
            app->submenu, "Get git version", MenuIndexGetGitVersion, submenu_callback, app);
        submenu_add_item(
            app->submenu, "Get battery", MenuIndexGetBattery, submenu_callback, app);
        submenu_add_item(
            app->submenu, "Get active slot", MenuIndexGetSlot, submenu_callback, app);
        submenu_add_item(
            app->submenu, "Get device model", MenuIndexGetModel, submenu_callback, app);
        submenu_add_item(
            app->submenu, "Get enabled slots", MenuIndexGetEnabledSlots, submenu_callback, app);
        submenu_add_item(
            app->submenu, "Get chip ID", MenuIndexGetChipId, submenu_callback, app);
        submenu_add_item(
            app->submenu, "Get BLE address", MenuIndexGetAddress, submenu_callback, app);
        submenu_add_item(
            app->submenu, "Set active slot", MenuIndexSelectSlot, submenu_callback, app);
        submenu_add_item(
            app->submenu, "Enter reader mode", MenuIndexEnterReaderMode, submenu_callback, app);
        submenu_add_item(
            app->submenu,
            "Enter emulator mode",
            MenuIndexEnterEmulatorMode,
            submenu_callback,
            app);
        submenu_add_item(
            app->submenu, "Get device mode", MenuIndexGetDeviceMode, submenu_callback, app);
        submenu_add_item(
            app->submenu,
            "Detect Mifare support",
            MenuIndexDetectMifareSupport,
            submenu_callback,
            app);
        submenu_add_item(app->submenu, "Scan for card", MenuIndexScanCard, submenu_callback, app);
        submenu_add_item(
            app->submenu, "Read slot block 0", MenuIndexReadSlotBlock0, submenu_callback, app);
        submenu_add_item(
            app->submenu,
            "Read card (dictionary)",
            MenuIndexReadCardWithDictionary,
            submenu_callback,
            app);
        submenu_add_item(
            app->submenu, "Dump slot to SD", MenuIndexDumpSlotToSd, submenu_callback, app);
    }

    submenu_add_item(
        app->submenu,
        "Disconnect GPIO & Exit",
        MenuIndexDisconnectAndQuit,
        submenu_callback,
        app);
}

/* Claims the chosen UART only now, not at app launch - so picking the
   wrong pins and pressing Start is recoverable (Back returns to
   Settings) rather than something only fixable by relaunching. */
static void action_start(App* app) {
    /* New Terminal session here too, not just at Connect - this is the
       ESP32 Firmware bring-up phase (the AT/OK check), and its
       own dated log file is what lets a "couldn't claim the UART" or
       "no reply to AT" failure be diagnosed from the SD card even if
       nothing ever gets as far as a Chameleon Connect attempt. */
    app_terminal_start_session(app);

    /* Pressing Start a second time (Back from a failed attempt, then
       trying different Pins/Baud) must not leak the previous claim -
       esp_at_alloc() would otherwise contend with our own still-open
       handle and fail every time after the first. */
    if(app->esp_at != NULL) {
        esp_at_free(app->esp_at);
        app->esp_at = NULL;
    }
    /* A fresh esp_at claim means a fresh ESP32-side session as far as
       this app is concerned - BLEINIT (if it ever ran) doesn't survive
       whatever just happened to the old UART handle, so both flags reset
       here too, not just esp_at itself. In practice Start can only be
       pressed while ble_initialized is already false (getting back to
       Settings at all requires !esp32_detected, which is never true once
       BLE has been initialized), but resetting explicitly here keeps
       that guarantee from being an unstated assumption. */
    app->ble_initialized = false;
    app->ble_connected = false;

    app->esp_at =
        esp_at_alloc(pin_options[app->pin_option_index].serial_id, baud_options[app->baud_option_index]);

    if(app->esp_at == NULL) {
        app_log(app, "Could not claim that");
        app_log(app, "UART. Another app may");
        app_log(app, "be using it - close it");
        app_log(app, "and try again.");
        app_render_log(app);
        return;
    }

    esp_at_raw_capture_start(app->esp_at);
    action_check_esp32(app);
}

/* --- Settings: custom View (not a Submenu) for the Pins/Baud/Start
   screen. ---
   Same "static App* pointer set once, read from the draw callback"
   pattern used by candidate_draw_cb()/terminal_draw_cb() above, for the
   same reason (the draw callback only gets the throwaway model, not the
   context set via view_set_context()).

   Rebuilt as a custom view specifically so Left/Right can adjust the
   Pins/Baud value directly on whichever row is selected - the previous
   Submenu-based version needed an OK press to cycle each value, which
   (a) required memorizing that OK does something different here than
   everywhere else in the app, and (b) had the side effect of resetting
   the Submenu's selection back to the top of the list after every
   press. Neither happens here: Up/Down moves the row selection,
   Left/Right adjusts Pins/Baud in place, OK only does something on the
   Start row. */
static App* s_settings_view_app = NULL;

#define SETTINGS_ROW_COUNT 3
#define SETTINGS_ROW_TOP   15
#define SETTINGS_ROW_STEP  15
#define SETTINGS_ROW_H     13
#define SETTINGS_BOX_X     4
#define SETTINGS_BOX_W     120

static void settings_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_settings_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Fox Chameleon");

    char pins_text[24];
    snprintf(pins_text, sizeof(pins_text), "%s", pin_options[app->pin_option_index].label);
    char baud_text[16];
    snprintf(baud_text, sizeof(baud_text), "%lu", (unsigned long)baud_options[app->baud_option_index]);

    const char* row_text[SETTINGS_ROW_COUNT] = {pins_text, baud_text, "Start"};
    bool row_has_arrows[SETTINGS_ROW_COUNT] = {true, true, false};

    canvas_set_font(canvas, FontSecondary);
    for(size_t i = 0; i < SETTINGS_ROW_COUNT; i++) {
        int32_t y = SETTINGS_ROW_TOP + (int32_t)i * SETTINGS_ROW_STEP;
        bool selected = (i == app->settings_selected);

        if(selected) {
            canvas_draw_rbox(canvas, SETTINGS_BOX_X, y, SETTINGS_BOX_W, SETTINGS_ROW_H, 3);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, SETTINGS_BOX_X, y, SETTINGS_BOX_W, SETTINGS_ROW_H, 3);
        }

        int32_t text_y = y + SETTINGS_ROW_H / 2;
        canvas_draw_str_aligned(canvas, 64, text_y, AlignCenter, AlignCenter, row_text[i]);
        if(row_has_arrows[i]) {
            canvas_draw_str_aligned(
                canvas, SETTINGS_BOX_X + 6, text_y, AlignLeft, AlignCenter, "<");
            canvas_draw_str_aligned(
                canvas, SETTINGS_BOX_X + SETTINGS_BOX_W - 6, text_y, AlignRight, AlignCenter, ">");
        }

        canvas_set_color(canvas, ColorBlack);
    }
}

static bool settings_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        app->settings_selected =
            (app->settings_selected == 0) ? (SETTINGS_ROW_COUNT - 1) : (app->settings_selected - 1);
        with_view_model(app->settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        app->settings_selected = (app->settings_selected + 1) % SETTINGS_ROW_COUNT;
        with_view_model(app->settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyLeft:
        if(app->settings_selected == SettingsIndexPins) {
            app->pin_option_index =
                (app->pin_option_index == 0) ? (PIN_OPTION_COUNT - 1) : (app->pin_option_index - 1);
            with_view_model(app->settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        } else if(app->settings_selected == SettingsIndexBaud) {
            app->baud_option_index = (app->baud_option_index == 0) ? (BAUD_OPTION_COUNT - 1) :
                                                                      (app->baud_option_index - 1);
            with_view_model(app->settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        /* Consumed either way (including on the Start row, where it's a
           no-op) - Left is a value-adjust key on this screen now, not a
           navigation key, so it should never fall through. */
        return true;
    case InputKeyRight:
        if(app->settings_selected == SettingsIndexPins) {
            app->pin_option_index = (app->pin_option_index + 1) % PIN_OPTION_COUNT;
            with_view_model(app->settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        } else if(app->settings_selected == SettingsIndexBaud) {
            app->baud_option_index = (app->baud_option_index + 1) % BAUD_OPTION_COUNT;
            with_view_model(app->settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyOk:
        if(app->settings_selected == SettingsIndexStart) {
            action_start(app);
        }
        return true;
    case InputKeyBack:
        return false; /* navigation_callback exits the app from here */
    default:
        return false;
    }
}

static bool navigation_callback(void* context) {
    App* app = context;

    if(app->current_view == FoxChameleonViewSlotMenu ||
       app->current_view == FoxChameleonViewCandidateMenu ||
       app->current_view == FoxChameleonViewConnectionMenu) {
        app->current_view = FoxChameleonViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewMenu);
        return true;
    }

    /* Message (the no-ESP32-response Retry/Skip screen) and Terminal (
       everything else - the actual log/status pages) share one Back rule:
       once esp32_detected, Back always returns to the main menu - "so
       that when these info pages come up the user knows that pressing
       back will get them back to where they were". Before esp32_detected
       (only possible on Message pre-Retry/Skip, or on Terminal while the
       very first "Checking for ESP32..." AT check is still in flight),
       Back instead goes to Settings, since there's no menu to return to
       yet and adjusting Pins/Baud + Start again is the way forward. */
    if(app->current_view == FoxChameleonViewMessage ||
       app->current_view == FoxChameleonViewTerminal) {
        if(app->esp32_detected) {
            app->current_view = FoxChameleonViewMenu;
            view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewMenu);
        } else {
            app->current_view = FoxChameleonViewSettings;
            view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewSettings);
        }
        return true;
    }

    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

/* Tries AT/OK on the given pin pair at the Fox ESP32 Firmware's stock
   115200 baud - used once at launch, before the Settings screen is ever
   shown, so a default wiring/baud doesn't force the user through a
   screen they don't need (see app_alloc() below). A shorter timeout
   than action_check_esp32()'s normal 2500ms - a real ESP32 that's
   actually wired up on this pin pair responds close to instantly, so
   this only costs real time on the pin pair that ISN'T connected.
   Leaves esp_at claimed (and pin_option_index/baud_option_index
   updated to match) on success; frees it and leaves both untouched on
   failure, so a second attempt with the other pin pair starts clean. */
static bool app_probe_default_uart(App* app, size_t pin_index) {
    app->esp_at =
        esp_at_alloc(pin_options[pin_index].serial_id, baud_options[BAUD_OPTION_DEFAULT_INDEX]);
    if(app->esp_at == NULL) return false;

    esp_at_send(app->esp_at, "AT");
    bool ok = app_expect_ok(app, 1500);

    if(!ok) {
        esp_at_free(app->esp_at);
        app->esp_at = NULL;
        return false;
    }

    app->pin_option_index = pin_index;
    app->baud_option_index = BAUD_OPTION_DEFAULT_INDEX;
    return true;
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->chameleon_mac = furi_string_alloc();
    app->gatt_service_uuid = furi_string_alloc();
    app->gatt_write_char_uuid = furi_string_alloc();
    app->gatt_notify_char_uuid = furi_string_alloc();
    app->log = furi_string_alloc();
    app->terminal_log_path = furi_string_alloc();
    app->baud_option_index = BAUD_OPTION_DEFAULT_INDEX;

    app_load_config(app);
    app_ensure_config_defaults(app);
    /* Ensures config.txt exists and is fully populated from the very
       first launch, defaults included - this is what makes it
       something the app maintains rather than something the user is
       expected to create by hand before first use. mac= may still be
       empty at this point; it gets filled in and re-saved the first
       time Connect actually finds or is given one. */
    app_save_config(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);

    /* Settings - see settings_draw_cb()/settings_input_cb() above for
       why this is a custom View, not a Submenu. */
    app->settings_view = view_alloc();
    view_set_draw_callback(app->settings_view, settings_draw_cb);
    view_set_input_callback(app->settings_view, settings_input_cb);
    view_set_context(app->settings_view, app);
    view_allocate_model(app->settings_view, ViewModelTypeLocking, sizeof(uint8_t));
    s_settings_view_app = app;

    app->submenu = submenu_alloc();
    render_main_menu(app);

    app->connection_submenu = submenu_alloc();
    submenu_set_header(app->connection_submenu, "Connection");
    submenu_add_item(
        app->connection_submenu,
        "Connect to C.U",
        ConnectionMenuIndexConnectToCU,
        connection_submenu_callback,
        app);
    submenu_add_item(
        app->connection_submenu,
        "Search for C.U",
        ConnectionMenuIndexSearchForCU,
        connection_submenu_callback,
        app);
    submenu_add_item(
        app->connection_submenu,
        "Disconnect BLE",
        ConnectionMenuIndexDisconnectBLE,
        connection_submenu_callback,
        app);

    app->slot_submenu = submenu_alloc();
    submenu_set_header(app->slot_submenu, "Select slot");
    char slot_label[8];
    for(uint32_t i = 0; i < 8; i++) {
        snprintf(slot_label, sizeof(slot_label), "Slot %lu", (unsigned long)i);
        submenu_add_item(app->slot_submenu, slot_label, i, slot_submenu_callback, app);
    }

    /* Populated on demand by action_scan_for_chameleons() - empty at
       allocation time, since which (if any) Chameleon Ultras are
       nearby isn't known until a scan actually runs. A custom View, not
       a Submenu - see candidate_draw_cb()'s comment for why (two-line
       rows, matching Fox File Browser's Favourites list). The model
       allocated here is a throwaway uint8_t, touched only via
       with_view_model() to ask for a redraw - the real state
       (candidates/candidate_selected/candidate_scroll) lives on App,
       reached from the draw callback through s_candidate_view_app since
       draw callbacks don't receive the context set below the way input
       callbacks do. */
    app->candidate_view = view_alloc();
    view_set_draw_callback(app->candidate_view, candidate_draw_cb);
    view_set_input_callback(app->candidate_view, candidate_input_cb);
    view_set_context(app->candidate_view, app);
    view_allocate_model(app->candidate_view, ViewModelTypeLocking, sizeof(uint8_t));
    s_candidate_view_app = app;

    /* Terminal - same custom-View pattern as candidate_view above, for
       the same reason (needs its own header/scroll drawing a Submenu or
       Widget can't do). See terminal_draw_cb()/terminal_input_cb(). */
    app->terminal_view = view_alloc();
    view_set_draw_callback(app->terminal_view, terminal_draw_cb);
    view_set_input_callback(app->terminal_view, terminal_input_cb);
    view_set_context(app->terminal_view, app);
    view_allocate_model(app->terminal_view, ViewModelTypeLocking, sizeof(uint8_t));
    s_terminal_view_app = app;

    app->widget = widget_alloc();

    view_dispatcher_add_view(app->view_dispatcher, FoxChameleonViewSettings, app->settings_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxChameleonViewMenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, FoxChameleonViewMessage, widget_get_view(app->widget));
    view_dispatcher_add_view(
        app->view_dispatcher,
        FoxChameleonViewSlotMenu,
        submenu_get_view(app->slot_submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, FoxChameleonViewCandidateMenu, app->candidate_view);
    view_dispatcher_add_view(
        app->view_dispatcher,
        FoxChameleonViewConnectionMenu,
        submenu_get_view(app->connection_submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, FoxChameleonViewTerminal, app->terminal_view);

    /* Takes a ViewDispatcherType, not a GuiLayer - an easy mix-up since
       older examples online still show GuiLayerFullscreen here. */
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Auto-probe the two supported pin pairs at the Fox ESP32 Firmware's
       stock baud (115200) before ever showing the Settings screen - this
       is what lets a default wiring/baud skip that screen altogether
       per the user's request; Settings is only needed as a fallback
       when neither responds (non-default wiring or baud). This runs
       synchronously here, before view_dispatcher_run() starts the GUI's
       render loop in fox_chameleon_main() - the screen simply doesn't
       show anything until this resolves, up to ~3s in the worst case
       (both attempts timing out). */
    if(app_probe_default_uart(app, 0) || app_probe_default_uart(app, 1)) {
        esp_at_raw_capture_start(app->esp_at);
        app_terminal_start_session(app);
        app_log(app, "ESP32 auto-detected on");
        app_log(
            app,
            "%s @ %lu",
            pin_options[app->pin_option_index].label,
            (unsigned long)baud_options[app->baud_option_index]);
        app->esp32_detected = true;
        app->current_view = FoxChameleonViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewMenu);
    } else {
        /* Lands on Settings - the UART isn't claimed at this point,
           since neither default probe succeeded (see action_start() for
           where it's claimed once the user picks Pins/Baud manually). */
        app->current_view = FoxChameleonViewSettings;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxChameleonViewSettings);
    }

    return app;
}

static void app_free(App* app) {
    if(app->esp_at != NULL) {
        if(app->ble_connected) {
            esp_at_send(app->esp_at, "BLEDISC");
        }
        esp_at_free(app->esp_at);
    }

    view_dispatcher_remove_view(app->view_dispatcher, FoxChameleonViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, FoxChameleonViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FoxChameleonViewMessage);
    view_dispatcher_remove_view(app->view_dispatcher, FoxChameleonViewSlotMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FoxChameleonViewCandidateMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FoxChameleonViewConnectionMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FoxChameleonViewTerminal);

    view_free(app->settings_view);
    s_settings_view_app = NULL;
    submenu_free(app->submenu);
    submenu_free(app->slot_submenu);
    submenu_free(app->connection_submenu);
    view_free(app->candidate_view);
    s_candidate_view_app = NULL;
    view_free(app->terminal_view);
    s_terminal_view_app = NULL;
    widget_free(app->widget);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    furi_string_free(app->chameleon_mac);
    furi_string_free(app->gatt_service_uuid);
    furi_string_free(app->gatt_write_char_uuid);
    furi_string_free(app->gatt_notify_char_uuid);
    furi_string_free(app->log);
    furi_string_free(app->terminal_log_path);
    free(app);
}

int32_t fox_chameleon_main(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
