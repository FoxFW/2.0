#pragma once

#include <stdint.h>
#include <stddef.h>

#define KEY_DICTIONARY_MAX_KEYS 128
#define KEY_DICTIONARY_KEY_LEN  6

typedef struct {
    uint8_t keys[KEY_DICTIONARY_MAX_KEYS][KEY_DICTIONARY_KEY_LEN];
    size_t count;
} KeyDictionary;

/* Reads keys from the Flipper's own NFC app dictionaries -
   /ext/nfc/assets/mf_classic_dict.nfc and, if present,
   mf_classic_dict_user.nfc - the same files the built-in NFC app's
   Mifare Classic key recovery uses, rather than maintaining a separate
   dictionary file for this app. Confirmed against the real file format
   in flipperdevices/flipperzero-firmware: comment lines start with '#',
   blank lines are skipped, and a key line is exactly 12 hex characters.
   Only the first KEY_DICTIONARY_READ_CHUNK bytes of each file are read
   (see key_dictionary.c) and at most KEY_DICTIONARY_MAX_KEYS keys are
   kept in total, even if the files hold more - see README.md. Returns
   the number of keys loaded. */
size_t key_dictionary_load(KeyDictionary* dictionary);
