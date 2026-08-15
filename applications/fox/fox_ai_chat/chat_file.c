#include "chat_file.h"

#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>

#define CHAT_FILE_DIR  EXT_PATH("apps_data/fox_ai_chat")
#define CHAT_FILE_PATH EXT_PATH("apps_data/fox_ai_chat/chat.ai")

static void write_header_line(File* file) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char buf[64];
    int len = snprintf(
        buf,
        sizeof(buf),
        "FOX-AI: Chat Start Date: %04u-%02u-%02u %02u:%02u:%02u\n",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);
    if(len > 0) storage_file_write(file, buf, (size_t)len);
}

bool chat_file_exists(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool exists = storage_file_exists(storage, CHAT_FILE_PATH);
    furi_record_close(RECORD_STORAGE);
    return exists;
}

void chat_file_create_blank(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, CHAT_FILE_DIR);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, CHAT_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        write_header_line(file);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void chat_file_ensure_exists(void) {
    if(!chat_file_exists()) {
        chat_file_create_blank();
    }
}

bool chat_file_append_line(const char* line) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, CHAT_FILE_DIR);

    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, CHAT_FILE_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        size_t len = strlen(line);
        ok = storage_file_write(file, line, len) == len;
        storage_file_write(file, "\n", 1);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool chat_file_load(FuriString* out) {
    furi_string_reset(out);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, CHAT_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buf[257];
        uint16_t read_len;
        while((read_len = storage_file_read(file, buf, sizeof(buf) - 1)) > 0) {
            buf[read_len] = '\0';
            furi_string_cat_str(out, (const char*)buf);
        }
        ok = true;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(furi_string_size(out) > CHAT_FILE_DISPLAY_MAX_CHARS) {
        size_t excess = furi_string_size(out) - CHAT_FILE_DISPLAY_MAX_CHARS;
        size_t cut = furi_string_search_char(out, '\n', excess);
        cut = (cut == FURI_STRING_FAILURE) ? excess : (cut + 1);
        furi_string_right(out, cut);
    }

    /* trailing newline from the last appended line looks odd as a blank
     * final row in the terminal view */
    while(furi_string_size(out) > 0 &&
          furi_string_get_char(out, furi_string_size(out) - 1) == '\n') {
        furi_string_left(out, furi_string_size(out) - 1);
    }

    return ok;
}
