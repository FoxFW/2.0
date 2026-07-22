#include "wifi_menu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>
#include <furi_hal_rtc.h>

/* Keep this in sync by hand with the firmware's WIFI_PCAP_SNAPLEN
   (config.h) - no shared header between the two projects, same
   convention as FOX_PORTAL_MAX_FIELDS. Used both as the pcap global
   header's snaplen field and as the stack buffer size a captured
   frame is reassembled into. */
#define FOX_PCAP_SNAPLEN 256


typedef enum {
    MenuWifiConnect,
    MenuWifiMyIp,
    MenuWifiHttp,
    MenuWifiRecon,
    MenuWifiAttacks,
} MenuWifiIndex;

typedef enum {
    MenuConnDisconnect,
    MenuConnStatus,
    MenuConnSwitch,
    MenuConnForget,
} MenuWifiConnectionIndex;

/* One [WIFI/STATUS] round trip - see app->wifi_menu_connected's comment
   in app.h for why the result is cached rather than queried again on
   the immediately-following click. */
static bool wifi_is_connected(App* app) {
    esp_at_send(app->esp_at, "[WIFI/STATUS]");
    EspAtMsg msg;
    if(!esp_at_receive(app->esp_at, &msg, 5000)) return false;
    return strcmp(msg.line, "[WIFI/STATUS/SUCCESS]true") == 0;
}

typedef enum {
    MenuReconScanAp,
    MenuReconScanSta,
    MenuReconSelectAp,
    MenuReconSignalMonitor,
    MenuReconPacketCount,
    MenuReconWardrive,
    MenuReconPcap,
} MenuReconIndex;

typedef enum {
    MenuAttackSelectAp,
    MenuAttackSelectStation,
    MenuAttackDeauthBroadcast,
    MenuAttackDeauthTargeted,
    MenuAttackBeaconRandom,
    MenuAttackBeaconCustom,
    MenuAttackProbe,
} MenuAttackIndex;

void wifi_render_menu(App* app, MenuContext ctx) {
    submenu_reset(app->submenu);

    switch(ctx) {
    case MenuContextWifi:
        submenu_set_header(app->submenu, "WiFi");
        /* One status round trip per render, cached for the click - see
           app->wifi_menu_connected's comment in app.h. Already
           connected: label reads "Connection" and leads to the
           Disconnect/Status/Switch/Forget submenu below instead of
           straight into the scan/select/password flow. */
        app->wifi_menu_connected = wifi_is_connected(app);
        submenu_add_item(
            app->submenu,
            app->wifi_menu_connected ? "Connection" : "Connect",
            MenuWifiConnect,
            app_menu_item_callback,
            app);
        submenu_add_item(app->submenu, "My IP", MenuWifiMyIp, app_menu_item_callback, app);
        submenu_add_item(
            app->submenu, "HTTP Request", MenuWifiHttp, app_menu_item_callback, app);
        submenu_add_item(app->submenu, "Recon", MenuWifiRecon, app_menu_item_callback, app);
        submenu_add_item(app->submenu, "Attacks", MenuWifiAttacks, app_menu_item_callback, app);
        break;
    case MenuContextWifiConnection:
        submenu_set_header(app->submenu, "Connection");
        submenu_add_item(
            app->submenu, "Disconnect", MenuConnDisconnect, app_menu_item_callback, app);
        submenu_add_item(app->submenu, "Status", MenuConnStatus, app_menu_item_callback, app);
        submenu_add_item(
            app->submenu, "Connect to Network", MenuConnSwitch, app_menu_item_callback, app);
        submenu_add_item(
            app->submenu, "Forget Network", MenuConnForget, app_menu_item_callback, app);
        break;
    case MenuContextWifiRecon:
        submenu_set_header(app->submenu, "WiFi Recon");
        submenu_add_item(app->submenu, "Scan APs", MenuReconScanAp, app_menu_item_callback, app);
        submenu_add_item(
            app->submenu, "Scan Stations", MenuReconScanSta, app_menu_item_callback, app);
        submenu_add_item(
            app->submenu, "Select AP", MenuReconSelectAp, app_menu_item_callback, app);
        submenu_add_item(
            app->submenu,
            "Signal Monitor",
            MenuReconSignalMonitor,
            app_menu_item_callback,
            app);
        submenu_add_item(
            app->submenu, "Packet Count", MenuReconPacketCount, app_menu_item_callback, app);
        submenu_add_item(
            app->submenu, "Wardrive", MenuReconWardrive, app_menu_item_callback, app);
        submenu_add_item(
            app->submenu, "Packet Capture", MenuReconPcap, app_menu_item_callback, app);
        break;
    case MenuContextWifiAttacks:
        submenu_set_header(app->submenu, "WiFi Attacks");
        submenu_add_item(
            app->submenu, "Select AP", MenuAttackSelectAp, app_menu_item_callback, app);
        submenu_add_item(
            app->submenu, "Select Station", MenuAttackSelectStation, app_menu_item_callback, app);
        submenu_add_item(
            app->submenu,
            "Deauth (Broadcast)",
            MenuAttackDeauthBroadcast,
            app_menu_item_callback,
            app);
        submenu_add_item(
            app->submenu,
            "Deauth (Targeted)",
            MenuAttackDeauthTargeted,
            app_menu_item_callback,
            app);
        submenu_add_item(
            app->submenu,
            "Beacon Spam (Random)",
            MenuAttackBeaconRandom,
            app_menu_item_callback,
            app);
        submenu_add_item(
            app->submenu,
            "Beacon Spam (Custom)",
            MenuAttackBeaconCustom,
            app_menu_item_callback,
            app);
        submenu_add_item(app->submenu, "Probe Flood", MenuAttackProbe, app_menu_item_callback, app);
        break;
    default:
        break;
    }
}


static void wifi_scan_and_merge(App* app, bool include_saved_out_of_range) {
    app->network_count = 0;

    /* --- WIFISCANAP: AP:<idx> ssid:"<ssid>" bssid:<mac> ch:<n> rssi:<n> enc:<label> --- */
    esp_at_send(app->esp_at, "WIFISCANAP");
    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + 15000;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        if(strcmp(msg.line, "SCANDONE") == 0) break;
        /* Firmware sends this immediately before SCANDONE when
           WiFi.scanNetworks() itself failed (negative return), as
           opposed to succeeding with zero APs in range - surface it so
           "No networks found" isn't ambiguous between "none nearby" and
           "the scan errored out". */
        if(strncmp(msg.line, "SCANERROR:", 10) == 0) {
            app_log(app, "Scan error (code %s).", msg.line + 10);
            continue;
        }

        int idx = -1;
        char ssid[FOX_WIFI_SSID_MAX] = {0};
        int rssi = 0;
        char enc[16] = {0};
        /* "AP:%d ssid:\"%[^\"]\" bssid:%*s ch:%*d rssi:%d enc:%15s" -
           bssid/ch are read but not needed here (WIFISELECT:AP takes an
           index, not a BSSID), so they're skipped with %*. */
        int matched = sscanf(
            msg.line,
            "AP:%d ssid:\"%32[^\"]\" bssid:%*s ch:%*d rssi:%d enc:%15s",
            &idx,
            ssid,
            &rssi,
            enc);
        if(matched == 4 && app->network_count < FOX_WIFI_NETWORK_MAX) {
            FoxWifiNetwork* n = &app->networks[app->network_count];
            strncpy(n->ssid, ssid, sizeof(n->ssid) - 1);
            n->ssid[sizeof(n->ssid) - 1] = '\0';
            n->rssi = rssi;
            n->secure = (strcmp(enc, "OPEN") != 0);
            n->saved = false;
            n->scan_index = idx;
            app->network_count++;
        }
    }

    if(!include_saved_out_of_range) return;

    /* --- [WIFI/LIST]: one "[WIFI/LIST]<ssid>" line per saved network,
       then "[WIFI/LIST/SUCCESS]". Mark matches against what's already
       in the list from the scan above (saved AND in range); append
       anything saved that isn't currently in range. --- */
    esp_at_send(app->esp_at, "[WIFI/LIST]");
    deadline = furi_get_tick() + 3000;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        if(strcmp(msg.line, "[WIFI/LIST/SUCCESS]") == 0) break;
        if(strncmp(msg.line, "[WIFI/LIST]", 11) != 0) continue;

        const char* ssid = msg.line + 11;
        if(ssid[0] == '\0') continue;

        bool found = false;
        for(size_t i = 0; i < app->network_count; i++) {
            if(strcmp(app->networks[i].ssid, ssid) == 0) {
                app->networks[i].saved = true;
                found = true;
                break;
            }
        }
        if(!found && app->network_count < FOX_WIFI_NETWORK_MAX) {
            FoxWifiNetwork* n = &app->networks[app->network_count];
            strncpy(n->ssid, ssid, sizeof(n->ssid) - 1);
            n->ssid[sizeof(n->ssid) - 1] = '\0';
            n->rssi = 0;
            n->secure = false;
            n->saved = true;
            n->scan_index = -1;
            app->network_count++;
        }
    }

    /* Saved networks first, per the original request ("list their
       saved wifi's first, and then list the wifi's that it can
       detect") - a simple stable-ish selection sort on the small (<=24
       entry) array is plenty; this isn't a hot path. */
    for(size_t i = 0; i < app->network_count; i++) {
        size_t best = i;
        for(size_t j = i + 1; j < app->network_count; j++) {
            if(app->networks[j].saved && !app->networks[best].saved) best = j;
        }
        if(best != i) {
            FoxWifiNetwork tmp = app->networks[i];
            app->networks[i] = app->networks[best];
            app->networks[best] = tmp;
        }
    }
}

void wifi_network_list_show(App* app, bool for_connect) {
    app->menu_return_context = app->menu_context;
    app_log(app, for_connect ? "Scanning for networks..." : "Scanning for AP targets...");
    /* Switch to Terminal immediately, before the blocking scan below -
       otherwise the screen just sits on the menu (looking frozen/
       unresponsive) for however long WIFISCANAP + [WIFI/LIST] take,
       then jumps straight to a fully-populated result. This way
       "Scanning for..." is visible right away, matching what a click
       on Connect should feel like. */
    app_render_log(app);

    wifi_scan_and_merge(app, for_connect);

    if(for_connect) {
        /* Target mode only wants networks with a live scan_index (see
           wifi_scan_and_merge's comment) - filter those out here rather
           than complicating the merge function with two return shapes. */
    } else {
        size_t write = 0;
        for(size_t i = 0; i < app->network_count; i++) {
            if(app->networks[i].scan_index >= 0) {
                app->networks[write++] = app->networks[i];
            }
        }
        app->network_count = write;
    }

    if(app->network_count == 0) {
        app_log(app, for_connect ? "No networks found." : "No APs in range - try Scan APs first.");
        app_render_log(app);
        return;
    }

    app->network_list_for_connect = for_connect;
    app->network_selected = 0;
    app->network_scroll = 0;
    app->current_view = FoxCommanderViewNetworkList;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewNetworkList);
}


/* Escapes " and \ (the two characters that would otherwise break the
   JSON string they're embedded in) from in into out, truncating safely
   rather than overflowing if the escaped result wouldn't fit. Every
   [WIFI/CONNECT] / [WIFI/SAVE] command below builds its JSON with a
   plain snprintf %s - an un-escaped quote or backslash in an SSID or
   password would corrupt the command and produce a failure on the
   firmware side that looks exactly like a wrong password, but is
   actually a parsing bug. */
/* Writes "1" or "0" to the SD-card flag file the desktop app's status
   bar WiFi icon polls (see desktop.c's FOX_ESP32_WIFI_STATUS_PATH /
   desktop_wifi_status_timer_callback() in the firmware fork's own
   source, not part of this app). This app is the only one with a WiFi
   Connect/Disconnect/Forget menu, so it's the only one that needs to
   write this - fox_chat/fox_portal just use whatever WiFi the ESP32
   already has and never change it. Best-effort: if the SD card is out
   or the write fails, the status bar just shows stale/default state
   until the next successful write - not worth surfacing an error to
   the user over a status icon. */
static void fox_wifi_status_write(bool connected) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, "/ext/apps_data/fox_esp32");

    File* file = storage_file_alloc(storage);
    if(storage_file_open(
           file, "/ext/apps_data/fox_esp32/wifi_status.txt", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* v = connected ? "1" : "0";
        storage_file_write(file, v, 1);
    }
    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);
}

static void json_escape(const char* in, char* out, size_t out_size) {
    size_t o = 0;
    for(size_t i = 0; in[i] != '\0' && o + 2 < out_size; i++) {
        char c = in[i];
        if(c == '"' || c == '\\') out[o++] = '\\';
        out[o++] = c;
    }
    out[o] = '\0';
}

void wifi_password_submitted(App* app) {
    const char* ssid = furi_string_get_cstr(app->pending_ssid);
    const char* pass = app->text_input_buffer;

    char ssid_esc[FOX_WIFI_SSID_MAX * 2];
    char pass_esc[FOX_TEXT_INPUT_BUFFER_MAX * 2];
    json_escape(ssid, ssid_esc, sizeof(ssid_esc));
    json_escape(pass, pass_esc, sizeof(pass_esc));

    char cmd[sizeof(ssid_esc) + sizeof(pass_esc) + 40];
    snprintf(
        cmd,
        sizeof(cmd),
        "[WIFI/CONNECT]{\"ssid\":\"%s\",\"password\":\"%s\"}",
        ssid_esc,
        pass_esc);

    /* Byte-for-byte confirmed correct end to end: whatever the keyboard
       hands us here matches what the firmware parses on the other end.
       So the one failure mode left is a mistyped password that *looks*
       right on the entry screen but isn't - echoing exactly what got
       captured, right before we act on it, is the only way to catch
       that before waiting out a whole connect attempt. */
    app_log(app, "Entered: \"%s\" (%d chars)", pass, (int)strlen(pass));

    esp_at_send(app->esp_at, cmd);

    app_log(app, "Connecting to %s...", ssid);
    app_render_log(app);
    EspAtMsg msg;
    /* Firmware's own connect attempt now waits up to 20s (WPA handshake
       + DHCP can be slow on some routers) - this needs to stay longer
       than that or the app can give up and report a false timeout right
       as the firmware was about to succeed. */
    uint32_t deadline = furi_get_tick() + 21000;
    bool connected = false;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        app_log(app, "%s", msg.line);
        if(strncmp(msg.line, "[WIFI/CONNECT/SUCCESS]", 22) == 0) {
            connected = true;
            break;
        }
        if(strncmp(msg.line, "[ERROR]", 7) == 0) break;
    }

    if(connected) {
        char save_cmd[sizeof(ssid_esc) + sizeof(pass_esc) + 40];
        snprintf(
            save_cmd,
            sizeof(save_cmd),
            "[WIFI/SAVE]{\"ssid\":\"%s\",\"password\":\"%s\"}",
            ssid_esc,
            pass_esc);
        esp_at_send(app->esp_at, save_cmd);
        app_expect_line(app, "[WIFI/SAVE/SUCCESS]", 3000);
        app_log(app, "Saved for next time.");
    }
    fox_wifi_status_write(connected);

    app_render_log(app);
}

static void network_connect_selected(App* app, const FoxWifiNetwork* n) {
    if(n->saved) {
        char ssid_esc[FOX_WIFI_SSID_MAX * 2];
        json_escape(n->ssid, ssid_esc, sizeof(ssid_esc));
        char cmd[sizeof(ssid_esc) + 32];
        snprintf(cmd, sizeof(cmd), "[WIFI/CONNECT]{\"ssid\":\"%s\"}", ssid_esc);
        esp_at_send(app->esp_at, cmd);

        app_log(app, "Connecting to %s...", n->ssid);
        app_render_log(app);
        EspAtMsg msg;
        bool connected = false;
        /* See wifi_password_submitted()'s comment - must stay longer
           than the firmware's own 20s connect timeout. */
        uint32_t deadline = furi_get_tick() + 21000;
        while(furi_get_tick() < deadline) {
            uint32_t remaining = deadline - furi_get_tick();
            if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
            app_log(app, "%s", msg.line);
            if(strncmp(msg.line, "[WIFI/CONNECT/SUCCESS]", 22) == 0) {
                connected = true;
                break;
            }
            if(strncmp(msg.line, "[ERROR]", 7) == 0) break;
        }
        fox_wifi_status_write(connected);
        app_render_log(app);
        return;
    }

    /* Not saved yet - ask for a password. Open networks still go
       through this screen (submitting empty text is valid - the
       firmware treats a missing/empty password as an open-network
       connect attempt, see http_bridge.cpp's handleWifiConnect). */
    furi_string_set(app->pending_ssid, n->ssid);
    char header[48];
    /* The case-toggle key on Flipper's stock keyboard has no visible
       label (it's a small icon, easy to miss), and the keyboard starts
       in uppercase - so a lowercase-containing password typed without
       noticing that key produces a silent wrong-password failure that
       looks identical to a real connection problem. Naming the key
       directly in the header is the cheapest real fix short of building
       a whole custom keyboard. */
    snprintf(header, sizeof(header), "Pass(Aa key): %.20s", n->ssid);
    app_show_text_input(app, header, TextInputPurposePassword);
}

static void network_target_selected(App* app, const FoxWifiNetwork* n) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "WIFISELECT:AP:%d", n->scan_index);
    esp_at_send(app->esp_at, cmd);

    app_log(app, "Selecting %s...", n->ssid);
    app_render_log(app);

    EspAtMsg msg;
    bool ok = false;
    if(esp_at_receive(app->esp_at, &msg, 1500)) {
        app_log(app, "%s", msg.line);
        ok = (strcmp(msg.line, "OK") == 0);
    }
    app_log(app, ok ? "%s selected as target." : "Selection failed.", n->ssid);
    app_render_log(app);
}

static void network_select(App* app, size_t index) {
    if(index >= app->network_count) return;
    const FoxWifiNetwork* n = &app->networks[index];
    if(app->network_list_for_connect) {
        network_connect_selected(app, n);
    } else {
        network_target_selected(app, n);
    }
}


static void station_scan(App* app) {
    app->station_count = 0;

    esp_at_send(app->esp_at, "WIFISCANSTA");
    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + 11000;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        if(strcmp(msg.line, "SCANDONE") == 0) break;
        if(strncmp(msg.line, "STA:", 4) != 0) continue;
        if(app->station_count >= FOX_STATION_MAX) continue;

        char mac[FOX_STATION_MAC_MAX] = {0};
        int rssi = 0;
        /* "STA:%d mac:%17s rssi:%d" - the index (%*d) isn't needed here,
           WIFIATTACK:DEAUTH takes a MAC, not an index. */
        int matched = sscanf(msg.line, "STA:%*d mac:%17s rssi:%d", mac, &rssi);
        if(matched == 2) {
            FoxStation* s = &app->stations[app->station_count];
            strncpy(s->mac, mac, sizeof(s->mac) - 1);
            s->mac[sizeof(s->mac) - 1] = '\0';
            s->rssi = rssi;
            app->station_count++;
        }
    }
}

static void station_list_show(App* app) {
    app->menu_return_context = app->menu_context;
    app_log(app, "Scanning stations (8s)...");
    app_render_log(app);

    station_scan(app);

    if(app->station_count == 0) {
        app_log(app, "No stations found - try again once devices are active.");
        app_render_log(app);
        return;
    }

    app->station_selected = 0;
    app->station_scroll = 0;
    app->current_view = FoxCommanderViewStationList;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewStationList);
}

static void station_select(App* app, size_t index) {
    if(index >= app->station_count) return;
    const FoxStation* s = &app->stations[index];
    strncpy(app->target_station_mac, s->mac, sizeof(app->target_station_mac) - 1);
    app->target_station_mac[sizeof(app->target_station_mac) - 1] = '\0';
    app->has_target_station = true;
    app_log(app, "%s selected as deauth target.", s->mac);
    app_render_log(app);
}


static void action_scan_aps(App* app) {
    app_log(app, "Scanning APs...");
    app_render_log(app);
    esp_at_send(app->esp_at, "WIFISCANAP");
    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + 15000;
    int count = 0;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        if(strcmp(msg.line, "SCANDONE") == 0) break;
        if(strncmp(msg.line, "SCANERROR:", 10) == 0) {
            app_log(app, "Scan error (code %s).", msg.line + 10);
            continue;
        }
        if(strncmp(msg.line, "AP:", 3) == 0) {
            app_log(app, "%s", msg.line);
            count++;
        }
    }
    app_log(app, "%d AP(s) found.", count);
    app_render_log(app);
}

static void action_scan_stas(App* app) {
    app_log(app, "Scanning stations (8s)...");
    app_render_log(app);
    esp_at_send(app->esp_at, "WIFISCANSTA");
    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + 11000;
    int count = 0;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        if(strcmp(msg.line, "SCANDONE") == 0) break;
        if(strncmp(msg.line, "STA:", 4) == 0) {
            app_log(app, "%s", msg.line);
            count++;
        }
    }
    app_log(app, "%d station(s) found.", count);
    app_render_log(app);
}

static void action_signal_monitor(App* app) {
    app_log(app, "Signal monitor (10s)...");
    app_render_log(app);
    esp_at_send(app->esp_at, "WIFISIGMON");
    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + 13000;
    int samples = 0;
    int last_rssi = 0;
    bool got_error = false;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        if(strcmp(msg.line, "DONE") == 0) break;
        if(strcmp(msg.line, "ERROR") == 0) {
            got_error = true;
            break;
        }
        if(strncmp(msg.line, "RSSI:", 5) == 0) {
            samples++;
            last_rssi = atoi(msg.line + 5);
        }
    }
    if(got_error) {
        app_log(app, "No AP selected - use Select AP first.");
    } else {
        app_log(app, "%d sample(s), last %ddBm.", samples, last_rssi);
    }
    app_render_log(app);
}

static void action_packet_count(App* app) {
    app_log(app, "Counting packets (5s)...");
    app_render_log(app);
    esp_at_send(app->esp_at, "WIFIPACKETCOUNT");
    EspAtMsg msg;
    if(esp_at_receive(app->esp_at, &msg, 8000)) {
        app_log(app, "%s", msg.line);
    } else {
        app_log(app, "No response.");
    }
    app_render_log(app);
}


static void csv_escape_field(const char* in, char* out, size_t out_size) {
    bool needs_quote = false;
    for(size_t i = 0; in[i] != '\0'; i++) {
        if(in[i] == ',' || in[i] == '"' || in[i] == '\n') {
            needs_quote = true;
            break;
        }
    }
    if(!needs_quote) {
        snprintf(out, out_size, "%s", in);
        return;
    }
    size_t o = 0;
    if(o + 1 < out_size) out[o++] = '"';
    for(size_t i = 0; in[i] != '\0' && o + 2 < out_size; i++) {
        if(in[i] == '"' && o + 2 < out_size) out[o++] = '"';
        if(o + 1 < out_size) out[o++] = in[i];
    }
    if(o + 1 < out_size) out[o++] = '"';
    out[o] = '\0';
}

/* Parses one "WARDRIVE:ssid:"<ssid>" bssid:<mac> ch:<n> rssi:<n>
   enc:<label> pos:<lat,lon|NOFIX>" line (wifi_recon.cpp's
   runWardrive()) into its fields. pos_out is left as the raw
   "<lat>,<lon>" or "NOFIX" text - splitting that further is the
   caller's job, since a missing fix has no lat/lon to split. */
static bool wardrive_parse_line(
    const char* line,
    char* ssid,
    size_t ssid_size,
    char* bssid,
    size_t bssid_size,
    int* channel,
    int* rssi,
    char* enc,
    size_t enc_size,
    char* pos,
    size_t pos_size) {
    if(strncmp(line, "WARDRIVE:ssid:\"", 15) != 0) return false;
    const char* p = line + 15;
    const char* end = strchr(p, '"');
    if(end == NULL) return false;
    size_t n = (size_t)(end - p);
    if(n >= ssid_size) n = ssid_size - 1;
    memcpy(ssid, p, n);
    ssid[n] = '\0';
    p = end + 1;

    const char* tag = strstr(p, "bssid:");
    if(tag == NULL) return false;
    p = tag + 6;
    end = strchr(p, ' ');
    if(end == NULL) return false;
    n = (size_t)(end - p);
    if(n >= bssid_size) n = bssid_size - 1;
    memcpy(bssid, p, n);
    bssid[n] = '\0';
    p = end + 1;

    tag = strstr(p, "ch:");
    if(tag == NULL) return false;
    *channel = atoi(tag + 3);

    tag = strstr(p, "rssi:");
    if(tag == NULL) return false;
    *rssi = atoi(tag + 5);

    tag = strstr(p, "enc:");
    if(tag == NULL) return false;
    p = tag + 4;
    end = strchr(p, ' ');
    if(end == NULL) return false;
    n = (size_t)(end - p);
    if(n >= enc_size) n = enc_size - 1;
    memcpy(enc, p, n);
    enc[n] = '\0';

    tag = strstr(p, "pos:");
    if(tag == NULL) return false;
    snprintf(pos, pos_size, "%s", tag + 4);
    return true;
}

static void run_wardrive(App* app) {
    app_log(app, "Wardriving (scan + GPS tag)...");
    app_render_log(app);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, "/ext/apps_data/fox_esp32");
    storage_simply_mkdir(storage, "/ext/apps_data/fox_esp32/wardrive");

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char date[16];
    snprintf(date, sizeof(date), "%04u-%02u-%02u", dt.year, dt.month, dt.day);
    /* 32, not 20 - hour/minute/second are uint8_t (range 0-255 as far
       as the compiler's concerned), so -Wformat-truncation sizes each
       "%02u" for a worst-case 3-digit result even though a real RTC
       never reports any of them above 2 digits. Same fix as
       foxportal_menu.c's own dated-log-filename buffer earlier in this
       project - oversize rather than fight the warning. */
    char timestamp[32];
    snprintf(
        timestamp,
        sizeof(timestamp),
        "%04u-%02u-%02u %02u:%02u:%02u",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);

    char path[72];
    snprintf(path, sizeof(path), "/ext/apps_data/fox_esp32/wardrive/wardrive_%s.csv", date);
    bool is_new = !storage_file_exists(storage, path);

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        app_log(app, "Could not open wardrive log on SD card.");
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        app_render_log(app);
        return;
    }
    if(is_new) {
        static const char* header =
            "bssid,ssid,latitude,longitude,rssi,channel,encryption_type,timestamp\n";
        storage_file_write(file, header, strlen(header));
    }

    esp_at_send(app->esp_at, "WIFIWARDRIVE");

    EspAtMsg msg;
    int logged = 0;
    bool scan_error = false;
    uint32_t deadline = furi_get_tick() + 15000;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        if(strcmp(msg.line, "WARDRIVEDONE") == 0) break;
        if(strncmp(msg.line, "SCANERROR:", 10) == 0) {
            scan_error = true;
            continue;
        }
        if(strncmp(msg.line, "WARDRIVE:", 9) != 0) continue;

        char ssid[FOX_WIFI_SSID_MAX];
        char bssid[FOX_STATION_MAC_MAX];
        char enc[16];
        char pos[40];
        int channel = 0, rssi = 0;
        if(!wardrive_parse_line(
               msg.line,
               ssid,
               sizeof(ssid),
               bssid,
               sizeof(bssid),
               &channel,
               &rssi,
               enc,
               sizeof(enc),
               pos,
               sizeof(pos))) {
            continue;
        }

        /* Sized to match pos[40] itself (the source these are copied
           from), not to the realistic length of an actual lat/lon
           string - the compiler's -Wformat-truncation bounds a "%s"
           copy against the *declared capacity* of the source buffer,
           not what will actually be in it at runtime (it can't know
           `pos` was just truncated at the comma above), so a tighter
           buffer here would just trade a real bug for a compiler
           warning-turned-error. Same "oversize rather than fight it"
           call as timestamp[] above. */
        char lat[40] = "";
        char lon[40] = "";
        if(strcmp(pos, "NOFIX") != 0) {
            char* comma = strchr(pos, ',');
            if(comma != NULL) {
                *comma = '\0';
                snprintf(lat, sizeof(lat), "%s", pos);
                snprintf(lon, sizeof(lon), "%s", comma + 1);
            }
        }

        char ssid_csv[FOX_WIFI_SSID_MAX * 2 + 4];
        csv_escape_field(ssid, ssid_csv, sizeof(ssid_csv));

        /* Same reasoning again - the compiler bounds this against the
           full declared capacity of every "%s" argument (bssid,
           ssid_csv, lat, lon, enc, timestamp) plus worst-case %d width,
           not their actual runtime contents, so the destination needs
           real headroom, not just a "reasonable" CSV row length. Sized
           generously (a real row is under 100 bytes) rather than
           computed to the exact byte, same margin-over-minimum call as
           timestamp[]/lat[]/lon[] above. */
        char row[300];
        snprintf(
            row,
            sizeof(row),
            "%s,%s,%s,%s,%d,%d,%s,%s\n",
            bssid,
            ssid_csv,
            lat,
            lon,
            rssi,
            channel,
            enc,
            timestamp);
        storage_file_write(file, row, strlen(row));
        logged++;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(scan_error) {
        app_log(app, "Scan error - %d network(s) logged before it occurred.", logged);
    } else {
        app_log(app, "%d network(s) logged to %s", logged, path);
    }
    app_render_log(app);
}

static void put_u32_le(uint8_t* buf, uint32_t v) {
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
    buf[2] = (uint8_t)((v >> 16) & 0xFF);
    buf[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void put_u16_le(uint8_t* buf, uint16_t v) {
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
}

static bool hex_nibble(char c, uint8_t* out) {
    if(c >= '0' && c <= '9') {
        *out = (uint8_t)(c - '0');
        return true;
    }
    if(c >= 'a' && c <= 'f') {
        *out = (uint8_t)(c - 'a' + 10);
        return true;
    }
    if(c >= 'A' && c <= 'F') {
        *out = (uint8_t)(c - 'A' + 10);
        return true;
    }
    return false;
}

/* Decodes a PCAPDATA: hex chunk directly into out. Returns 0 (nothing
   written) on any malformed input - an odd-length string or a non-hex
   character - so a corrupted line just contributes no bytes to the
   frame rather than writing garbage. */
static size_t hex_decode(const char* hex, uint8_t* out, size_t out_capacity) {
    size_t len = strlen(hex);
    if(len == 0 || len % 2 != 0) return 0;
    size_t n = len / 2;
    if(n > out_capacity) n = out_capacity;
    for(size_t i = 0; i < n; i++) {
        uint8_t hi, lo;
        if(!hex_nibble(hex[i * 2], &hi) || !hex_nibble(hex[i * 2 + 1], &lo)) return 0;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return n;
}

static void run_pcap_capture(App* app) {
    app_log(app, "Capturing WiFi packets (15s)...");
    app_render_log(app);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, "/ext/apps_data/fox_esp32");
    storage_simply_mkdir(storage, "/ext/apps_data/fox_esp32/captures");

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char path[80];
    snprintf(
        path,
        sizeof(path),
        "/ext/apps_data/fox_esp32/captures/capture_%04u%02u%02u_%02u%02u%02u.pcap",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        app_log(app, "Could not create capture file on SD card.");
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        app_render_log(app);
        return;
    }

    uint8_t ghdr[24];
    put_u32_le(ghdr + 0, 0xa1b2c3d4);
    put_u16_le(ghdr + 4, 2);
    put_u16_le(ghdr + 6, 4);
    put_u32_le(ghdr + 8, 0);
    put_u32_le(ghdr + 12, 0);
    put_u32_le(ghdr + 16, FOX_PCAP_SNAPLEN);
    put_u32_le(ghdr + 20, 105); /* LINKTYPE_IEEE802_11 */
    storage_file_write(file, ghdr, sizeof(ghdr));

    esp_at_send(app->esp_at, "WIFIPCAP");

    uint32_t base_epoch = furi_hal_rtc_get_timestamp();
    uint32_t base_ms = 0;
    bool have_base = false;
    int packet_count = 0;

    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + 20000;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;

        if(strncmp(msg.line, "PCAPDONE:", 9) == 0) {
            packet_count = atoi(msg.line + 9);
            break;
        }
        if(strncmp(msg.line, "PCAPPKT:", 8) != 0) continue;

        int capLen = 0, origLen = 0;
        unsigned long tsMs = 0;
        if(sscanf(msg.line + 8, "%d:%d:%lu", &capLen, &origLen, &tsMs) != 3) continue;
        if(capLen < 0) capLen = 0;
        if(capLen > FOX_PCAP_SNAPLEN) capLen = FOX_PCAP_SNAPLEN;

        if(!have_base) {
            base_ms = (uint32_t)tsMs;
            have_base = true;
        }
        uint32_t delta_ms = (uint32_t)tsMs - base_ms;
        uint32_t ts_sec = base_epoch + delta_ms / 1000;
        uint32_t ts_usec = (delta_ms % 1000) * 1000;

        uint8_t frame[FOX_PCAP_SNAPLEN];
        size_t written = 0;
        bool frame_ok = true;

        while(true) {
            if(!esp_at_receive(app->esp_at, &msg, 3000)) {
                frame_ok = false;
                break;
            }
            if(strcmp(msg.line, "PCAPEND") == 0) break;
            if(strncmp(msg.line, "PCAPDATA:", 9) != 0) {
                frame_ok = false;
                break;
            }
            size_t remaining_cap = (size_t)capLen > written ? (size_t)capLen - written : 0;
            written += hex_decode(msg.line + 9, frame + written, remaining_cap);
        }
        if(!frame_ok) break;

        uint8_t rhdr[16];
        put_u32_le(rhdr + 0, ts_sec);
        put_u32_le(rhdr + 4, ts_usec);
        put_u32_le(rhdr + 8, (uint32_t)written);
        put_u32_le(rhdr + 12, (uint32_t)origLen);
        storage_file_write(file, rhdr, sizeof(rhdr));
        if(written > 0) storage_file_write(file, frame, written);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    app_log(app, "%d packet(s) captured to %s", packet_count, path);
    app_render_log(app);
}


static void run_attack(App* app, const char* command, const char* label) {
    app_log(app, "%s...", label);
    app_render_log(app);
    esp_at_send(app->esp_at, command);
    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + 8000;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        app_log(app, "%s", msg.line);
        if(strcmp(msg.line, "ATTACKDONE") == 0) break;
        if(strncmp(msg.line, "ERROR", 5) == 0) break;
    }
    app_render_log(app);
}

static void action_deauth_targeted(App* app) {
    if(!app->has_target_station) {
        app_log(app, "No station selected - use Select Station first.");
        app_render_log(app);
        return;
    }
    char cmd[FOX_STATION_MAC_MAX + 24];
    snprintf(cmd, sizeof(cmd), "WIFIATTACK:DEAUTH:%s", app->target_station_mac);
    char label[48];
    snprintf(label, sizeof(label), "Deauth (targeted: %s)", app->target_station_mac);
    run_attack(app, cmd, label);
}

void wifi_beacon_custom_submitted(App* app) {
    if(app->text_input_buffer[0] == '\0') {
        app_log(app, "No SSIDs entered.");
        app_render_log(app);
        return;
    }
    char cmd[FOX_TEXT_INPUT_BUFFER_MAX + 32];
    snprintf(cmd, sizeof(cmd), "WIFIATTACK:BEACON:%s", app->text_input_buffer);
    run_attack(app, cmd, "Beacon spam (custom)");
}


void wifi_menu_select(App* app, MenuContext ctx, uint32_t index) {
    switch(ctx) {
    case MenuContextWifi:
        switch((MenuWifiIndex)index) {
        case MenuWifiConnect:
            if(app->wifi_menu_connected) {
                app_switch_to_menu(app, MenuContextWifiConnection);
            } else {
                wifi_network_list_show(app, true);
            }
            break;
        case MenuWifiMyIp:
            app_log(app, "Fetching public IP...");
            app_render_log(app);
            esp_at_send(app->esp_at, "[WIFI/IP]");
            {
                EspAtMsg msg;
                if(esp_at_receive(app->esp_at, &msg, 10000)) {
                    app_log(app, "%s", msg.line);
                } else {
                    app_log(app, "No response.");
                }
            }
            app_render_log(app);
            break;
        case MenuWifiHttp:
            app_switch_to_menu(app, MenuContextWifiHttp);
            break;
        case MenuWifiRecon:
            app_switch_to_menu(app, MenuContextWifiRecon);
            break;
        case MenuWifiAttacks:
            app_switch_to_menu(app, MenuContextWifiAttacks);
            break;
        }
        break;

    case MenuContextWifiConnection:
        switch((MenuWifiConnectionIndex)index) {
        case MenuConnDisconnect:
            app_log(app, "Disconnecting...");
            app_render_log(app);
            esp_at_send(app->esp_at, "[WIFI/DISCONNECT]");
            {
                EspAtMsg msg;
                if(esp_at_receive(app->esp_at, &msg, 5000)) {
                    app_log(app, "%s", msg.line);
                } else {
                    app_log(app, "No response.");
                }
            }
            fox_wifi_status_write(false);
            app_render_log(app);
            app_switch_to_menu(app, MenuContextWifi);
            break;
        case MenuConnStatus: {
            app_log(app, "Checking status...");
            app_render_log(app);
            EspAtMsg msg;
            esp_at_send(app->esp_at, "[WIFI/SSID]");
            bool got_ssid = esp_at_receive(app->esp_at, &msg, 5000);
            if(got_ssid) {
                app_log(app, "%s", msg.line);
            } else {
                app_log(app, "No response.");
            }
            /* This is a genuine live check (unlike the status file's
               usual best-effort write-on-change elsewhere), so it's a
               good point to correct the flag if it ever drifted -
               [WIFI/SSID/SUCCESS] means connected, anything else
               (including no response) doesn't. */
            fox_wifi_status_write(
                got_ssid && strncmp(msg.line, "[WIFI/SSID/SUCCESS]", 19) == 0);
            esp_at_send(app->esp_at, "[IP/ADDRESS]");
            if(esp_at_receive(app->esp_at, &msg, 5000)) {
                app_log(app, "%s", msg.line);
            } else {
                app_log(app, "No response.");
            }
            app_render_log(app);
            break;
        }
        case MenuConnSwitch:
            wifi_network_list_show(app, true);
            break;
        case MenuConnForget: {
            app_log(app, "Looking up current network...");
            app_render_log(app);
            static const char* ssid_prefix = "[WIFI/SSID/SUCCESS]";
            size_t prefix_len = strlen(ssid_prefix);
            EspAtMsg msg;
            esp_at_send(app->esp_at, "[WIFI/SSID]");
            if(!esp_at_receive(app->esp_at, &msg, 5000) ||
               strncmp(msg.line, ssid_prefix, prefix_len) != 0) {
                app_log(app, "Not connected - nothing to forget.");
                app_render_log(app);
                break;
            }
            char ssid_esc[FOX_WIFI_SSID_MAX * 2];
            json_escape(msg.line + prefix_len, ssid_esc, sizeof(ssid_esc));
            char cmd[sizeof(ssid_esc) + 32];
            snprintf(cmd, sizeof(cmd), "[WIFI/FORGET]{\"ssid\":\"%s\"}", ssid_esc);
            esp_at_send(app->esp_at, cmd);
            if(esp_at_receive(app->esp_at, &msg, 5000)) {
                app_log(app, "%s", msg.line);
            } else {
                app_log(app, "No response.");
            }
            esp_at_send(app->esp_at, "[WIFI/DISCONNECT]");
            if(esp_at_receive(app->esp_at, &msg, 5000)) {
                app_log(app, "%s", msg.line);
            }
            fox_wifi_status_write(false);
            app_render_log(app);
            app_switch_to_menu(app, MenuContextWifi);
            break;
        }
        }
        break;

    case MenuContextWifiRecon:
        switch((MenuReconIndex)index) {
        case MenuReconScanAp:
            action_scan_aps(app);
            break;
        case MenuReconScanSta:
            action_scan_stas(app);
            break;
        case MenuReconSelectAp:
            wifi_network_list_show(app, false);
            break;
        case MenuReconSignalMonitor:
            action_signal_monitor(app);
            break;
        case MenuReconPacketCount:
            action_packet_count(app);
            break;
        case MenuReconWardrive:
            run_wardrive(app);
            break;
        case MenuReconPcap:
            run_pcap_capture(app);
            break;
        }
        break;

    case MenuContextWifiAttacks:
        switch((MenuAttackIndex)index) {
        case MenuAttackSelectAp:
            wifi_network_list_show(app, false);
            break;
        case MenuAttackSelectStation:
            station_list_show(app);
            break;
        case MenuAttackDeauthBroadcast:
            run_attack(app, "WIFIATTACK:DEAUTH", "Deauth (broadcast)");
            break;
        case MenuAttackDeauthTargeted:
            action_deauth_targeted(app);
            break;
        case MenuAttackBeaconRandom:
            run_attack(app, "WIFIATTACK:BEACON:RANDOM:8", "Beacon spam (random)");
            break;
        case MenuAttackBeaconCustom:
            app_show_text_input(app, "SSIDs (comma-sep)", TextInputPurposeBeaconSsids);
            break;
        case MenuAttackProbe:
            run_attack(app, "WIFIATTACK:PROBE", "Probe flood");
            break;
        }
        break;

    default:
        break;
    }
}


static App* s_network_list_app = NULL;

#define NETWORK_ROW_HEADER_H 14
#define NETWORK_ROW_H        22
#define NETWORK_ROW_VIS      2

static void network_list_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_network_list_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas,
        64,
        2,
        AlignCenter,
        AlignTop,
        app->network_list_for_connect ? "Connect" : "Select AP target");

    if(app->network_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "None found");
        return;
    }

    for(size_t i = app->network_scroll;
        i < app->network_count && (i - app->network_scroll) < NETWORK_ROW_VIS;
        i++) {
        int row = (int)(i - app->network_scroll);
        int ry = NETWORK_ROW_HEADER_H + row * NETWORK_ROW_H;
        int by = ry + 1;
        int bh = NETWORK_ROW_H - 2;
        bool selected = (i == app->network_selected);
        const FoxWifiNetwork* n = &app->networks[i];

        if(selected) {
            canvas_draw_rbox(canvas, 2, by, 124, bh, 3);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, 2, by, 124, bh, 3);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, by + 5, AlignCenter, AlignCenter, n->ssid);

        char line2[40];
        if(n->scan_index >= 0) {
            snprintf(
                line2,
                sizeof(line2),
                "%ddBm %s%s",
                n->rssi,
                n->secure ? "secure" : "open",
                n->saved ? " [saved]" : "");
        } else {
            snprintf(line2, sizeof(line2), "saved (out of range)");
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, by + 15, AlignCenter, AlignCenter, line2);

        canvas_set_color(canvas, ColorBlack);
    }

    if(app->network_count > NETWORK_ROW_VIS) {
        int available_h = 64 - NETWORK_ROW_HEADER_H;
        int bar_h = (int)(available_h * NETWORK_ROW_VIS / (int)app->network_count);
        if(bar_h < 3) bar_h = 3;
        int bar_y = NETWORK_ROW_HEADER_H +
                    (int)(available_h * (int)app->network_scroll / (int)app->network_count);
        canvas_draw_box(canvas, 125, bar_y, 3, bar_h);
    }
}

static bool network_list_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(app->network_count == 0) return false;

    switch(event->key) {
    case InputKeyUp:
        if(app->network_selected > 0) {
            app->network_selected--;
            if(app->network_selected < app->network_scroll) {
                app->network_scroll = app->network_selected;
            }
        } else {
            app->network_selected = app->network_count - 1;
            app->network_scroll = (app->network_count > NETWORK_ROW_VIS) ?
                                       app->network_count - NETWORK_ROW_VIS :
                                       0;
        }
        with_view_model(app->network_list_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        if(app->network_selected + 1 < app->network_count) {
            app->network_selected++;
            if(app->network_selected >= app->network_scroll + NETWORK_ROW_VIS) {
                app->network_scroll = app->network_selected - NETWORK_ROW_VIS + 1;
            }
        } else {
            app->network_selected = 0;
            app->network_scroll = 0;
        }
        with_view_model(app->network_list_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyOk:
    case InputKeyRight:
        network_select(app, app->network_selected);
        return true;
    case InputKeyBack:
    case InputKeyLeft:
        return false; /* navigation_callback sends this back to the menu */
    default:
        return false;
    }
}

View* wifi_network_list_view_alloc(App* app) {
    s_network_list_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, network_list_draw_cb);
    view_set_input_callback(view, network_list_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void wifi_network_list_view_free(View* view) {
    s_network_list_app = NULL;
    view_free(view);
}


static App* s_station_list_app = NULL;

#define STATION_ROW_HEADER_H 14
#define STATION_ROW_H        22
#define STATION_ROW_VIS      2

static void station_list_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_station_list_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Select Station");

    if(app->station_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "None found");
        return;
    }

    for(size_t i = app->station_scroll;
        i < app->station_count && (i - app->station_scroll) < STATION_ROW_VIS;
        i++) {
        int row = (int)(i - app->station_scroll);
        int ry = STATION_ROW_HEADER_H + row * STATION_ROW_H;
        int by = ry + 1;
        int bh = STATION_ROW_H - 2;
        bool selected = (i == app->station_selected);
        const FoxStation* s = &app->stations[i];

        if(selected) {
            canvas_draw_rbox(canvas, 2, by, 124, bh, 3);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, 2, by, 124, bh, 3);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, by + 5, AlignCenter, AlignCenter, s->mac);

        char line2[24];
        snprintf(line2, sizeof(line2), "%ddBm", s->rssi);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, by + 15, AlignCenter, AlignCenter, line2);

        canvas_set_color(canvas, ColorBlack);
    }

    if(app->station_count > STATION_ROW_VIS) {
        int available_h = 64 - STATION_ROW_HEADER_H;
        int bar_h = (int)(available_h * STATION_ROW_VIS / (int)app->station_count);
        if(bar_h < 3) bar_h = 3;
        int bar_y = STATION_ROW_HEADER_H +
                    (int)(available_h * (int)app->station_scroll / (int)app->station_count);
        canvas_draw_box(canvas, 125, bar_y, 3, bar_h);
    }
}

static bool station_list_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(app->station_count == 0) return false;

    switch(event->key) {
    case InputKeyUp:
        if(app->station_selected > 0) {
            app->station_selected--;
            if(app->station_selected < app->station_scroll) {
                app->station_scroll = app->station_selected;
            }
        } else {
            app->station_selected = app->station_count - 1;
            app->station_scroll = (app->station_count > STATION_ROW_VIS) ?
                                       app->station_count - STATION_ROW_VIS :
                                       0;
        }
        with_view_model(app->station_list_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        if(app->station_selected + 1 < app->station_count) {
            app->station_selected++;
            if(app->station_selected >= app->station_scroll + STATION_ROW_VIS) {
                app->station_scroll = app->station_selected - STATION_ROW_VIS + 1;
            }
        } else {
            app->station_selected = 0;
            app->station_scroll = 0;
        }
        with_view_model(app->station_list_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyOk:
    case InputKeyRight:
        station_select(app, app->station_selected);
        return true;
    case InputKeyBack:
    case InputKeyLeft:
        return false; /* navigation_callback sends this back to the menu */
    default:
        return false;
    }
}

View* wifi_station_list_view_alloc(App* app) {
    s_station_list_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, station_list_draw_cb);
    view_set_input_callback(view, station_list_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void wifi_station_list_view_free(View* view) {
    s_station_list_app = NULL;
    view_free(view);
}
