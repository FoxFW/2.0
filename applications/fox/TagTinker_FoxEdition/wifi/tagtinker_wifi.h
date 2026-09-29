#ifndef TAGTINKER_WIFI_H
#define TAGTINKER_WIFI_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

enum {
    TT_WIFI_DISCONNECTED = 0,
    TT_WIFI_CONNECTING   = 1,
    TT_WIFI_CONNECTED    = 2,
    TT_WIFI_AUTH_FAILED  = 3,
    TT_WIFI_NO_AP        = 4,
};

enum {
    TT_PARAM_STRING = 0,
    TT_PARAM_INT    = 1,
    TT_PARAM_ENUM   = 2,
    TT_PARAM_BOOL   = 3,
};

enum {
    TT_ACCENT_NONE   = 0,
    TT_ACCENT_RED    = 1,
    TT_ACCENT_YELLOW = 2,
};

typedef struct TagTinkerWifi TagTinkerWifi;

typedef enum {
    TtWifiEvtHello,
    TtWifiEvtWifiStatus,
    TtWifiEvtPlugin,
    TtWifiEvtPluginsEnd,
    TtWifiEvtProgress,
    TtWifiEvtResultBegin,

    TtWifiEvtResultChunk,
    TtWifiEvtResultEnd,
    TtWifiEvtError,
    TtWifiEvtLinkLost,
} TtWifiEventType;

#define TT_WIFI_MAX_PARAMS  6
#define TT_WIFI_MAX_OPTIONS 8

#define TT_WIFI_MAX_FAP_PLUGINS 8

typedef struct {
    char        key[24];
    char        label[24];
    uint8_t     type;
    char        default_value[64];
    uint8_t     option_count;
    char        options[TT_WIFI_MAX_OPTIONS][24];
    int32_t     int_min;
    int32_t     int_max;
} TtWifiParam;

typedef struct {
    uint8_t     index;
    char        id[24];
    char        name[40];
    char        description[64];
    uint8_t     accent_modes;
    uint8_t     param_count;
    TtWifiParam params[TT_WIFI_MAX_PARAMS];
} TagTinkerWifiPlugin;

typedef struct {
    TtWifiEventType type;
    uint32_t   u0, u1, u2;
    int32_t    i1;
    const char* str0;
    const char* str1;
    const TagTinkerWifiPlugin* plugin;
    const uint8_t* data; uint16_t data_len;
} TtWifiEvent;

typedef void (*TtWifiEventCb)(const TtWifiEvent* e, void* user);

TagTinkerWifi* tagtinker_wifi_alloc(TtWifiEventCb cb, void* user);
void           tagtinker_wifi_free (TagTinkerWifi* w);

bool tagtinker_wifi_open (TagTinkerWifi* w);
void tagtinker_wifi_close(TagTinkerWifi* w);

void tagtinker_wifi_set_callback(
    TagTinkerWifi* w,
    TtWifiEventCb new_cb, void* new_user,
    TtWifiEventCb* out_prev_cb, void** out_prev_user);

void tagtinker_wifi_ping        (TagTinkerWifi* w);
void tagtinker_wifi_set_creds   (TagTinkerWifi* w, const char* ssid, const char* pwd);
void tagtinker_wifi_forget      (TagTinkerWifi* w);
void tagtinker_wifi_query_status(TagTinkerWifi* w);
void tagtinker_wifi_list_plugins(TagTinkerWifi* w);

typedef struct { const char* key; const char* value; } TtWifiKV;
void tagtinker_wifi_run_plugin(
    TagTinkerWifi* w,
    uint8_t plugin_index,
    uint16_t target_w,
    uint16_t target_h,
    uint8_t accent,
    const TtWifiKV* params, uint8_t n_params);

#endif
