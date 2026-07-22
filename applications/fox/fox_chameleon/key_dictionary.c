#include "key_dictionary.h"

#include <furi.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define KEY_DICTIONARY_PRIMARY_PATH "/ext/nfc/assets/mf_classic_dict.nfc"
#define KEY_DICTIONARY_USER_PATH    "/ext/nfc/assets/mf_classic_dict_user.nfc"
#define KEY_DICTIONARY_READ_CHUNK   4096

static bool parse_key_line(const char* line, uint8_t* key) {
    if(strlen(line) != KEY_DICTIONARY_KEY_LEN * 2) return false;

    for(int i = 0; i < KEY_DICTIONARY_KEY_LEN; i++) {
        unsigned byte_value = 0;
        if(sscanf(line + i * 2, "%2x", &byte_value) != 1) return false;
        key[i] = (uint8_t)byte_value;
    }
    return true;
}

/* Reads only the first KEY_DICTIONARY_READ_CHUNK bytes of the file, not
   the whole thing - community dictionaries can run to hundreds of KB,
   and this app has no need to hold all of that in RAM at once. Any
   partial line cut off at the chunk boundary is simply not 12 hex
   characters and is safely rejected by parse_key_line(). */
static size_t load_file(Storage* storage, const char* path, KeyDictionary* dictionary) {
    File* file = storage_file_alloc(storage);
    size_t loaded = 0;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buffer[KEY_DICTIONARY_READ_CHUNK + 1];
        uint16_t read = storage_file_read(file, buffer, KEY_DICTIONARY_READ_CHUNK);
        buffer[read] = '\0';

        char* line = strtok(buffer, "\r\n");
        while(line != NULL && dictionary->count < KEY_DICTIONARY_MAX_KEYS) {
            if(line[0] != '#' && line[0] != '\0') {
                uint8_t key[KEY_DICTIONARY_KEY_LEN];
                if(parse_key_line(line, key)) {
                    memcpy(dictionary->keys[dictionary->count], key, KEY_DICTIONARY_KEY_LEN);
                    dictionary->count++;
                    loaded++;
                }
            }
            line = strtok(NULL, "\r\n");
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    return loaded;
}

size_t key_dictionary_load(KeyDictionary* dictionary) {
    dictionary->count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    size_t total = load_file(storage, KEY_DICTIONARY_PRIMARY_PATH, dictionary);
    total += load_file(storage, KEY_DICTIONARY_USER_PATH, dictionary);
    furi_record_close(RECORD_STORAGE);

    return total;
}
