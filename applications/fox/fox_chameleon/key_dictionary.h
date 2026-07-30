#pragma once

#include <stdint.h>
#include <stddef.h>

#define KEY_DICTIONARY_MAX_KEYS 128
#define KEY_DICTIONARY_KEY_LEN  6

typedef struct {
    uint8_t keys[KEY_DICTIONARY_MAX_KEYS][KEY_DICTIONARY_KEY_LEN];
    size_t count;
} KeyDictionary;

size_t key_dictionary_load(KeyDictionary* dictionary);
