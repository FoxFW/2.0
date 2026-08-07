#include "csight.h"
#include "csight_log.h"
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <string.h>

// This is CSIght's original binary-protocol UART layer rewritten to speak
// Fox's shared [CSI/...] text-line protocol over esp_at.c — the same
// transport TagTinker_FoxEdition uses (wifi/esp_at.c, copied verbatim).
// csight_app.c and csight_draw.c are untouched: this file still exposes the
// exact same csight_uart_*/csight_send_* API declared in csight.h, it just
// fills app-> fields from JSON lines instead of checksummed binary packets.

#define TAG              "CSIght_UART"
#define THREAD_STOP_FLAG  0x1

// ─── Tiny hand-rolled JSON helpers — each Fox module writes its own small
// version of these rather than sharing a full JSON library; matches the
// convention already used on the ESP32 side (http_bridge.cpp, fox_csi.cpp). ─
static bool json_extract_long(const char* json, const char* key, long* out) {
    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char* pos = strstr(json, needle);
    if(!pos) return false;
    pos += strlen(needle);
    char* end = NULL;
    long val = strtol(pos, &end, 10);
    if(end == pos) return false;
    *out = val;
    return true;
}

static bool json_extract_string(const char* json, const char* key, char* out, size_t out_size) {
    char needle[32];
    snprintf(needle, sizeof(needle), "\"%s\":\"", key);
    const char* pos = strstr(json, needle);
    if(!pos) return false;
    pos += strlen(needle);
    const char* end = strchr(pos, '"');
    if(!end) return false;
    size_t len = (size_t)(end - pos);
    if(len >= out_size) len = out_size - 1;
    memcpy(out, pos, len);
    out[len] = '\0';
    return true;
}

static uint8_t hex_nibble(char c) {
    if(c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if(c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if(c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0;
}

static size_t hex_decode(const char* hex, uint8_t* out, size_t max_bytes) {
    size_t n = 0;
    size_t len = strlen(hex);
    while(n < max_bytes && (n * 2 + 1) < len) {
        out[n] = (uint8_t)((hex_nibble(hex[n * 2]) << 4) | hex_nibble(hex[n * 2 + 1]));
        n++;
    }
    return n;
}

// ─── Event line handlers ──────────────────────────────────────────────────────
static void handle_hello(CSIghtApp* app, const char* body) {
    char role[16] = {0};
    json_extract_string(body, "chip", app->chip_name, sizeof(app->chip_name));

    long csi_support = 0, fw_major = 0, fw_minor = 0, webui = 0;
    json_extract_long(body, "csi_support", &csi_support);
    json_extract_long(body, "fw_major", &fw_major);
    json_extract_long(body, "fw_minor", &fw_minor);
    json_extract_long(body, "webui", &webui);
    json_extract_string(body, "role", role, sizeof(role));

    app->csi_support  = (uint8_t)csi_support;
    app->fw_major     = (uint8_t)fw_major;
    app->fw_minor     = (uint8_t)fw_minor;
    app->web_ui_active = (webui == 1);

    FURI_LOG_I(TAG, "Handshake: %s CSI=%d FW=%d.%d role=%s",
               app->chip_name, app->csi_support, app->fw_major, app->fw_minor, role);

    // Fox's [CSI/...] protocol requires an explicit START — the original
    // CSIght firmware ran CSI capture continuously from boot, but that's
    // wasteful on multi-purpose Fox_ESP32_FW boards, so we kick it off here
    // instead, right after a successful handshake, to preserve the same
    // "just works" feel from the app's point of view.
    csight_send_start(app);

    app->state = app->config_exists ? AppStateMainMenu : AppStateCompatCheck;
}

static void handle_motion(CSIghtApp* app, const char* body) {
    long intensity = 0, proximity = 0;
    if(!json_extract_long(body, "intensity", &intensity)) return;
    if(!json_extract_long(body, "proximity", &proximity)) return;

    app->motion_intensity = (uint8_t)intensity;
    app->proximity        = (uint8_t)proximity;
    csight_add_blip(app, (uint8_t)intensity, (uint8_t)proximity);
    app->target_acquired = true;
    app->target_ts       = furi_get_tick();
    app->motion_count++;

    if(app->alert_armed && intensity >= app->alert_threshold && !app->alert_triggered) {
        app->alert_triggered = true;
        app->alert_ts        = furi_get_tick();
        notification_message(app->notifications, &sequence_blink_red_100);
        char detail[24];
        snprintf(detail, sizeof(detail), "intensity=%ld", intensity);
        csight_log_event(app, "ALERT", detail);
    } else {
        notification_message(app->notifications, &sequence_single_vibro);
    }
}

static void handle_waterfall(CSIghtApp* app, const char* body) {
    char hex[160] = {0};
    if(!json_extract_string(body, "bins", hex, sizeof(hex))) return;

    uint8_t bins[64];
    size_t count = hex_decode(hex, bins, sizeof(bins));
    if(count == 0) return;

    uint8_t col[WATERFALL_HEIGHT / 4];
    memset(col, 0, sizeof(col));
    int bins_per_pixel = (int)count / (WATERFALL_HEIGHT / 4);
    if(bins_per_pixel < 1) bins_per_pixel = 1;

    for(int p = 0; p < WATERFALL_HEIGHT / 4; p++) {
        uint32_t sum = 0;
        for(int b = 0; b < bins_per_pixel; b++) {
            int idx = p * bins_per_pixel + b;
            if(idx < (int)count) sum += bins[idx];
        }
        col[p] = (uint8_t)(sum / (uint32_t)bins_per_pixel);
    }

    memcpy(app->waterfall.cols[app->wf_write_col], col, sizeof(col));
    app->wf_write_col = (app->wf_write_col + 1) % WATERFALL_COLS;
}

static void handle_vitals(CSIghtApp* app, const char* body) {
    long breathing = 0, heart = 0;
    if(!json_extract_long(body, "breath", &breathing)) return;
    if(!json_extract_long(body, "heart", &heart)) return;

    app->breathing_bpm = (uint8_t)breathing;
    app->heart_bpm     = (uint8_t)heart;
    app->vitals_valid  = true;

    if(breathing > 0) {
        char detail[48];
        snprintf(detail, sizeof(detail), "breath=%ld heart=%ld", breathing, heart);
        csight_log_event(app, "VITALS", detail);
    }
}

static void handle_channel_set(CSIghtApp* app, const char* body) {
    long ch = 0;
    if(!json_extract_long(body, "ch", &ch)) return;
    if(ch >= 1 && ch <= 13) {
        app->wifi_channel = (uint8_t)ch;
        FURI_LOG_I(TAG, "ESP32 auto-selected CH %ld", ch);
    }
}

static void handle_node_found(CSIghtApp* app, const char* body) {
    long id = 0;
    if(!json_extract_long(body, "id", &id)) return;

    app->node_found_pending = true;
    app->node_found_id      = (uint8_t)id;
    app->node_found_ts      = furi_get_tick();
    notification_message(app->notifications, &sequence_success);
    char detail[24];
    snprintf(detail, sizeof(detail), "node_id=%ld", id);
    csight_log_event(app, "NODE_FOUND", detail);
}

static void handle_mesh(CSIghtApp* app, const char* body) {
    long has_est = 0, est_x = 0, est_y = 0;
    json_extract_long(body, "has_est", &has_est);
    json_extract_long(body, "est_x", &est_x);
    json_extract_long(body, "est_y", &est_y);

    for(int n = 0; n < MESH_MAX_NODES; n++) app->mesh_node_active[n] = false;

    const char* nodes = strstr(body, "\"nodes\":[");
    if(nodes) {
        const char* cursor = nodes;
        while((cursor = strstr(cursor, "{\"id\":")) != NULL) {
            const char* node_end = strchr(cursor, '}');
            if(!node_end) break;

            char node_json[64] = {0};
            size_t node_len = (size_t)(node_end - cursor) + 1;
            if(node_len >= sizeof(node_json)) node_len = sizeof(node_json) - 1;
            memcpy(node_json, cursor, node_len);
            node_json[node_len] = '\0';

            long id = -1, intensity = 0, proximity = 0;
            json_extract_long(node_json, "id", &id);
            json_extract_long(node_json, "intensity", &intensity);
            json_extract_long(node_json, "proximity", &proximity);

            if(id >= 0 && id < MESH_MAX_NODES) {
                app->mesh_node_active[id]              = true;
                app->mesh_node_intensity[id]           = (uint8_t)intensity;
                app->mesh_node_proximity[id]           = (uint8_t)proximity;
                app->mesh_node_last_seen_tick[id]       = furi_get_tick();
            }
            cursor = node_end + 1;
        }
    }

    app->mesh_has_estimate = (has_est == 1);
    app->mesh_est_x_cm     = (int16_t)est_x;
    app->mesh_est_y_cm     = (int16_t)est_y;
    if(app->mesh_has_estimate) {
        csight_heatmap_add(app, (int16_t)est_x, (int16_t)est_y);
    }
}

// ─── Line dispatch ─────────────────────────────────────────────────────────
static void dispatch_line(CSIghtApp* app, const char* line) {
    // Plain-text reply to the generic "info" probe — every Fox_ESP32_FW
    // build answers this the same way regardless of which app is talking to
    // it, so this is the same detect-gate check fox_file_downloader and the
    // other Fox apps use. Not bracket-tagged, so it has to be checked before
    // the [CSI/...] parsing below bails out on it.
    if(strcmp(line, "Fox ESP32 Firmware") == 0) {
        app->esp32_probe_ok = true;
        return;
    }

    int close = -1;
    for(int i = 0; line[i] != '\0'; i++) {
        if(line[i] == ']') { close = i; break; }
        if(i > 60) break; // tags are always short — bail rather than scan garbage
    }
    if(line[0] != '[' || close < 0) return;

    char tag[48] = {0};
    size_t tag_len = (size_t)(close - 1);
    if(tag_len >= sizeof(tag)) tag_len = sizeof(tag) - 1;
    memcpy(tag, &line[1], tag_len);
    tag[tag_len] = '\0';

    const char* body = &line[close + 1];

    if(strcmp(tag, "CSI/HELLO/SUCCESS") == 0) {
        handle_hello(app, body);
    } else if(strcmp(tag, "CSI/EVENT/MOTION") == 0) {
        handle_motion(app, body);
    } else if(strcmp(tag, "CSI/EVENT/WATERFALL") == 0) {
        handle_waterfall(app, body);
    } else if(strcmp(tag, "CSI/EVENT/VITALS") == 0) {
        handle_vitals(app, body);
    } else if(strcmp(tag, "CSI/CHANNEL/SET") == 0) {
        handle_channel_set(app, body);
    } else if(strcmp(tag, "CSI/EVENT/NODE_FOUND") == 0) {
        handle_node_found(app, body);
    } else if(strcmp(tag, "CSI/EVENT/MESH") == 0) {
        handle_mesh(app, body);
    }
    // Unrecognised tags (plain command ACKs like [CSI/MODE/SUCCESS], or
    // anything from another Fox module) are ignored — matches the original
    // "unknown byte, skip and keep parsing" behaviour.
}

// ─── RX thread ────────────────────────────────────────────────────────────────
static int32_t uart_rx_thread(void* ctx) {
    CSIghtApp* app = (CSIghtApp*)ctx;
    EspAtMsg   msg;

    while(true) {
        uint32_t flags = furi_thread_flags_get();
        if(flags & THREAD_STOP_FLAG) {
            furi_thread_flags_clear(THREAD_STOP_FLAG);
            break;
        }

        if(!esp_at_receive(app->esp_at, &msg, 50)) continue;
        dispatch_line(app, msg.line);
    }
    return 0;
}

// ─── Init / Deinit ────────────────────────────────────────────────────────────
void csight_uart_init(CSIghtApp* app) {
    app->esp_at = esp_at_alloc(CSIGHT_UART_CH, CSIGHT_UART_BAUD);

    // uart_rx_thread keeps an EspAtMsg (char[ESP_AT_LINE_MAX], 6200 bytes) as
    // a local variable for the life of the thread, so the stack has to be
    // sized well past that — 1024 bytes here caused an immediate MPU
    // fault/stack-overflow crash on launch. TagTinker's own worker thread
    // (wifi/tagtinker_wifi.c) under-sizes this the same way with 3072, so
    // don't copy that number either; go comfortably above the 6200-byte
    // struct instead.
    app->uart_thread = furi_thread_alloc_ex("csight_rx", 8192, uart_rx_thread, app);
    furi_thread_start(app->uart_thread);
}

void csight_uart_deinit(CSIghtApp* app) {
    csight_send_stop(app);

    furi_thread_flags_set(furi_thread_get_id(app->uart_thread), THREAD_STOP_FLAG);
    furi_thread_join(app->uart_thread);
    furi_thread_free(app->uart_thread);
    app->uart_thread = NULL;

    esp_at_free(app->esp_at);
    app->esp_at = NULL;
}

// ─── Send helpers ─────────────────────────────────────────────────────────────
void csight_uart_send(CSIghtApp* app, const uint8_t* data, size_t len) {
    char line[ESP_AT_LINE_MAX];
    size_t n = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
    memcpy(line, data, n);
    line[n] = '\0';
    esp_at_send(app->esp_at, line);
}

void csight_send_probe(CSIghtApp* app) {
    // Generic "info" command every Fox_ESP32_FW build answers with a plain
    // "Fox ESP32 Firmware" line — used only to confirm a Fox ESP32 is on
    // the wire at all, before ever touching the CSI-specific protocol.
    esp_at_send(app->esp_at, "info");
}

void csight_send_handshake(CSIghtApp* app) {
    esp_at_send(app->esp_at, "[CSI/HELLO]");
}

void csight_send_start(CSIghtApp* app) {
    uint8_t mode = (app->display_mode == DisplayModeWaterfall) ? 1 :
                   (app->display_mode == DisplayModeProximity) ? 2 :
                   (app->display_mode == DisplayModeVitals)    ? 3 : 0;
    char line[40];
    snprintf(line, sizeof(line), "[CSI/START]{\"mode\":%d}", mode);
    esp_at_send(app->esp_at, line);
}

void csight_send_stop(CSIghtApp* app) {
    esp_at_send(app->esp_at, "[CSI/STOP]");
}

void csight_send_sensitivity(CSIghtApp* app) {
    char line[40];
    snprintf(line, sizeof(line), "[CSI/SENSITIVITY]{\"level\":%d}", app->sensitivity);
    esp_at_send(app->esp_at, line);
}

void csight_send_webui_toggle(CSIghtApp* app) {
    esp_at_send(app->esp_at, app->web_ui_active ? "[CSI/WEBUI/OFF]" : "[CSI/WEBUI/ON]");
    app->web_ui_active = !app->web_ui_active;
}

void csight_send_mode(CSIghtApp* app) {
    uint8_t mode = (app->display_mode == DisplayModeWaterfall) ? 1 :
                   (app->display_mode == DisplayModeProximity) ? 2 :
                   (app->display_mode == DisplayModeVitals)    ? 3 : 0;
    char line[40];
    snprintf(line, sizeof(line), "[CSI/MODE]{\"mode\":%d}", mode);
    esp_at_send(app->esp_at, line);
}

void csight_send_calibrate(CSIghtApp* app) {
    esp_at_send(app->esp_at, "[CSI/CALIBRATE]");
}

void csight_send_channel(CSIghtApp* app) {
    char line[40];
    snprintf(line, sizeof(line), "[CSI/CHANNEL]{\"ch\":%d}", app->wifi_channel);
    esp_at_send(app->esp_at, line);
}

void csight_send_channel_auto(CSIghtApp* app) {
    esp_at_send(app->esp_at, "[CSI/CHANNEL/AUTO]");
    // wifi_channel is updated when the ESP32 replies with [CSI/CHANNEL/SET]
}

void csight_send_forget_nodes(CSIghtApp* app) {
    esp_at_send(app->esp_at, "[CSI/MESH/FORGET]");
    for(int n = 1; n < MESH_MAX_NODES; n++) app->mesh_node_active[n] = false;
}

void csight_send_node_positions(CSIghtApp* app) {
    char line[128];
    snprintf(
        line,
        sizeof(line),
        "[CSI/MESH/POSITIONS]{\"x1\":%d,\"y1\":%d,\"x2\":%d,\"y2\":%d,\"x3\":%d,\"y3\":%d}",
        app->mesh_node_x_cm[1],
        app->mesh_node_y_cm[1],
        app->mesh_node_x_cm[2],
        app->mesh_node_y_cm[2],
        app->mesh_node_x_cm[3],
        app->mesh_node_y_cm[3]);
    esp_at_send(app->esp_at, line);
}

void csight_send_pathloss_gamma(CSIghtApp* app) {
    char line[40];
    snprintf(line, sizeof(line), "[CSI/MESH/GAMMA]{\"gamma\":%d}", app->pathloss_gamma_x10);
    esp_at_send(app->esp_at, line);
}
