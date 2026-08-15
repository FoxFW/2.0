#include "ai_chat_menu.h"
#include "chat_file.h"
#include "progress_view.h"
#include "message_limit_view.h"

#include <furi_hal_version.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    MenuAiPost,
    MenuAiRead,
    MenuAiClear,
} MenuAiIndex;

void ai_chat_render_menu(App* app) {
    chat_file_ensure_exists();

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Fox AI Chat");
    submenu_add_item(app->submenu, "Post Message to AI", MenuAiPost, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Read AI Chat", MenuAiRead, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Clear Chat", MenuAiClear, app_menu_item_callback, app);
}

void ai_chat_show_terminal(App* app) {
    chat_file_ensure_exists();
    chat_file_load(app->log);
    app->terminal_scroll = (size_t)-1;
    app->current_view = AiChatViewTerminal;
    view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewTerminal);
}

static void action_post_message(App* app) {
    if(app->esp_at == NULL) {
        app_show_text_input(app, "Message", TextInputPurposeChatMessage);
        return;
    }

    esp_at_send(app->esp_at, "AICOOLDOWN");

    EspAtMsg msg;
    uint32_t remaining = 0;
    if(esp_at_receive(app->esp_at, &msg, 3000) && strncmp(msg.line, "COOLDOWN:", 9) == 0) {
        remaining = (uint32_t)atoi(msg.line + 9);
    }

    if(remaining > 0) {
        message_limit_view_show(app, remaining);
    } else {
        app_show_text_input(app, "Message", TextInputPurposeChatMessage);
    }
}

void ai_chat_menu_select(App* app, uint32_t index) {
    switch((MenuAiIndex)index) {
    case MenuAiPost:
        action_post_message(app);
        break;
    case MenuAiRead:
        ai_chat_show_terminal(app);
        break;
    case MenuAiClear:
        chat_file_create_blank();
        ai_chat_show_terminal(app);
        break;
    }
}

static void unescape_json_newlines(FuriString* out, const char* src, size_t len) {
    for(size_t i = 0; i < len; i++) {
        if(src[i] == '\\' && i + 1 < len && src[i + 1] == 'n') {
            furi_string_push_back(out, '\n');
            i++;
        } else {
            furi_string_push_back(out, src[i]);
        }
    }
}

void chat_message_submitted(App* app) {
    if(app->text_input_buffer[0] == '\0') {
        app_switch_to_menu(app);
        return;
    }

    strncpy(app->saved_message, app->text_input_buffer, FOX_TEXT_INPUT_BUFFER_MAX - 1);
    app->saved_message[FOX_TEXT_INPUT_BUFFER_MAX - 1] = '\0';

    const char* device_name = furi_hal_version_get_name_ptr();
    if(device_name == NULL || device_name[0] == '\0') device_name = "Flipper";

    progress_view_show(app, ProgressStageSending);
    /* esp_at_send() below is just a UART write and returns almost
     * instantly - without this delay the "Sending Message..." stage
     * never gets a chance to actually render before we flip to
     * "Receiving Reply...". */
    furi_delay_ms(400);

    char cmd[FOX_TEXT_INPUT_BUFFER_MAX + 16];
    snprintf(cmd, sizeof(cmd), "AIASK:%s", app->text_input_buffer);
    esp_at_send(app->esp_at, cmd);

    progress_view_set_stage(app, ProgressStageReceiving);

    EspAtMsg msg;
    bool ok = false;
    FuriString* reply = furi_string_alloc();

    if(!esp_at_receive(app->esp_at, &msg, 25000)) {
        app_log(app, "No response.");
    } else if(strncmp(msg.line, "AIREPLY:", 8) == 0) {
        furi_string_set(reply, msg.line + 8);
        ok = true;
    } else if(strcmp(msg.line, "ERROR:NOWIFI") == 0) {
        app_log(app, "WiFi not connected.");
    } else if(strncmp(msg.line, "ERROR:RATELIMIT", 15) == 0) {
        app_log(app, "Message limit - please wait.");
    } else if(strcmp(msg.line, "ERROR:BLOCKED") == 0) {
        app_log(app, "Blocked by Gemini safety filter.");
    } else if(strcmp(msg.line, "ERROR:NOREPLY") == 0) {
        app_log(app, "No reply text in response.");
    } else if(strncmp(msg.line, "ERROR:HTTP:", 11) == 0) {
        app_log(app, "HTTP error: %s", msg.line + 11);
    } else {
        app_log(app, "%s", msg.line);
    }

    if(ok) {
        char user_line[FOX_TEXT_INPUT_BUFFER_MAX + 64];
        snprintf(user_line, sizeof(user_line), "%s: %s", device_name, app->saved_message);
        chat_file_append_line(user_line);

        FuriString* unescaped = furi_string_alloc();
        unescape_json_newlines(unescaped, furi_string_get_cstr(reply), furi_string_size(reply));

        FuriString* ai_line = furi_string_alloc();
        furi_string_printf(ai_line, "FOX-AI: %s", furi_string_get_cstr(unescaped));
        chat_file_append_line(furi_string_get_cstr(ai_line));

        furi_string_free(ai_line);
        furi_string_free(unescaped);

        app->text_input_buffer[0] = '\0';
        app->saved_message[0] = '\0';
        ai_chat_show_terminal(app);
    } else {
        app_render_log(app);
    }

    furi_string_free(reply);
}
