#include "http_menu.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    MenuHttpGet,
    MenuHttpPost,
} MenuHttpIndex;

void http_render_menu(App* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "HTTP Request");
    submenu_add_item(app->submenu, "GET", MenuHttpGet, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "POST", MenuHttpPost, app_menu_item_callback, app);
}

void http_menu_select(App* app, uint32_t index) {
    switch((MenuHttpIndex)index) {
    case MenuHttpGet:
        app_show_text_input(app, "URL", TextInputPurposeHttpGetUrl);
        break;
    case MenuHttpPost:
        app_show_text_input(app, "URL", TextInputPurposeHttpPostUrl);
        break;
    }
}

static void http_drain_and_log(App* app, const char* end_tag, uint32_t first_timeout_ms) {
    EspAtMsg msg;
    if(!esp_at_receive(app->esp_at, &msg, first_timeout_ms)) {
        app_log(app, "No response.");
        app_render_log(app);
        return;
    }

    if(strncmp(msg.line, "[ERROR]", 7) == 0) {
        app_log(app, "%s", msg.line);
        app_render_log(app);
        return;
    }

    if(strstr(msg.line, "/SUCCESS]{") != NULL) {
        app_log(app, "%s", msg.line);
        if(!esp_at_receive(app->esp_at, &msg, 3000)) {
            app_render_log(app);
            return;
        }
    }

    FuriString* full = furi_string_alloc();
    bool done = false;
    for(size_t guard = 0; !done && guard < 200; guard++) {
        if(strcmp(msg.line, end_tag) == 0) {
            done = true;
            break;
        }
        if(furi_string_size(full) > 0) furi_string_cat(full, "\n");
        furi_string_cat(full, msg.line);

        if(!esp_at_receive(app->esp_at, &msg, 3000)) break;
    }

    if(furi_string_size(full) > 0) {
        app_log_raw(app, furi_string_get_cstr(full));
    } else {
        app_log(app, "(empty response)");
    }
    furi_string_free(full);
    app_render_log(app);
}

void http_get_url_submitted(App* app) {
    if(app->text_input_buffer[0] == '\0') {
        app_log(app, "No URL entered.");
        app_render_log(app);
        return;
    }

    char cmd[FOX_TEXT_INPUT_BUFFER_MAX + 8];
    snprintf(cmd, sizeof(cmd), "[GET]%s", app->text_input_buffer);
    esp_at_send(app->esp_at, cmd);

    app_log(app, "GET %s...", app->text_input_buffer);
    app_render_log(app);
    http_drain_and_log(app, "[GET/END]", 10000);
}

void http_post_url_submitted(App* app) {
    if(app->text_input_buffer[0] == '\0') {
        app_log(app, "No URL entered.");
        app_render_log(app);
        return;
    }
    furi_string_set(app->pending_http_url, app->text_input_buffer);
    app_show_text_input(app, "Body", TextInputPurposeHttpPostBody);
}

void http_post_body_submitted(App* app) {
    const char* url = furi_string_get_cstr(app->pending_http_url);
    const char* body = app->text_input_buffer;

    char cmd[FOX_TEXT_INPUT_BUFFER_MAX * 2 + 48];
    snprintf(cmd, sizeof(cmd), "[POST/HTTP]{\"url\":\"%s\",\"payload\":\"%s\"}", url, body);
    esp_at_send(app->esp_at, cmd);

    app_log(app, "POST %s...", url);
    app_render_log(app);
    http_drain_and_log(app, "[POST/END]", 10000);
}
