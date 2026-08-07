#include "foxchat_menu.h"
#include "chat_list_view.h"

#include <furi_hal_version.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef enum {
    MenuChatPost,
    MenuChatRead,
} MenuChatIndex;

static bool chat_fetch_messages(App* app) {
    bool line_protocol_error = false;
    char cmd[24];
    snprintf(cmd, sizeof(cmd), "DISCORDREAD:%d", FOX_CHAT_MESSAGE_MAX);
    esp_at_send(app->esp_at, cmd);

    app->chat_message_count = 0;
    for(;;) {
        EspAtMsg msg;
        if(!esp_at_receive(app->esp_at, &msg, 10000)) {
            app_log(app, "No response.");
            line_protocol_error = true;
            break;
        }
        if(strcmp(msg.line, "DISCORDREADDONE") == 0) {
            break;
        }
        if(strncmp(msg.line, "ERROR:", 6) == 0) {
            app_log(app, "%s", msg.line);
            line_protocol_error = true;
            break;
        }
        if(strncmp(msg.line, "DISCORDMSG:", 11) == 0 &&
           app->chat_message_count < FOX_CHAT_MESSAGE_MAX) {
            const char* rest = msg.line + 11;
            const char* pipe1 = strchr(rest, '|');
            const char* pipe2 = pipe1 ? strchr(pipe1 + 1, '|') : NULL;
            ChatMessage* cm = &app->chat_messages[app->chat_message_count];
            if(pipe1 != NULL && pipe2 != NULL) {
                // "<time>|<full_time>|<text>" - current protocol
                size_t tlen = (size_t)(pipe1 - rest);
                if(tlen > sizeof(cm->time) - 1) tlen = sizeof(cm->time) - 1;
                memcpy(cm->time, rest, tlen);
                cm->time[tlen] = '\0';

                size_t flen = (size_t)(pipe2 - (pipe1 + 1));
                if(flen > sizeof(cm->full_time) - 1) flen = sizeof(cm->full_time) - 1;
                memcpy(cm->full_time, pipe1 + 1, flen);
                cm->full_time[flen] = '\0';

                strncpy(cm->text, pipe2 + 1, sizeof(cm->text) - 1);
                cm->text[sizeof(cm->text) - 1] = '\0';
            } else if(pipe1 != NULL) {
                // "<time>|<text>" - older ESP32 firmware without full_time
                size_t tlen = (size_t)(pipe1 - rest);
                if(tlen > sizeof(cm->time) - 1) tlen = sizeof(cm->time) - 1;
                memcpy(cm->time, rest, tlen);
                cm->time[tlen] = '\0';
                strncpy(cm->full_time, cm->time, sizeof(cm->full_time) - 1);
                cm->full_time[sizeof(cm->full_time) - 1] = '\0';
                strncpy(cm->text, pipe1 + 1, sizeof(cm->text) - 1);
                cm->text[sizeof(cm->text) - 1] = '\0';
            } else {
                strncpy(cm->time, "--:--", sizeof(cm->time) - 1);
                cm->time[sizeof(cm->time) - 1] = '\0';
                strncpy(cm->full_time, "--:--", sizeof(cm->full_time) - 1);
                cm->full_time[sizeof(cm->full_time) - 1] = '\0';
                strncpy(cm->text, rest, sizeof(cm->text) - 1);
                cm->text[sizeof(cm->text) - 1] = '\0';
            }
            app->chat_message_count++;
            continue;
        }
    }
    return !line_protocol_error;
}

void foxchat_render_menu(App* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Fox Chat");
    submenu_add_item(app->submenu, "Post Message", MenuChatPost, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Read Messages", MenuChatRead, app_menu_item_callback, app);
}

void foxchat_menu_select(App* app, uint32_t index) {
    switch((MenuChatIndex)index) {
    case MenuChatPost:
        app_show_text_input(app, "Message", TextInputPurposeChatMessage);
        break;
    case MenuChatRead: {
        app_log(app, "Reading messages...");
        app_render_log(app);

        if(chat_fetch_messages(app)) {
            chat_list_view_show(app);
        } else {
            app_render_log(app);
        }
        break;
    }
    }
}

void chat_message_submitted(App* app) {
    if(app->text_input_buffer[0] == '\0') {
        app_log(app, "No message entered.");
        app_render_log(app);
        return;
    }

    strncpy(app->saved_message, app->text_input_buffer, FOX_TEXT_INPUT_BUFFER_MAX - 1);
    app->saved_message[FOX_TEXT_INPUT_BUFFER_MAX - 1] = '\0';

    const char* device_name = furi_hal_version_get_name_ptr();
    if(device_name == NULL || device_name[0] == '\0') device_name = "Flipper";

    char cmd[FOX_TEXT_INPUT_BUFFER_MAX + 64];
    snprintf(cmd, sizeof(cmd), "DISCORDPOST:%s: %s", device_name, app->text_input_buffer);

    app_log(app, "Posting...");
    app_render_log(app);
    bool posted = false;
    esp_at_send(app->esp_at, cmd);
    EspAtMsg msg;
    if(!esp_at_receive(app->esp_at, &msg, 10000)) {
        app_log(app, "No response.");
    } else if(strcmp(msg.line, "ERROR:PROFANITY") == 0) {
        app_log(app, "Blocked - message flagged by content filter.");
    } else if(strcmp(msg.line, "ERROR:RATELIMIT") == 0) {
        app_log(app, "Too soon - wait a few seconds and try again.");
    } else if(strcmp(msg.line, "OK") == 0) {
        posted = true;
    } else {
        app_log(app, "%s", msg.line);
    }

    if(posted && chat_fetch_messages(app)) {
        app->saved_message[0] = '\0';
        chat_list_view_show(app);
    } else {
        strncpy(app->text_input_buffer, app->saved_message, FOX_TEXT_INPUT_BUFFER_MAX - 1);
        app->text_input_buffer[FOX_TEXT_INPUT_BUFFER_MAX - 1] = '\0';
        app_show_text_input_restore(app, "Message", TextInputPurposeChatMessage);
    }
}
