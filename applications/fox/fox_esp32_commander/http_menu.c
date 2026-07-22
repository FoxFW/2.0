#include "http_menu.h"

#include <stdio.h>

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
    EspAtMsg msg;
    if(esp_at_receive(app->esp_at, &msg, 10000)) {
        app_log(app, "%s", msg.line);
    } else {
        app_log(app, "No response.");
    }
    app_render_log(app);
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
    EspAtMsg msg;
    if(esp_at_receive(app->esp_at, &msg, 10000)) {
        app_log(app, "%s", msg.line);
    } else {
        app_log(app, "No response.");
    }
    app_render_log(app);
}
