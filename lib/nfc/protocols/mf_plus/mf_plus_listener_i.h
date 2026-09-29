#pragma once

#include "mf_plus_listener.h"

#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_listener_i.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MfPlusListenerStateIdle,
    MfPlusListenerStateAuthFirstDone,
    MfPlusListenerStateAuthenticated,
} MfPlusListenerState;

typedef struct {
    uint8_t k_enc[MF_PLUS_KEY_SIZE];
    uint8_t k_mac[MF_PLUS_KEY_SIZE];
    uint8_t ti[4];
    uint16_t r_ctr;
    uint16_t w_ctr;
} MfPlusListenerSession;

struct MfPlusListener {
    Iso14443_4aListener* iso14443_4a_listener;
    MfPlusData* data;

    MfPlusListenerState state;
    MfPlusKey auth_key;
    uint8_t rnd_b[MF_PLUS_KEY_SIZE];
    MfPlusListenerSession session;
    uint8_t get_version_stage;

    BitBuffer* tx_buffer;

    NfcGenericEvent generic_event;
    MfPlusListenerEvent mfp_event;
    MfPlusListenerEventData mfp_event_data;
    NfcGenericCallback callback;
    void* context;
};

NfcCommand mf_plus_listener_auth_first_handler(MfPlusListener* instance, const BitBuffer* rx);

NfcCommand mf_plus_listener_auth_continue_handler(MfPlusListener* instance, const BitBuffer* rx);

NfcCommand
    mf_plus_listener_read_handler(MfPlusListener* instance, const BitBuffer* rx, bool plain);

NfcCommand
    mf_plus_listener_write_handler(MfPlusListener* instance, const BitBuffer* rx, bool encrypted);

NfcCommand mf_plus_listener_read_signature_handler(MfPlusListener* instance, const BitBuffer* rx);

NfcCommand mf_plus_listener_write_perso_handler(MfPlusListener* instance, const BitBuffer* rx);

NfcCommand mf_plus_listener_unsupported_handler(MfPlusListener* instance, const BitBuffer* rx);

NfcCommand mf_plus_listener_get_version_handler(MfPlusListener* instance, bool wrapped);

NfcCommand mf_plus_listener_get_version_continue_handler(MfPlusListener* instance, bool wrapped);

void mf_plus_listener_reset_session(MfPlusListener* instance);

#ifdef __cplusplus
}
#endif
