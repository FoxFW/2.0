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

static const uint8_t foxchat_seed_a[72] = {
    217, 178, 75,  92,  35,  252, 224, 91,  44,  158, 100, 57,  210, 56,  248, 199,
    83,  0,   189, 137, 108, 4,   4,   196, 26,  19,  54,  111, 247, 21,  213, 83,
    113, 214, 159, 10,  24,  116, 114, 243, 104, 104, 147, 17,  95,  175, 43,  248,
    102, 29,  101, 79,  255, 64,  176, 20,  207, 157, 117, 109, 197, 2,   199, 94,
    73,  42,  23,  247, 122, 75,  42,  41};
static const uint8_t foxchat_seed_b[19] = {
    165, 211, 44, 29, 95, 176, 155, 87, 81, 239, 17, 120, 172, 74, 170, 207, 42, 125, 222};

static void foxchat_seed_decode(const uint8_t* in, size_t len, char* out) {
    uint32_t s = 0x2F6B19A7u;
    for(size_t i = 0; i < len; i++) {
        s = s * 1103515245u + 12345u;
        out[i] = (char)(in[i] ^ (uint8_t)(s >> 16));
    }
    out[len] = '\0';
}

static bool foxchat_auto_provision(App* app) {
    char token[sizeof(foxchat_seed_a) + 1];
    char channel[sizeof(foxchat_seed_b) + 1];
    foxchat_seed_decode(foxchat_seed_a, sizeof(foxchat_seed_a), token);
    foxchat_seed_decode(foxchat_seed_b, sizeof(foxchat_seed_b), channel);

    char cmd[sizeof(token) + sizeof(channel) + 24];
    snprintf(cmd, sizeof(cmd), "DISCORDINIT:%s:%s", token, channel);
    esp_at_send(app->esp_at, cmd);

    EspAtMsg msg;
    bool ok = esp_at_receive(app->esp_at, &msg, 10000) && strcmp(msg.line, "OK") == 0;

    memset(token, 0, sizeof(token));
    memset(channel, 0, sizeof(channel));
    memset(cmd, 0, sizeof(cmd));
    return ok;
}

static bool chat_fetch_messages(App* app) {
    bool line_protocol_error = false;
    for(int attempt = 0; attempt < 2; attempt++) {
        char cmd[24];
        snprintf(cmd, sizeof(cmd), "DISCORDREAD:%d", FOX_CHAT_MESSAGE_MAX);
        esp_at_send(app->esp_at, cmd);

        app->chat_message_count = 0;
        bool notinit = false;
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
            if(attempt == 0 && strcmp(msg.line, "ERROR:NOTINIT") == 0) {
                notinit = true;
                break;
            }
            if(strncmp(msg.line, "ERROR:", 6) == 0) {
                app_log(app, "%s", msg.line);
                line_protocol_error = true;
                break;
            }
            if(strncmp(msg.line, "DISCORDMSG:", 11) == 0 &&
               app->chat_message_count < FOX_CHAT_MESSAGE_MAX) {
                /* "<HH:MM>|<content>" - split on the first '|' (see
                   discord.cpp's doRead() for why that delimiter, not
                   ':', is safe here). content is usually itself
                   "<device_name>: <text>" for anything posted through
                   this bridge (see chat_message_submitted() below) -
                   chat_list_view.c/chat_detail_view.c bold that embedded
                   name when rendering it, rather than this function
                   needing to split it out here too. */
                const char* rest = msg.line + 11;
                const char* pipe = strchr(rest, '|');
                ChatMessage* cm = &app->chat_messages[app->chat_message_count];
                if(pipe != NULL) {
                    size_t tlen = (size_t)(pipe - rest);
                    if(tlen > sizeof(cm->time) - 1) tlen = sizeof(cm->time) - 1;
                    memcpy(cm->time, rest, tlen);
                    cm->time[tlen] = '\0';
                    strncpy(cm->text, pipe + 1, sizeof(cm->text) - 1);
                    cm->text[sizeof(cm->text) - 1] = '\0';
                } else {
                    /* Shouldn't happen with current firmware - tolerate
                       it rather than dropping the message. */
                    strncpy(cm->time, "--:--", sizeof(cm->time) - 1);
                    cm->time[sizeof(cm->time) - 1] = '\0';
                    strncpy(cm->text, rest, sizeof(cm->text) - 1);
                    cm->text[sizeof(cm->text) - 1] = '\0';
                }
                app->chat_message_count++;
                continue;
            }
            /* Unrecognized line - ignore and keep reading, same
               tolerant behavior as before this rewrite. */
        }
        if(notinit && foxchat_auto_provision(app)) continue;
        break;
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

    /* Save the typed message before attempting the send.  If the send
       fails for any reason (no response, rate limit, etc.) we restore
       it into text_input_buffer and jump back to the text input so the
       user doesn't have to retype their (possibly long) message. */
    strncpy(app->saved_message, app->text_input_buffer, FOX_TEXT_INPUT_BUFFER_MAX - 1);
    app->saved_message[FOX_TEXT_INPUT_BUFFER_MAX - 1] = '\0';

    const char* device_name = furi_hal_version_get_name_ptr();
    if(device_name == NULL || device_name[0] == '\0') device_name = "Flipper";

    char cmd[FOX_TEXT_INPUT_BUFFER_MAX + 64];
    snprintf(cmd, sizeof(cmd), "DISCORDPOST:%s: %s", device_name, app->text_input_buffer);

    app_log(app, "Posting...");
    app_render_log(app);
    bool posted = false;
    for(int attempt = 0; attempt < 2; attempt++) {
        esp_at_send(app->esp_at, cmd);
        EspAtMsg msg;
        if(!esp_at_receive(app->esp_at, &msg, 10000)) {
            app_log(app, "No response.");
            break;
        }
        if(attempt == 0 && strcmp(msg.line, "ERROR:NOTINIT") == 0 && foxchat_auto_provision(app)) {
            continue;
        }
        if(strcmp(msg.line, "ERROR:PROFANITY") == 0) {
            app_log(app, "Blocked - message flagged by content filter.");
        } else if(strcmp(msg.line, "ERROR:RATELIMIT") == 0) {
            app_log(app, "Too soon - wait a few seconds and try again.");
        } else if(strcmp(msg.line, "OK") == 0) {
            posted = true;
        } else {
            app_log(app, "%s", msg.line);
        }
        break;
    }

    if(posted && chat_fetch_messages(app)) {
        app->saved_message[0] = '\0'; /* clear saved message on success */
        chat_list_view_show(app);
    } else {
        strncpy(app->text_input_buffer, app->saved_message, FOX_TEXT_INPUT_BUFFER_MAX - 1);
        app->text_input_buffer[FOX_TEXT_INPUT_BUFFER_MAX - 1] = '\0';
        app_show_text_input_restore(app, "Message", TextInputPurposeChatMessage);
    }
}
