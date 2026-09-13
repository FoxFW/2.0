#include "foxchat_menu.h"
#include "chat_list_view.h"
#include "chat_save_view.h"

#include <furi_hal_version.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef enum {
    MenuChatPost,
    MenuChatRead,
    MenuChatSave,
} MenuChatIndex;

#define CHAT_SAVE_DIR EXT_PATH("apps_data/fox_chat")

static void chat_format_save_timestamp(const ChatMessage* cm, char* out, size_t out_size) {
    const char* traw = cm->full_time;
    bool has_tag = (traw[0] == 'L' || traw[0] == 'Z');
    const char* tdisp = has_tag ? traw + 1 : traw;

    int day = 0, year = 0, hour = 0, minute = 0;
    char mon_abbr[8] = {0};
    if(sscanf(tdisp, "%d %3s %d, %d:%d", &day, mon_abbr, &year, &hour, &minute) != 5) {
        snprintf(out, out_size, "[%s]", tdisp);
        return;
    }

    static const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int mon = 1;
    for(int i = 0; i < 12; i++) {
        if(mon_abbr[0] == months[i][0] && mon_abbr[1] == months[i][1] &&
           mon_abbr[2] == months[i][2]) {
            mon = i + 1;
            break;
        }
    }

    int hour12 = hour % 12;
    if(hour12 == 0) hour12 = 12;
    const char* ampm = (hour < 12) ? "AM" : "PM";

    snprintf(out, out_size, "[%02d/%02d/%04d %d:%02d%s]", day, mon, year, hour12, minute, ampm);
}

static bool chat_write_save_file(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, CHAT_SAVE_DIR);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char filename[40];
    snprintf(
        filename,
        sizeof(filename),
        "FoxChat_%04u%02u%02u_%02u%02u%02u.txt",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);

    char path[96];
    snprintf(path, sizeof(path), "%s/%s", CHAT_SAVE_DIR, filename);

    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    for(size_t i = 0; ok && i < app->chat_message_count; i++) {
        const ChatMessage* cm = &app->chat_messages[i];
        char ts[24];
        chat_format_save_timestamp(cm, ts, sizeof(ts));

        char line[24 + FOX_CHAT_MESSAGE_TEXT_MAX + 4];
        int len = snprintf(line, sizeof(line), "%s %s\n", ts, cm->text);
        if(len <= 0) continue;
        size_t wlen = (size_t)len < sizeof(line) ? (size_t)len : sizeof(line) - 1;
        if(storage_file_write(file, line, wlen) != wlen) ok = false;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

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
    submenu_add_item(app->submenu, "Save Chat", MenuChatSave, app_menu_item_callback, app);
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
    case MenuChatSave: {
        app_log(app, "Saving chat...");
        app_render_log(app);

        if(chat_fetch_messages(app)) {
            if(chat_write_save_file(app)) {
                chat_save_result_view_show(app, true, "Chat saved to", "SD Card!");
            } else {
                chat_save_result_view_show(app, false, "Save Failed", "Check SD card");
            }
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
