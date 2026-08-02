#include "github_release.h"
#include "json_mini.h"
#include "strutil.h"

#include <string.h>
#include <stdio.h>

static EspAtMsg s_esp_msg;

bool github_probe(EspAt* esp_at, uint32_t timeout_ms) {
    esp_at_send(esp_at, "[PING]");

    uint32_t start = furi_get_tick();
    while((furi_get_tick() - start) < timeout_ms) {
        if(esp_at_receive(esp_at, &s_esp_msg, 200)) {
            if(strstr(s_esp_msg.line, "PONG") != NULL) return true;
        }
    }
    return false;
}

bool github_release_check(
    EspAt* esp_at,
    const char* repo,
    bool need_commit,
    bool need_assets,
    ReleaseInfo* info,
    uint32_t timeout_ms,
    UpdaterApp* app) {
    memset(info, 0, sizeof(*info));

    char cmd[192];
    snprintf(
        cmd,
        sizeof(cmd),
        "[RELEASE/CHECK]{\"repo\":\"%s\",\"needCommit\":%s,\"needAssets\":%s}",
        repo,
        need_commit ? "true" : "false",
        need_assets ? "true" : "false");
    esp_at_send(esp_at, cmd);

    uint32_t start = furi_get_tick();
    bool got_header = false;

    while((furi_get_tick() - start) < timeout_ms) {
        if(!esp_at_receive(esp_at, &s_esp_msg, 300)) continue;
        const char* line = s_esp_msg.line;

        if(!got_header) {
            if(strncmp(line, "[ERROR]", 7) == 0) {
                const char* text = line + 7;
                while(*text == ' ') text++;
                info->ok = false;
                strncpy(info->error, text, sizeof(info->error) - 1);
                str_capitalize_first(info->error);
                return true;
            }
            if(strncmp(line, "[RELEASE/CHECK/SUCCESS]", 23) == 0) {
                const char* json = line + 23;
                json_mini_get_string(json, "tag", info->tag, sizeof(info->tag));
                json_mini_get_string(json, "commit", info->commit, sizeof(info->commit));
                info->ok = true;
                got_header = true;
                if(app) updater_set_check_stage(app, "Receiving...", 50);
            }
            continue;
        }

        if(strcmp(line, "[RELEASE/CHECK/END]") == 0) {
            return true;
        }
        if(line[0] == '{' && info->asset_count < UPDATER_ASSET_MAX) {
            ReleaseAsset* a = &info->assets[info->asset_count];
            json_mini_get_string(line, "name", a->name, sizeof(a->name));
            json_mini_get_string(line, "url", a->url, sizeof(a->url));
            uint32_t size = 0;
            json_mini_get_uint(line, "size", &size);
            a->size = size;
            info->asset_count++;
            if(app) {
                uint8_t pct = (uint8_t)(50 + info->asset_count * 8);
                if(pct > 89) pct = 89;
                updater_set_check_stage(app, "Receiving...", pct);
            }
        }
    }

    return false;
}
