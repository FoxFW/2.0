#include "mf_plus_poller_i.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>

#include "mf_plus_i.h"
#include "mf_plus_crypto.h"

#define TAG "MfPlusPoller"

MfPlusError mf_plus_process_error(Iso14443_4aError error) {
    switch(error) {
    case Iso14443_4aErrorNone:
        return MfPlusErrorNone;
    case Iso14443_4aErrorNotPresent:
        return MfPlusErrorNotPresent;
    case Iso14443_4aErrorTimeout:
        return MfPlusErrorTimeout;
    default:
        return MfPlusErrorProtocol;
    }
}

MfPlusError mf_plus_process_status_code(uint8_t status_code) {
    switch(status_code) {
    case NXP_NATIVE_COMMAND_STATUS_OPERATION_OK:
        return MfPlusErrorNone;
    default:
        return MfPlusErrorProtocol;
    }
}

MfPlusError mf_plus_poller_send_chunks(
    MfPlusPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer) {
    furi_assert(instance);

    NxpNativeCommandStatus status_code = NXP_NATIVE_COMMAND_STATUS_OPERATION_OK;
    Iso14443_4aError iso14443_4a_error = nxp_native_command_iso14443_4a_poller(
        instance->iso14443_4a_poller,
        &status_code,
        tx_buffer,
        rx_buffer,
        NxpNativeCommandModePlain,
        instance->tx_buffer,
        instance->rx_buffer);

    if(iso14443_4a_error != Iso14443_4aErrorNone) {
        return mf_plus_process_error(iso14443_4a_error);
    }

    return mf_plus_process_status_code(status_code);
}

#define MF_PLUS_CMD_WRITE_PERSO       (0xA8)
#define MF_PLUS_WRITE_PERSO_PROBE_LEN (1 + 2 + 16)

MfPlusError
    mf_plus_poller_probe_security_level(MfPlusPoller* instance, MfPlusProbeResult* result) {
    furi_check(instance);
    furi_check(result);

    bit_buffer_reset(instance->input_buffer);
    bit_buffer_append_byte(instance->input_buffer, MF_PLUS_CMD_WRITE_PERSO);
    bit_buffer_append_byte(instance->input_buffer, 0x90);
    bit_buffer_append_byte(instance->input_buffer, 0x90);
    for(size_t i = 0; i < MF_PLUS_WRITE_PERSO_PROBE_LEN - 3; i++) {
        bit_buffer_append_byte(instance->input_buffer, 0x00);
    }

    NxpNativeCommandStatus status = NXP_NATIVE_COMMAND_STATUS_OPERATION_OK;
    Iso14443_4aError error = nxp_native_command_iso14443_4a_poller(
        instance->iso14443_4a_poller,
        &status,
        instance->input_buffer,
        instance->result_buffer,
        NxpNativeCommandModePlain,
        instance->tx_buffer,
        instance->rx_buffer);
    if(error != Iso14443_4aErrorNone) {
        return mf_plus_process_error(error);
    }

    FURI_LOG_D(TAG, "SL probe (SAK 20) WritePerso status 0x%02X", status);
    switch(status) {
    case 0x09:
        *result = MfPlusProbeResultSl0;
        break;
    case 0x06:
    case 0x0B:
        *result = MfPlusProbeResultSl3;
        break;
    default:
        *result = MfPlusProbeResultNotPlus;
        break;
    }
    return MfPlusErrorNone;
}

MfPlusError mf_plus_poller_read_version(MfPlusPoller* instance, MfPlusVersion* data) {
    furi_check(instance);

    bit_buffer_reset(instance->input_buffer);
    bit_buffer_append_byte(instance->input_buffer, MF_PLUS_CMD_GET_VERSION);

    bit_buffer_reset(instance->result_buffer);

    MfPlusError error =
        mf_plus_poller_send_chunks(instance, instance->input_buffer, instance->result_buffer);

    if(mf_plus_version_parse(data, instance->result_buffer) == MfPlusErrorNone &&
       data->hw_vendor == 0x04 && (data->hw_type & 0x0F) == 0x02) {
        return MfPlusErrorNone;
    }

    return (error != MfPlusErrorNone) ? error : MfPlusErrorProtocol;
}

#define MF_PLUS_CMD_AUTH_FIRST    (0x70)
#define MF_PLUS_CMD_AUTH_CONTINUE (0x72)
#define MF_PLUS_CMD_READ_ENC      (0x31)
#define MF_PLUS_CMD_READ_PLAIN    (0x33)
#define MF_PLUS_CMD_READ_SIG      (0x3C)
#define MF_PLUS_CMD_WRITE_ENC     (0xA1)
#define MF_PLUS_CMD_WRITE_PLAIN   (0xA3)
#define MF_PLUS_STATUS_OK         (0x90)

static MfPlusError mf_plus_poller_send_raw(
    MfPlusPoller* instance,
    const uint8_t* cmd,
    size_t cmd_len,
    uint8_t* resp,
    size_t* resp_len,
    size_t resp_capacity) {
    bit_buffer_reset(instance->input_buffer);
    bit_buffer_copy_bytes(instance->input_buffer, cmd, cmd_len);
    bit_buffer_reset(instance->result_buffer);

    Iso14443_4aError error = iso14443_4a_poller_send_block(
        instance->iso14443_4a_poller, instance->input_buffer, instance->result_buffer);
    if(error != Iso14443_4aErrorNone) {
        return mf_plus_process_error(error);
    }

    size_t len = bit_buffer_get_size_bytes(instance->result_buffer);
    if(len > resp_capacity) {
        FURI_LOG_W(TAG, "Truncating %zu-byte response to %zu", len, resp_capacity);
        len = resp_capacity;
    }
    memcpy(resp, bit_buffer_get_data(instance->result_buffer), len);
    *resp_len = len;
    return MfPlusErrorNone;
}

MfPlusError mf_plus_poller_authenticate_key_id(
    MfPlusPoller* instance,
    uint16_t key_id,
    const MfPlusKey* key,
    MfPlusPollerSession* session) {
    furi_check(instance);
    furi_check(key);
    furi_check(session);

    const uint8_t cmd1[4] = {
        MF_PLUS_CMD_AUTH_FIRST, (uint8_t)(key_id & 0xFF), (uint8_t)(key_id >> 8), 0x00};
    uint8_t resp[64];
    size_t resp_len = 0;
    MfPlusError error =
        mf_plus_poller_send_raw(instance, cmd1, sizeof(cmd1), resp, &resp_len, sizeof(resp));
    if(error != MfPlusErrorNone) return error;
    if(resp_len < 1 + MF_PLUS_AES_BLOCK_SIZE || resp[0] != MF_PLUS_STATUS_OK) {
        return MfPlusErrorProtocol;
    }

    uint8_t rnd_b[MF_PLUS_AES_BLOCK_SIZE];
    mf_plus_crypto_ecb_decrypt(key->data, &resp[1], rnd_b);

    uint8_t rnd_a[MF_PLUS_AES_BLOCK_SIZE];
    furi_hal_random_fill_buf(rnd_a, sizeof(rnd_a));
    uint8_t rnd_a_rrot[MF_PLUS_AES_BLOCK_SIZE];
    rnd_a_rrot[0] = rnd_a[MF_PLUS_AES_BLOCK_SIZE - 1];
    memcpy(&rnd_a_rrot[1], rnd_a, MF_PLUS_AES_BLOCK_SIZE - 1);

    uint8_t rnd_b_rot[MF_PLUS_AES_BLOCK_SIZE];
    mf_plus_crypto_rotate_left(rnd_b, rnd_b_rot);

    uint8_t plain[2 * MF_PLUS_AES_BLOCK_SIZE];
    memcpy(&plain[0], rnd_a_rrot, MF_PLUS_AES_BLOCK_SIZE);
    memcpy(&plain[MF_PLUS_AES_BLOCK_SIZE], rnd_b_rot, MF_PLUS_AES_BLOCK_SIZE);

    const uint8_t zero_iv[MF_PLUS_AES_BLOCK_SIZE] = {0};
    uint8_t cmd2[1 + 2 * MF_PLUS_AES_BLOCK_SIZE];
    cmd2[0] = MF_PLUS_CMD_AUTH_CONTINUE;
    mf_plus_crypto_cbc_encrypt(key->data, zero_iv, plain, &cmd2[1], sizeof(plain));

    error = mf_plus_poller_send_raw(instance, cmd2, sizeof(cmd2), resp, &resp_len, sizeof(resp));
    if(error != MfPlusErrorNone) return error;
    if(resp_len < 1 + 2 * MF_PLUS_AES_BLOCK_SIZE || resp[0] != MF_PLUS_STATUS_OK) {
        return MfPlusErrorAuth;
    }

    uint8_t dec[2 * MF_PLUS_AES_BLOCK_SIZE];
    mf_plus_crypto_cbc_decrypt(key->data, zero_iv, &resp[1], dec, sizeof(dec));

    uint8_t rnd_a_rot[MF_PLUS_AES_BLOCK_SIZE];
    mf_plus_crypto_rotate_left(rnd_a, rnd_a_rot);
    if(memcmp(&dec[4], rnd_a_rrot, MF_PLUS_AES_BLOCK_SIZE) != 0 &&
       memcmp(&dec[4], rnd_a_rot, MF_PLUS_AES_BLOCK_SIZE) != 0 &&
       memcmp(&dec[4], rnd_a, MF_PLUS_AES_BLOCK_SIZE) != 0) {
        return MfPlusErrorAuth;
    }

    memcpy(session->ti, &dec[0], 4);
    session->r_ctr = 0;
    session->w_ctr = 0;

    mf_plus_crypto_derive_session_keys(
        key->data, rnd_a_rrot, rnd_b, session->k_enc, session->k_mac);

    return MfPlusErrorNone;
}

MfPlusError mf_plus_poller_authenticate(
    MfPlusPoller* instance,
    uint8_t sector,
    MfPlusKeyType key_type,
    const MfPlusKey* key,
    MfPlusPollerSession* session) {

    const uint16_t key_id = 0x4000U + ((uint16_t)sector << 1) + (uint8_t)key_type;
    return mf_plus_poller_authenticate_key_id(instance, key_id, key, session);
}

MfPlusError mf_plus_poller_read_block(
    MfPlusPoller* instance,
    uint8_t block_low,
    uint8_t block_high,
    bool plain,
    MfPlusPollerSession* session,
    MfPlusBlock* out) {
    furi_check(instance);
    furi_check(session);
    furi_check(out);

    const uint8_t opcode = plain ? MF_PLUS_CMD_READ_PLAIN : MF_PLUS_CMD_READ_ENC;

    const uint8_t payload[3] = {block_low, block_high, 0x01};
    uint8_t cmd_mac[MF_PLUS_MAC_SIZE];
    mf_plus_crypto_calculate_mac(
        session->k_mac, opcode, session->r_ctr, session->ti, payload, sizeof(payload), cmd_mac);

    uint8_t cmd[4 + MF_PLUS_MAC_SIZE];
    cmd[0] = opcode;
    cmd[1] = block_low;
    cmd[2] = block_high;
    cmd[3] = 0x01;
    memcpy(&cmd[4], cmd_mac, MF_PLUS_MAC_SIZE);

    uint8_t resp[64];
    size_t resp_len = 0;
    MfPlusError error =
        mf_plus_poller_send_raw(instance, cmd, sizeof(cmd), resp, &resp_len, sizeof(resp));
    if(error != MfPlusErrorNone) return error;

    if(resp_len < 1 + MF_PLUS_BLOCK_SIZE + MF_PLUS_MAC_SIZE || resp[0] != MF_PLUS_STATUS_OK) {
        return MfPlusErrorRejected;
    }

    session->r_ctr++;

    uint8_t mac_input[sizeof(payload) + MF_PLUS_BLOCK_SIZE];
    memcpy(&mac_input[0], payload, sizeof(payload));
    memcpy(&mac_input[sizeof(payload)], &resp[1], MF_PLUS_BLOCK_SIZE);
    uint8_t resp_mac[MF_PLUS_MAC_SIZE];
    mf_plus_crypto_calculate_mac(
        session->k_mac,
        MF_PLUS_STATUS_OK,
        session->r_ctr,
        session->ti,
        mac_input,
        sizeof(mac_input),
        resp_mac);
    if(memcmp(&resp[1 + MF_PLUS_BLOCK_SIZE], resp_mac, MF_PLUS_MAC_SIZE) != 0) {

        return MfPlusErrorAuth;
    }

    if(plain) {
        memcpy(out->data, &resp[1], MF_PLUS_BLOCK_SIZE);
    } else {
        uint8_t iv[MF_PLUS_AES_BLOCK_SIZE];
        mf_plus_crypto_build_read_iv(session->ti, session->r_ctr, session->w_ctr, iv);
        mf_plus_crypto_cbc_decrypt(session->k_enc, iv, &resp[1], out->data, MF_PLUS_BLOCK_SIZE);
    }

    return MfPlusErrorNone;
}

MfPlusError mf_plus_poller_write_block(
    MfPlusPoller* instance,
    uint8_t block_low,
    uint8_t block_high,
    bool plain,
    const MfPlusBlock* in,
    MfPlusPollerSession* session) {
    furi_check(instance);
    furi_check(session);
    furi_check(in);

    const uint8_t opcode = plain ? MF_PLUS_CMD_WRITE_PLAIN : MF_PLUS_CMD_WRITE_ENC;

    uint8_t wire_data[MF_PLUS_BLOCK_SIZE];
    if(plain) {
        memcpy(wire_data, in->data, MF_PLUS_BLOCK_SIZE);
    } else {
        uint8_t iv[MF_PLUS_AES_BLOCK_SIZE];
        mf_plus_crypto_build_write_iv(session->ti, session->r_ctr, session->w_ctr, iv);
        mf_plus_crypto_cbc_encrypt(session->k_enc, iv, in->data, wire_data, MF_PLUS_BLOCK_SIZE);
    }

    uint8_t mac_input[2 + MF_PLUS_BLOCK_SIZE];
    mac_input[0] = block_low;
    mac_input[1] = block_high;
    memcpy(&mac_input[2], wire_data, MF_PLUS_BLOCK_SIZE);
    uint8_t cmd_mac[MF_PLUS_MAC_SIZE];
    mf_plus_crypto_calculate_mac(
        session->k_mac, opcode, session->w_ctr, session->ti, mac_input, sizeof(mac_input), cmd_mac);

    uint8_t cmd[3 + MF_PLUS_BLOCK_SIZE + MF_PLUS_MAC_SIZE];
    cmd[0] = opcode;
    cmd[1] = block_low;
    cmd[2] = block_high;
    memcpy(&cmd[3], wire_data, MF_PLUS_BLOCK_SIZE);
    memcpy(&cmd[3 + MF_PLUS_BLOCK_SIZE], cmd_mac, MF_PLUS_MAC_SIZE);

    uint8_t resp[64];
    size_t resp_len = 0;
    MfPlusError error =
        mf_plus_poller_send_raw(instance, cmd, sizeof(cmd), resp, &resp_len, sizeof(resp));
    if(error != MfPlusErrorNone) return error;

    if(resp_len < 1 + MF_PLUS_MAC_SIZE || resp[0] != MF_PLUS_STATUS_OK) {
        return MfPlusErrorRejected;
    }

    session->w_ctr++;
    uint8_t resp_mac[MF_PLUS_MAC_SIZE];
    mf_plus_crypto_calculate_mac(
        session->k_mac, MF_PLUS_STATUS_OK, session->w_ctr, session->ti, NULL, 0, resp_mac);
    if(memcmp(&resp[1], resp_mac, MF_PLUS_MAC_SIZE) != 0) {

        return MfPlusErrorAuth;
    }

    return MfPlusErrorNone;
}

MfPlusError
    mf_plus_poller_read_signature(MfPlusPoller* instance, uint8_t* signature, bool* present) {
    furi_check(instance);
    furi_check(signature);
    furi_check(present);

    *present = false;

    const uint8_t cmd[2] = {MF_PLUS_CMD_READ_SIG, 0x00};
    uint8_t resp[64];
    size_t resp_len = 0;
    MfPlusError error =
        mf_plus_poller_send_raw(instance, cmd, sizeof(cmd), resp, &resp_len, sizeof(resp));
    if(error != MfPlusErrorNone) return error;

    const uint8_t* sig = NULL;
    if(resp_len >= 1 + MF_PLUS_SIGNATURE_SIZE && resp[0] == MF_PLUS_STATUS_OK) {
        sig = &resp[1];
    } else if(resp_len >= MF_PLUS_SIGNATURE_SIZE) {
        sig = &resp[0];
    }

    if(sig != NULL) {
        memcpy(signature, sig, MF_PLUS_SIGNATURE_SIZE);
        *present = true;
    }

    return MfPlusErrorNone;
}
