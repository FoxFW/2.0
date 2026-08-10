/*
 * Flipper-side TagTinker WiFi link - Fox ESP32 Firmware edition.
 *
 * TagTinker originally shipped its own ESP-IDF firmware (esp32-wifi-fw)
 * speaking a bespoke 0xAA55 framed binary protocol. This file replaces
 * that with the AT bracket-command protocol every other Fox app already
 * speaks to Fox ESP32 Firmware (see esp_at.c), so a WiFi Dev Board that's
 * already flashed for Fox ESP32 Commander/Chat/Portal/etc. works here too
 * with no second flash.
 *
 * Mapping from the old wire protocol to Fox ESP32 Firmware commands:
 *
 *   WIFI_SET      -> [WIFI/SAVE] then [WIFI/CONNECT]
 *   WIFI_FORGET   -> [WIFI/FORGET]
 *   WIFI_STATUS   -> [WIFI/STATUS] + [WIFI/SSID] + [IP/ADDRESS]
 *   LIST_PLUGINS  -> [DOWNLOAD/START]+[DOWNLOAD/STREAM] on <cloud>/plugins,
 *                    parsed as JSON on the FAP side (the ESP no longer
 *                    parses or re-frames anything).
 *   RUN_PLUGIN    -> [DOWNLOAD/START]+[DOWNLOAD/STREAM] on
 *                    <cloud>/render/<id>?w=&h=&accent=&<params>. The
 *                    worker's 8-byte framebuffer header (width, height,
 *                    planes, reserved, row_stride) rides straight through
 *                    the raw byte stream and is parsed here instead of on
 *                    the ESP.
 *   HELLO         -> auto-sent [VERSION] probe right after open().
 *
 * Threading model:
 *
 *   - Public API calls (ping/query_status/set_creds/forget/list_plugins/
 *     run_plugin) just enqueue a small command struct and return
 *     immediately, matching the old fire-and-forget framed-emit calls.
 *   - A single worker thread pulls commands off that queue and executes
 *     them one at a time as blocking send/receive sequences against
 *     esp_at.c (the same transport fox_update_downloader uses). All
 *     TtWifiEvent callback invocations happen on this worker thread, same
 *     as before.
 *   - esp_at.c owns the actual UART + expansion-port lifecycle, so this
 *     file no longer touches FuriHalSerial or Expansion directly.
 */
#include "tagtinker_wifi.h"
#include "esp_at.h"
#include "gpio_remap_compat.h"

#include <furi.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TAG "TtWifi"
#define ESP_BAUD 115200U

#define TT_CLOUD_BASE_URL "https://tagtinker.jhackerr.workers.dev"

#define WIFI_LINE_WAIT_MS    4000
#define WIFI_CONNECT_WAIT_MS 25000
#define DL_START_WAIT_MS     15000
#define DL_STREAM_WAIT_MS    10000
#define DL_CHUNK_TIMEOUT_MS  10000
#define RENDER_STREAM_BUF    512
#define PLUGIN_BODY_MAX      12288

/* ---- Command queue -------------------------------------------------------*/

typedef enum {
    WcPing,
    WcQueryStatus,
    WcSetCreds,
    WcForget,
    WcListPlugins,
    WcRunPlugin,
} WifiCmdType;

typedef struct {
    WifiCmdType type;
    char     ssid[33];
    char     pwd[65];
    uint8_t  plugin_index;
    uint16_t target_w;
    uint16_t target_h;
    uint8_t  accent;
    uint8_t  n_params;
    char     keys[TT_WIFI_MAX_PARAMS][24];
    char     vals[TT_WIFI_MAX_PARAMS][96];
} WifiCmd;

struct TagTinkerWifi {
    EspAt*            esp_at;
    FuriThread*       worker;
    FuriMessageQueue* cmd_queue;
    volatile bool     running;

    TtWifiEventCb cb;
    void*         user;

    char    last_ssid[33];
    char    cached_ids[TT_WIFI_MAX_FAP_PLUGINS][32];
    uint8_t cached_id_count;

    /* Reusable scratch buffer for TtWifiEvtPlugin dispatch. */
    TagTinkerWifiPlugin pending_plugin;
};

static void emit(TagTinkerWifi* w, TtWifiEvent* ev) {
    if(w->cb) w->cb(ev, w->user);
}

/* ---- AT line helpers ------------------------------------------------------*/

typedef enum { WlOk, WlError, WlTimeout } WaitResult;

static WaitResult wait_for_line(
    EspAt* esp_at,
    const char* tag,
    char* out_rest,
    size_t out_rest_size,
    uint32_t timeout_ms) {
    EspAtMsg msg;
    size_t tag_len = strlen(tag);
    uint32_t start = furi_get_tick();
    while((furi_get_tick() - start) < timeout_ms) {
        if(!esp_at_receive(esp_at, &msg, 300)) continue;
        if(strncmp(msg.line, tag, tag_len) == 0) {
            if(out_rest && out_rest_size) {
                strncpy(out_rest, msg.line + tag_len, out_rest_size - 1);
                out_rest[out_rest_size - 1] = 0;
            }
            return WlOk;
        }
        if(strncmp(msg.line, "[ERROR]", 7) == 0) {
            if(out_rest && out_rest_size) {
                strncpy(out_rest, msg.line, out_rest_size - 1);
                out_rest[out_rest_size - 1] = 0;
            }
            return WlError;
        }
    }
    return WlTimeout;
}

static bool read_plain_line(EspAt* esp_at, char* out, size_t out_size, uint32_t timeout_ms) {
    EspAtMsg msg;
    if(!esp_at_receive(esp_at, &msg, timeout_ms)) return false;
    strncpy(out, msg.line, out_size - 1);
    out[out_size - 1] = 0;
    return true;
}

/* Blank lines never reach esp_at_receive - esp_at.c's line collector drops
 * zero-length lines before they hit the message queue - so a single read
 * is enough to catch the tag that follows a raw byte stream. */
static bool dl_stream_end_ok(EspAt* esp_at) {
    EspAtMsg msg;
    if(!esp_at_receive(esp_at, &msg, 3000)) return false;
    return strcmp(msg.line, "[DOWNLOAD/STREAM/END]") == 0;
}

/* ---- Tiny JSON helpers -----------------------------------------------------
 * Deliberately minimal (no allocation beyond the caller's buffers, no
 * recursion) - mirrors the same style Fox ESP32 Firmware's own
 * http_bridge.cpp uses for its AT-command JSON, just reimplemented here
 * since the FAP has no cJSON and doesn't need one for this. */

static bool j_find_key(const char* json, const char* key, size_t* out_pos) {
    char pat[40];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(json, pat);
    if(!p) return false;
    *out_pos = (size_t)(p - json);
    return true;
}

static bool j_get_string(const char* json, const char* key, char* out, size_t cap) {
    size_t pos;
    if(!j_find_key(json, key, &pos)) return false;
    const char* p = strchr(json + pos, ':');
    if(!p) return false;
    p++;
    while(*p == ' ' || *p == '\t') p++;
    if(*p != '"') return false;
    p++;
    size_t i = 0;
    while(*p && *p != '"' && i + 1 < cap) {
        if(*p == '\\' && p[1]) p++;
        out[i++] = *p++;
    }
    out[i] = 0;
    return true;
}

static bool j_get_int(const char* json, const char* key, int32_t* out) {
    size_t pos;
    if(!j_find_key(json, key, &pos)) return false;
    const char* p = strchr(json + pos, ':');
    if(!p) return false;
    p++;
    while(*p == ' ' || *p == '\t') p++;
    *out = (int32_t)strtol(p, NULL, 10);
    return true;
}

static const char* j_array_start(const char* json, const char* key) {
    size_t pos;
    if(!j_find_key(json, key, &pos)) return NULL;
    const char* p = strchr(json + pos, '[');
    return p ? p + 1 : NULL;
}

/* Walks one top-level element (quoted string or {..} object) starting at
 * *cursor, skipping leading whitespace/commas. Returns false at the
 * closing ']'. Advances *cursor past the element on success. */
static bool j_next_element(const char** cursor, const char** out_start, size_t* out_len) {
    const char* p = *cursor;
    while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
    if(*p == ']' || *p == '\0') {
        *cursor = p;
        return false;
    }

    const char* start = p;
    if(*p == '{') {
        int depth = 0;
        bool in_str = false;
        while(*p) {
            if(in_str) {
                if(*p == '\\' && p[1]) {
                    p++;
                } else if(*p == '"') {
                    in_str = false;
                }
            } else if(*p == '"') {
                in_str = true;
            } else if(*p == '{') {
                depth++;
            } else if(*p == '}') {
                depth--;
                if(depth == 0) {
                    p++;
                    break;
                }
            }
            p++;
        }
    } else if(*p == '"') {
        p++;
        while(*p && *p != '"') {
            if(*p == '\\' && p[1]) p++;
            p++;
        }
        if(*p == '"') p++;
    } else {
        while(*p && *p != ',' && *p != ']') p++;
    }
    *out_start = start;
    *out_len = (size_t)(p - start);
    *cursor = p;
    return true;
}

static void j_copy_unquoted(const char* start, size_t len, char* out, size_t cap) {
    size_t s = 0, e = len;
    if(len >= 2 && start[0] == '"' && start[len - 1] == '"') {
        s = 1;
        e = len - 1;
    }
    size_t i = 0;
    for(size_t k = s; k < e && i + 1 < cap; k++) {
        if(start[k] == '\\' && k + 1 < e) k++;
        out[i++] = start[k];
    }
    out[i] = 0;
}

/* ---- Escaping --------------------------------------------------------------*/

static void json_escape(char* dst, size_t dst_cap, const char* src) {
    size_t i = 0;
    for(const char* p = src; *p && i + 2 < dst_cap; p++) {
        if(*p == '"' || *p == '\\') {
            dst[i++] = '\\';
            dst[i++] = *p;
        } else if((unsigned char)*p >= 0x20) {
            dst[i++] = *p;
        }
    }
    dst[i] = 0;
}

static void url_encode(char* dst, size_t dst_cap, const char* src) {
    static const char hex[] = "0123456789ABCDEF";
    size_t i = 0;
    for(const unsigned char* p = (const unsigned char*)src; *p && i + 4 < dst_cap; p++) {
        unsigned char c = *p;
        bool safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                    c == '-' || c == '_' || c == '.' || c == '~';
        if(safe) {
            dst[i++] = (char)c;
        } else {
            dst[i++] = '%';
            dst[i++] = hex[c >> 4];
            dst[i++] = hex[c & 0xF];
        }
    }
    dst[i] = 0;
}

/* ---- Plugin manifest JSON -> TagTinkerWifiPlugin --------------------------*/

static void parse_plugin_json(
    uint8_t index,
    const char* obj,
    size_t obj_len,
    TagTinkerWifiPlugin* m,
    char* out_id,
    size_t out_id_cap) {
    static char scratch[2048];
    size_t take = obj_len < sizeof(scratch) - 1 ? obj_len : sizeof(scratch) - 1;
    memcpy(scratch, obj, take);
    scratch[take] = 0;

    memset(m, 0, sizeof(*m));
    m->index = index;
    j_get_string(scratch, "id", m->id, sizeof(m->id));
    j_get_string(scratch, "name", m->name, sizeof(m->name));
    j_get_string(scratch, "description", m->description, sizeof(m->description));
    int32_t acc = 1;
    j_get_int(scratch, "accent_modes", &acc);
    m->accent_modes = (uint8_t)acc;

    if(out_id) {
        strncpy(out_id, m->id, out_id_cap - 1);
        out_id[out_id_cap - 1] = 0;
    }

    const char* arr = j_array_start(scratch, "params");
    if(!arr) return;
    const char* cur = arr;
    const char* estart;
    size_t elen;
    while(m->param_count < TT_WIFI_MAX_PARAMS && j_next_element(&cur, &estart, &elen)) {
        static char pscratch[512];
        size_t ptake = elen < sizeof(pscratch) - 1 ? elen : sizeof(pscratch) - 1;
        memcpy(pscratch, estart, ptake);
        pscratch[ptake] = 0;

        TtWifiParam* pp = &m->params[m->param_count];
        j_get_string(pscratch, "key", pp->key, sizeof(pp->key));
        j_get_string(pscratch, "label", pp->label, sizeof(pp->label));
        char type_str[16] = "string";
        j_get_string(pscratch, "type", type_str, sizeof(type_str));
        if(strcmp(type_str, "int") == 0) {
            pp->type = TT_PARAM_INT;
        } else if(strcmp(type_str, "enum") == 0) {
            pp->type = TT_PARAM_ENUM;
        } else if(strcmp(type_str, "bool") == 0) {
            pp->type = TT_PARAM_BOOL;
        } else {
            pp->type = TT_PARAM_STRING;
        }
        j_get_string(pscratch, "default", pp->default_value, sizeof(pp->default_value));

        if(pp->type == TT_PARAM_ENUM) {
            const char* oarr = j_array_start(pscratch, "options");
            if(oarr) {
                const char* ocur = oarr;
                const char* ostart;
                size_t olen;
                while(pp->option_count < TT_WIFI_MAX_OPTIONS &&
                      j_next_element(&ocur, &ostart, &olen)) {
                    j_copy_unquoted(
                        ostart, olen, pp->options[pp->option_count],
                        sizeof(pp->options[pp->option_count]));
                    pp->option_count++;
                }
            }
        } else if(pp->type == TT_PARAM_INT) {
            int32_t mn = 0, mx = 100;
            j_get_int(pscratch, "min", &mn);
            j_get_int(pscratch, "max", &mx);
            pp->int_min = mn;
            pp->int_max = mx;
        }
        m->param_count++;
    }
}

/* ---- DOWNLOAD/START + DOWNLOAD/STREAM helpers -----------------------------*/

static WaitResult dl_start(EspAt* esp_at, const char* url, uint32_t* out_size, char* err, size_t err_size) {
    char cmd[420];
    snprintf(cmd, sizeof(cmd), "[DOWNLOAD/START]{\"url\":\"%s\"}", url);
    esp_at_send(esp_at, cmd);

    char rest[64] = {0};
    WaitResult r = wait_for_line(esp_at, "[DOWNLOAD/START/SUCCESS]", rest, sizeof(rest), DL_START_WAIT_MS);
    if(r == WlOk) {
        int32_t size = 0;
        j_get_int(rest, "size", &size);
        *out_size = size > 0 ? (uint32_t)size : 0;
    } else if(err) {
        snprintf(err, err_size, "%s", r == WlError ? rest : "Download could not start");
    }
    return r;
}

static WaitResult dl_stream_begin(EspAt* esp_at) {
    esp_at_send(esp_at, "[DOWNLOAD/STREAM]");
    return wait_for_line(esp_at, "[DOWNLOAD/STREAM/BEGIN]", NULL, 0, DL_START_WAIT_MS);
}

/* ---- Command handlers ------------------------------------------------------*/

static void handle_ping(TagTinkerWifi* w) {
    esp_at_send(w->esp_at, "[VERSION]");
    char rest[40] = {0};
    if(wait_for_line(w->esp_at, "[VERSION/SUCCESS]", rest, sizeof(rest), WIFI_LINE_WAIT_MS) == WlOk) {
        TtWifiEvent ev = {.type = TtWifiEvtHello, .str0 = rest};
        emit(w, &ev);
    } else {
        TtWifiEvent ev = {.type = TtWifiEvtLinkLost};
        emit(w, &ev);
    }
}

static void handle_query_status(TagTinkerWifi* w) {
    esp_at_send(w->esp_at, "[WIFI/STATUS]");
    char rest[8] = {0};
    if(wait_for_line(w->esp_at, "[WIFI/STATUS/SUCCESS]", rest, sizeof(rest), WIFI_LINE_WAIT_MS) != WlOk) {
        TtWifiEvent ev = {.type = TtWifiEvtLinkLost};
        emit(w, &ev);
        return;
    }

    bool connected = (strncmp(rest, "true", 4) == 0);
    char ssid[33] = {0}, ip[20] = {0};
    if(connected) {
        esp_at_send(w->esp_at, "[WIFI/SSID]");
        char ssid_rest[33] = {0};
        if(wait_for_line(w->esp_at, "[WIFI/SSID/SUCCESS]", ssid_rest, sizeof(ssid_rest), WIFI_LINE_WAIT_MS) ==
           WlOk) {
            strncpy(ssid, ssid_rest, sizeof(ssid) - 1);
        }

        esp_at_send(w->esp_at, "[IP/ADDRESS]");
        read_plain_line(w->esp_at, ip, sizeof(ip), WIFI_LINE_WAIT_MS);

        strncpy(w->last_ssid, ssid, sizeof(w->last_ssid) - 1);
    }

    TtWifiEvent ev = {
        .type = TtWifiEvtWifiStatus,
        .u0 = connected ? TT_WIFI_CONNECTED : TT_WIFI_DISCONNECTED,
        .i1 = 0,
        .str0 = ssid,
        .str1 = ip,
    };
    emit(w, &ev);
}

static void handle_set_creds(TagTinkerWifi* w, const WifiCmd* cmd) {
    char ssid_esc[80], pwd_esc[160];
    json_escape(ssid_esc, sizeof(ssid_esc), cmd->ssid);
    json_escape(pwd_esc, sizeof(pwd_esc), cmd->pwd);

    char buf[280];
    snprintf(buf, sizeof(buf), "[WIFI/SAVE]{\"ssid\":\"%s\",\"password\":\"%s\"}", ssid_esc, pwd_esc);
    esp_at_send(w->esp_at, buf);
    wait_for_line(w->esp_at, "[WIFI/SAVE/SUCCESS]", NULL, 0, WIFI_LINE_WAIT_MS);

    snprintf(buf, sizeof(buf), "[WIFI/CONNECT]{\"ssid\":\"%s\",\"password\":\"%s\"}", ssid_esc, pwd_esc);
    esp_at_send(w->esp_at, buf);
    char rest[64] = {0};
    WaitResult r = wait_for_line(w->esp_at, "[WIFI/CONNECT/SUCCESS]", rest, sizeof(rest), WIFI_CONNECT_WAIT_MS);

    if(r == WlOk) {
        strncpy(w->last_ssid, cmd->ssid, sizeof(w->last_ssid) - 1);
        w->last_ssid[sizeof(w->last_ssid) - 1] = 0;

        char ip[20] = {0};
        esp_at_send(w->esp_at, "[IP/ADDRESS]");
        read_plain_line(w->esp_at, ip, sizeof(ip), WIFI_LINE_WAIT_MS);

        TtWifiEvent ev = {
            .type = TtWifiEvtWifiStatus,
            .u0 = TT_WIFI_CONNECTED,
            .i1 = 0,
            .str0 = cmd->ssid,
            .str1 = ip,
        };
        emit(w, &ev);
    } else {
        TtWifiEvent ev = {
            .type = TtWifiEvtWifiStatus,
            .u0 = TT_WIFI_DISCONNECTED,
            .i1 = 0,
            .str0 = cmd->ssid,
            .str1 = "",
        };
        emit(w, &ev);
    }
}

static void handle_forget(TagTinkerWifi* w) {
    if(w->last_ssid[0]) {
        char ssid_esc[80];
        json_escape(ssid_esc, sizeof(ssid_esc), w->last_ssid);
        char buf[128];
        snprintf(buf, sizeof(buf), "[WIFI/FORGET]{\"ssid\":\"%s\"}", ssid_esc);
        esp_at_send(w->esp_at, buf);
        wait_for_line(w->esp_at, "[WIFI/FORGET/SUCCESS]", NULL, 0, WIFI_LINE_WAIT_MS);
    } else {
        esp_at_send(w->esp_at, "[WIFI/DISCONNECT]");
        wait_for_line(w->esp_at, "[DISCONNECTED]", NULL, 0, WIFI_LINE_WAIT_MS);
    }
    w->last_ssid[0] = 0;

    TtWifiEvent ev = {.type = TtWifiEvtWifiStatus, .u0 = TT_WIFI_DISCONNECTED, .str0 = "", .str1 = ""};
    emit(w, &ev);
}

static void handle_list_plugins(TagTinkerWifi* w) {
    char url[160];
    snprintf(url, sizeof(url), "%s/plugins", TT_CLOUD_BASE_URL);

    char err[64] = {0};
    uint32_t size = 0;
    w->cached_id_count = 0;

    if(dl_start(w->esp_at, url, &size, err, sizeof(err)) != WlOk) {
        TtWifiEvent eerr = {.type = TtWifiEvtError, .str0 = err[0] ? err : "plugin fetch failed"};
        emit(w, &eerr);
        TtWifiEvent eend = {.type = TtWifiEvtPluginsEnd};
        emit(w, &eend);
        return;
    }
    if(size == 0 || size > PLUGIN_BODY_MAX) {
        TtWifiEvent eerr = {.type = TtWifiEvtError, .str0 = "plugin list too large"};
        emit(w, &eerr);
        TtWifiEvent eend = {.type = TtWifiEvtPluginsEnd};
        emit(w, &eend);
        return;
    }
    if(dl_stream_begin(w->esp_at) != WlOk) {
        TtWifiEvent eerr = {.type = TtWifiEvtError, .str0 = "stream did not start"};
        emit(w, &eerr);
        TtWifiEvent eend = {.type = TtWifiEvtPluginsEnd};
        emit(w, &eend);
        return;
    }

    uint8_t* body = malloc(size + 1);
    if(!body) {
        TtWifiEvent eerr = {.type = TtWifiEvtError, .str0 = "out of memory"};
        emit(w, &eerr);
        TtWifiEvent eend = {.type = TtWifiEvtPluginsEnd};
        emit(w, &eend);
        return;
    }

    esp_at_begin_raw(w->esp_at);
    size_t got = esp_at_read_raw(w->esp_at, body, size, DL_STREAM_WAIT_MS);
    esp_at_end_raw(w->esp_at);
    body[got] = 0;
    bool stream_ok = dl_stream_end_ok(w->esp_at);

    if(got < size || !stream_ok) {
        free(body);
        TtWifiEvent eerr = {.type = TtWifiEvtError, .str0 = "plugin fetch incomplete"};
        emit(w, &eerr);
        TtWifiEvent eend = {.type = TtWifiEvtPluginsEnd};
        emit(w, &eend);
        return;
    }

    const char* arr = j_array_start((const char*)body, "plugins");
    if(arr) {
        const char* cur = arr;
        const char* estart;
        size_t elen;
        uint8_t idx = 0;
        while(idx < TT_WIFI_MAX_FAP_PLUGINS && j_next_element(&cur, &estart, &elen)) {
            parse_plugin_json(
                idx, estart, elen, &w->pending_plugin, w->cached_ids[idx], sizeof(w->cached_ids[idx]));
            w->cached_id_count = idx + 1;
            TtWifiEvent ev = {.type = TtWifiEvtPlugin, .plugin = &w->pending_plugin};
            emit(w, &ev);
            idx++;
        }
    }
    free(body);

    TtWifiEvent eend = {.type = TtWifiEvtPluginsEnd};
    emit(w, &eend);
}

static void handle_run_plugin(TagTinkerWifi* w, const WifiCmd* cmd) {
    if(cmd->plugin_index >= w->cached_id_count || w->cached_ids[cmd->plugin_index][0] == 0) {
        TtWifiEvent ev = {.type = TtWifiEvtError, .str0 = "no plugin cache (refresh first)"};
        emit(w, &ev);
        return;
    }

    TtWifiEvent prog = {.type = TtWifiEvtProgress, .u0 = 15, .str0 = "Connecting to cloud"};
    emit(w, &prog);

    const char* accent_str =
        cmd->accent == TT_ACCENT_RED ? "red" : (cmd->accent == TT_ACCENT_YELLOW ? "yellow" : "none");

    char url[420];
    int off = snprintf(
        url, sizeof(url), "%s/render/%s?w=%u&h=%u&accent=%s", TT_CLOUD_BASE_URL,
        w->cached_ids[cmd->plugin_index], cmd->target_w, cmd->target_h, accent_str);
    for(uint8_t i = 0; i < cmd->n_params && off > 0 && (size_t)off < sizeof(url) - 4; i++) {
        char enc[192];
        url_encode(enc, sizeof(enc), cmd->vals[i]);
        off += snprintf(url + off, sizeof(url) - (size_t)off, "&%s=%s", cmd->keys[i], enc);
    }

    char err[64] = {0};
    uint32_t total_size = 0;
    if(dl_start(w->esp_at, url, &total_size, err, sizeof(err)) != WlOk) {
        TtWifiEvent ev = {.type = TtWifiEvtError, .str0 = err[0] ? err : "render request failed"};
        emit(w, &ev);
        return;
    }
    if(total_size < 8) {
        TtWifiEvent ev = {.type = TtWifiEvtError, .str0 = "bad render response"};
        emit(w, &ev);
        return;
    }

    TtWifiEvent prog2 = {.type = TtWifiEvtProgress, .u0 = 50, .str0 = "Receiving image"};
    emit(w, &prog2);

    if(dl_stream_begin(w->esp_at) != WlOk) {
        TtWifiEvent ev = {.type = TtWifiEvtError, .str0 = "stream did not start"};
        emit(w, &ev);
        return;
    }

    esp_at_begin_raw(w->esp_at);

    uint8_t hdr[8];
    size_t hdr_got = esp_at_read_raw(w->esp_at, hdr, sizeof(hdr), DL_CHUNK_TIMEOUT_MS);
    if(hdr_got < sizeof(hdr)) {
        esp_at_end_raw(w->esp_at);
        TtWifiEvent ev = {.type = TtWifiEvtError, .str0 = "truncated image header"};
        emit(w, &ev);
        return;
    }
    uint16_t iw = (uint16_t)(hdr[0] | (hdr[1] << 8));
    uint16_t ih = (uint16_t)(hdr[2] | (hdr[3] << 8));
    uint8_t  planes = hdr[4];
    /* hdr[5] is reserved, hdr[6..7] is row_stride - not needed here since
     * the BMP writer derives its own row stride from width, same as it
     * always has (RESULT_BEGIN never carried row_stride either). */
    uint32_t pixel_total = total_size - 8;

    TtWifiEvent begin_ev = {
        .type = TtWifiEvtResultBegin,
        .u0 = ((uint32_t)ih << 16) | iw,
        .u1 = planes,
        .u2 = pixel_total,
    };
    emit(w, &begin_ev);

    static uint8_t chunk_buf[RENDER_STREAM_BUF];
    uint32_t received = 0;
    bool ok = true;
    uint8_t last_pct = 50;
    while(received < pixel_total) {
        size_t want = pixel_total - received;
        if(want > sizeof(chunk_buf)) want = sizeof(chunk_buf);
        size_t got = esp_at_read_raw(w->esp_at, chunk_buf, want, DL_CHUNK_TIMEOUT_MS);
        if(got == 0) {
            ok = false;
            break;
        }
        TtWifiEvent chunk_ev = {.type = TtWifiEvtResultChunk, .data = chunk_buf, .data_len = (uint16_t)got};
        emit(w, &chunk_ev);
        received += (uint32_t)got;

        if(pixel_total) {
            uint32_t pct = 50 + (received * 45) / pixel_total;
            if(pct > 95) pct = 95;
            if(pct >= (uint32_t)last_pct + 10) {
                TtWifiEvent p = {.type = TtWifiEvtProgress, .u0 = pct, .str0 = "Receiving image"};
                emit(w, &p);
                last_pct = (uint8_t)pct;
            }
        }
    }
    esp_at_end_raw(w->esp_at);

    bool stream_ok = dl_stream_end_ok(w->esp_at);

    if(!ok || received < pixel_total || !stream_ok) {
        TtWifiEvent ev = {.type = TtWifiEvtError, .str0 = "image download incomplete"};
        emit(w, &ev);
        return;
    }

    TtWifiEvent end_ev = {.type = TtWifiEvtResultEnd};
    emit(w, &end_ev);
}

/* ---- Worker thread ---------------------------------------------------------*/

static int32_t worker_thread(void* ctx) {
    TagTinkerWifi* w = ctx;
    WifiCmd cmd;
    while(w->running) {
        if(furi_message_queue_get(w->cmd_queue, &cmd, 200) != FuriStatusOk) continue;
        switch(cmd.type) {
        case WcPing:
            handle_ping(w);
            break;
        case WcQueryStatus:
            handle_query_status(w);
            break;
        case WcSetCreds:
            handle_set_creds(w, &cmd);
            break;
        case WcForget:
            handle_forget(w);
            break;
        case WcListPlugins:
            handle_list_plugins(w);
            break;
        case WcRunPlugin:
            handle_run_plugin(w, &cmd);
            break;
        }
    }
    return 0;
}

/* ---- Public API ------------------------------------------------------------*/

static void enqueue(TagTinkerWifi* w, const WifiCmd* cmd) {
    if(!w->esp_at) return;
    furi_message_queue_put(w->cmd_queue, cmd, 100);
}

void tagtinker_wifi_ping(TagTinkerWifi* w) {
    WifiCmd cmd = {.type = WcPing};
    enqueue(w, &cmd);
}

void tagtinker_wifi_query_status(TagTinkerWifi* w) {
    WifiCmd cmd = {.type = WcQueryStatus};
    enqueue(w, &cmd);
}

void tagtinker_wifi_list_plugins(TagTinkerWifi* w) {
    WifiCmd cmd = {.type = WcListPlugins};
    enqueue(w, &cmd);
}

void tagtinker_wifi_forget(TagTinkerWifi* w) {
    WifiCmd cmd = {.type = WcForget};
    enqueue(w, &cmd);
}

void tagtinker_wifi_set_creds(TagTinkerWifi* w, const char* ssid, const char* pwd) {
    WifiCmd cmd = {.type = WcSetCreds};
    strncpy(cmd.ssid, ssid ? ssid : "", sizeof(cmd.ssid) - 1);
    strncpy(cmd.pwd, pwd ? pwd : "", sizeof(cmd.pwd) - 1);
    enqueue(w, &cmd);
}

void tagtinker_wifi_run_plugin(
    TagTinkerWifi* w,
    uint8_t idx,
    uint16_t target_w,
    uint16_t target_h,
    uint8_t accent,
    const TtWifiKV* params,
    uint8_t n_params) {
    WifiCmd cmd = {
        .type = WcRunPlugin,
        .plugin_index = idx,
        .target_w = target_w,
        .target_h = target_h,
        .accent = accent,
    };
    if(n_params > TT_WIFI_MAX_PARAMS) n_params = TT_WIFI_MAX_PARAMS;
    cmd.n_params = n_params;
    for(uint8_t i = 0; i < n_params; i++) {
        strncpy(cmd.keys[i], params[i].key ? params[i].key : "", sizeof(cmd.keys[i]) - 1);
        strncpy(cmd.vals[i], params[i].value ? params[i].value : "", sizeof(cmd.vals[i]) - 1);
    }
    enqueue(w, &cmd);
}

/* ---- Lifecycle ---------------------------------------------------------- */

TagTinkerWifi* tagtinker_wifi_alloc(TtWifiEventCb cb, void* user) {
    TagTinkerWifi* w = malloc(sizeof(*w));
    memset(w, 0, sizeof(*w));
    w->cb = cb;
    w->user = user;
    w->cmd_queue = furi_message_queue_alloc(4, sizeof(WifiCmd));
    return w;
}

void tagtinker_wifi_free(TagTinkerWifi* w) {
    if(!w) return;
    tagtinker_wifi_close(w);
    if(w->cmd_queue) furi_message_queue_free(w->cmd_queue);
    free(w);
}

bool tagtinker_wifi_open(TagTinkerWifi* w) {
    if(w->esp_at) return true;

    GpioRemapSettings gpio_remap;
    gpio_remap_settings_load(&gpio_remap);
    FuriHalSerialId serial_id = (gpio_remap.esp32_uart_channel == GpioRemapEsp32UartLpuart)
                                     ? FuriHalSerialIdLpuart
                                     : FuriHalSerialIdUsart;
    w->esp_at = esp_at_alloc(serial_id, ESP_BAUD);
    if(!w->esp_at) return false;

    w->running = true;
    w->worker = furi_thread_alloc_ex("TtWifiCmd", 3072, worker_thread, w);
    furi_thread_start(w->worker);

    /* Fox ESP32 Firmware doesn't beacon on boot the way the old custom
     * firmware did, so we probe for it explicitly right after opening -
     * this is the new HELLO equivalent. */
    tagtinker_wifi_ping(w);
    return true;
}

void tagtinker_wifi_set_callback(
    TagTinkerWifi* w,
    TtWifiEventCb new_cb,
    void* new_user,
    TtWifiEventCb* out_prev_cb,
    void** out_prev_user) {
    if(out_prev_cb) *out_prev_cb = w->cb;
    if(out_prev_user) *out_prev_user = w->user;
    w->cb = new_cb;
    w->user = new_user;
}

void tagtinker_wifi_close(TagTinkerWifi* w) {
    if(!w->esp_at) return;
    w->running = false;
    if(w->worker) {
        furi_thread_join(w->worker);
        furi_thread_free(w->worker);
        w->worker = NULL;
    }
    esp_at_free(w->esp_at);
    w->esp_at = NULL;
}
