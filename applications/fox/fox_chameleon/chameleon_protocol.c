#include "chameleon_protocol.h"

#include <string.h>
#include <stdio.h>

#define CHAMELEON_SOF 0x11

/* LRC is the 8-bit two's complement of the sum of the given bytes, i.e.
   the value that makes (sum of bytes + lrc) == 0 mod 256. */
static uint8_t lrc(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for(size_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + data[i]);
    }
    return (uint8_t)(0 - sum);
}

size_t chameleon_build_frame(
    uint16_t cmd,
    const uint8_t* data,
    uint16_t data_len,
    uint8_t* out,
    size_t out_capacity) {
    if(data_len > CHAMELEON_MAX_DATA) return 0;

    size_t frame_len = CHAMELEON_FRAME_OVERHEAD + data_len;
    if(out_capacity < frame_len) return 0;

    out[0] = CHAMELEON_SOF;
    out[1] = lrc(out, 1);
    out[2] = (uint8_t)(cmd >> 8);
    out[3] = (uint8_t)(cmd & 0xFF);
    out[4] = 0x00;
    out[5] = 0x00;
    out[6] = (uint8_t)(data_len >> 8);
    out[7] = (uint8_t)(data_len & 0xFF);
    /* LRC2 covers CMD|STATUS|LEN - that's 6 bytes (2+2+2), out[2..7],
       confirmed against RfidResearchGroup/ChameleonUltraDocs protocol.md
       ("LRC2: 1 byte, LRC over CMD|STATUS|LEN bytes"). A prior version of
       this line summed only 5 bytes (out[2..6]), silently dropping LEN's
       low byte from the checksum. That's invisible for any frame whose
       data_len fits with a zero low byte (data_len == 0, i.e. most
       *commands* sent to the device), which is why simple round trips
       could look fine - but it corrupts LRC2 for essentially every real
       *response*, since almost every Chameleon response carries a
       non-zero, sub-256 data_len (e.g. GET_APP_VERSION's LEN=2), whose
       low byte is exactly the byte this was dropping. This one-byte
       omission is what was causing "OK" followed by "Malformed response
       frame" on nearly every info request. */
    out[8] = lrc(&out[2], 6);

    if(data_len > 0 && data != NULL) {
        memcpy(&out[9], data, data_len);
    }
    out[9 + data_len] = lrc(&out[9], data_len);

    return frame_len;
}

const char* chameleon_frame_error_str(ChameleonFrameError error) {
    switch(error) {
    case ChameleonFrameOk:
        return "ok";
    case ChameleonFrameErrTooShort:
        return "frame shorter than 10-byte overhead";
    case ChameleonFrameErrBadSof:
        return "bad SOF (not 0x11)";
    case ChameleonFrameErrBadLrc1:
        return "bad LRC1 (SOF checksum)";
    case ChameleonFrameErrBadLrc2:
        return "bad LRC2 (CMD|STATUS|LEN checksum)";
    case ChameleonFrameErrLenExceedsBuffer:
        return "declared LEN exceeds bytes received (fragmented?)";
    case ChameleonFrameErrBadLrc3:
        return "bad LRC3 (DATA checksum)";
    default:
        return "unknown";
    }
}

bool chameleon_parse_frame_ex(
    const uint8_t* buffer,
    size_t length,
    ChameleonFrame* out,
    ChameleonFrameError* error_out) {
    if(length < CHAMELEON_FRAME_OVERHEAD) {
        if(error_out) *error_out = ChameleonFrameErrTooShort;
        return false;
    }
    if(buffer[0] != CHAMELEON_SOF) {
        if(error_out) *error_out = ChameleonFrameErrBadSof;
        return false;
    }
    if(buffer[1] != lrc(buffer, 1)) {
        if(error_out) *error_out = ChameleonFrameErrBadLrc1;
        return false;
    }
    /* Must match chameleon_build_frame()'s LRC2 - see the comment there.
       6 bytes (CMD|STATUS|LEN), not 5. */
    if(buffer[8] != lrc(&buffer[2], 6)) {
        if(error_out) *error_out = ChameleonFrameErrBadLrc2;
        return false;
    }

    uint16_t data_len = (uint16_t)((buffer[6] << 8) | buffer[7]);
    if(length < (size_t)(CHAMELEON_FRAME_OVERHEAD + data_len)) {
        if(error_out) *error_out = ChameleonFrameErrLenExceedsBuffer;
        return false;
    }
    if(buffer[9 + data_len] != lrc(&buffer[9], data_len)) {
        if(error_out) *error_out = ChameleonFrameErrBadLrc3;
        return false;
    }

    out->cmd = (uint16_t)((buffer[2] << 8) | buffer[3]);
    out->status = (uint16_t)((buffer[4] << 8) | buffer[5]);
    out->data_len = data_len;
    out->data = &buffer[9];
    if(error_out) *error_out = ChameleonFrameOk;
    return true;
}

bool chameleon_parse_frame(const uint8_t* buffer, size_t length, ChameleonFrame* out) {
    return chameleon_parse_frame_ex(buffer, length, out, NULL);
}

size_t chameleon_build_get_app_version(uint8_t* out, size_t out_capacity) {
    return chameleon_build_frame(CHAMELEON_CMD_GET_APP_VERSION, NULL, 0, out, out_capacity);
}

size_t chameleon_build_get_git_version(uint8_t* out, size_t out_capacity) {
    return chameleon_build_frame(CHAMELEON_CMD_GET_GIT_VERSION, NULL, 0, out, out_capacity);
}

size_t chameleon_build_get_battery_info(uint8_t* out, size_t out_capacity) {
    return chameleon_build_frame(CHAMELEON_CMD_GET_BATTERY_INFO, NULL, 0, out, out_capacity);
}

size_t chameleon_build_get_active_slot(uint8_t* out, size_t out_capacity) {
    return chameleon_build_frame(CHAMELEON_CMD_GET_ACTIVE_SLOT, NULL, 0, out, out_capacity);
}

size_t chameleon_build_get_device_model(uint8_t* out, size_t out_capacity) {
    return chameleon_build_frame(CHAMELEON_CMD_GET_DEVICE_MODEL, NULL, 0, out, out_capacity);
}

size_t chameleon_build_get_device_chip_id(uint8_t* out, size_t out_capacity) {
    return chameleon_build_frame(CHAMELEON_CMD_GET_DEVICE_CHIP_ID, NULL, 0, out, out_capacity);
}

size_t chameleon_build_get_device_address(uint8_t* out, size_t out_capacity) {
    return chameleon_build_frame(CHAMELEON_CMD_GET_DEVICE_ADDRESS, NULL, 0, out, out_capacity);
}

size_t chameleon_build_get_enabled_slots(uint8_t* out, size_t out_capacity) {
    return chameleon_build_frame(CHAMELEON_CMD_GET_ENABLED_SLOTS, NULL, 0, out, out_capacity);
}

size_t chameleon_build_get_device_mode(uint8_t* out, size_t out_capacity) {
    return chameleon_build_frame(CHAMELEON_CMD_GET_DEVICE_MODE, NULL, 0, out, out_capacity);
}

size_t chameleon_build_mf1_detect_support(uint8_t* out, size_t out_capacity) {
    return chameleon_build_frame(CHAMELEON_CMD_MF1_DETECT_SUPPORT, NULL, 0, out, out_capacity);
}

size_t chameleon_build_hf14a_scan(uint8_t* out, size_t out_capacity) {
    return chameleon_build_frame(CHAMELEON_CMD_HF14A_SCAN, NULL, 0, out, out_capacity);
}

size_t chameleon_build_set_active_slot(uint8_t slot, uint8_t* out, size_t out_capacity) {
    if(slot > 7) return 0;
    return chameleon_build_frame(CHAMELEON_CMD_SET_ACTIVE_SLOT, &slot, 1, out, out_capacity);
}

size_t chameleon_build_change_device_mode(uint8_t mode, uint8_t* out, size_t out_capacity) {
    if(mode > 1) return 0;
    return chameleon_build_frame(CHAMELEON_CMD_CHANGE_DEVICE_MODE, &mode, 1, out, out_capacity);
}

size_t chameleon_build_mf1_read_emu_block(
    uint8_t block_start,
    uint8_t block_count,
    uint8_t* out,
    size_t out_capacity) {
    if(block_count < 1 || block_count > 32) return 0;
    uint8_t data[2] = {block_start, block_count};
    return chameleon_build_frame(
        CHAMELEON_CMD_MF1_READ_EMU_BLOCK_DATA, data, 2, out, out_capacity);
}

size_t chameleon_build_mf1_read_one_block(
    uint8_t type,
    uint8_t block,
    const uint8_t key[6],
    uint8_t* out,
    size_t out_capacity) {
    if(type != 0x60 && type != 0x61) return 0;
    uint8_t data[8];
    data[0] = type;
    data[1] = block;
    memcpy(&data[2], key, 6);
    return chameleon_build_frame(CHAMELEON_CMD_MF1_READ_ONE_BLOCK, data, 8, out, out_capacity);
}

void chameleon_format_response(const ChameleonFrame* frame, char* out, size_t out_capacity) {
    switch(frame->cmd) {
    case CHAMELEON_CMD_GET_APP_VERSION:
        if(frame->data_len >= 2) {
            snprintf(out, out_capacity, "firmware v%u.%u", frame->data[0], frame->data[1]);
            return;
        }
        break;
    case CHAMELEON_CMD_GET_GIT_VERSION: {
        size_t n = frame->data_len < (out_capacity - 1) ? frame->data_len : (out_capacity - 1);
        memcpy(out, frame->data, n);
        out[n] = '\0';
        return;
    }
    case CHAMELEON_CMD_GET_BATTERY_INFO:
        if(frame->data_len >= 3) {
            uint16_t mv = (uint16_t)((frame->data[0] << 8) | frame->data[1]);
            snprintf(out, out_capacity, "%u mV, %u%%", mv, frame->data[2]);
            return;
        }
        break;
    case CHAMELEON_CMD_GET_ACTIVE_SLOT:
        if(frame->data_len >= 1) {
            snprintf(out, out_capacity, "active slot %u (0-7)", frame->data[0]);
            return;
        }
        break;
    case CHAMELEON_CMD_GET_DEVICE_MODEL:
        if(frame->data_len >= 1) {
            snprintf(out, out_capacity, "model: %s", frame->data[0] == 0 ? "Ultra" : "Lite");
            return;
        }
        break;
    case CHAMELEON_CMD_SET_ACTIVE_SLOT:
        snprintf(out, out_capacity, "slot switched");
        return;
    case CHAMELEON_CMD_CHANGE_DEVICE_MODE:
        snprintf(out, out_capacity, "mode changed");
        return;
    case CHAMELEON_CMD_GET_DEVICE_MODE:
        if(frame->data_len >= 1) {
            snprintf(out, out_capacity, "mode: %s", frame->data[0] == 0 ? "emulator" : "reader");
            return;
        }
        break;
    case CHAMELEON_CMD_MF1_DETECT_SUPPORT:
        if(frame->data_len >= 1) {
            snprintf(
                out,
                out_capacity,
                "Mifare Classic: %s",
                frame->data[0] != 0 ? "supported" : "not supported");
            return;
        }
        break;
    case CHAMELEON_CMD_GET_DEVICE_CHIP_ID:
        if(frame->data_len >= 8) {
            snprintf(
                out,
                out_capacity,
                "chip %02X%02X%02X%02X%02X%02X%02X%02X",
                frame->data[0],
                frame->data[1],
                frame->data[2],
                frame->data[3],
                frame->data[4],
                frame->data[5],
                frame->data[6],
                frame->data[7]);
            return;
        }
        break;
    case CHAMELEON_CMD_GET_DEVICE_ADDRESS:
        if(frame->data_len >= 6) {
            snprintf(
                out,
                out_capacity,
                "%02X:%02X:%02X:%02X:%02X:%02X",
                frame->data[0],
                frame->data[1],
                frame->data[2],
                frame->data[3],
                frame->data[4],
                frame->data[5]);
            return;
        }
        break;
    case CHAMELEON_CMD_GET_ENABLED_SLOTS:
        if(frame->data_len >= 16) {
            char list[32];
            size_t pos = 0;
            for(int i = 0; i < 8 && pos < sizeof(list) - 4; i++) {
                bool enabled = frame->data[i * 2] != 0 || frame->data[i * 2 + 1] != 0;
                int written =
                    snprintf(list + pos, sizeof(list) - pos, "%d%s ", i, enabled ? "*" : "");
                if(written > 0) pos += (size_t)written;
            }
            snprintf(out, out_capacity, "slots: %s", list);
            return;
        }
        break;
    case CHAMELEON_CMD_MF1_READ_EMU_BLOCK_DATA:
        break;
    default:
        break;
    }
    snprintf(out, out_capacity, "%u bytes of data", frame->data_len);
}

void chameleon_format_hf14a_scan(const ChameleonFrame* frame, char* out, size_t out_capacity) {
    if(frame->data_len < 1) {
        snprintf(out, out_capacity, "no card detected");
        return;
    }

    uint8_t uid_len = frame->data[0];
    size_t fixed_fields = 3; /* atqa[2] + sak */
    if(frame->data_len < (size_t)(1 + uid_len + fixed_fields)) {
        snprintf(out, out_capacity, "malformed scan response");
        return;
    }

    const uint8_t* uid = &frame->data[1];
    uint8_t atqa0 = frame->data[1 + uid_len];
    uint8_t atqa1 = frame->data[1 + uid_len + 1];
    uint8_t sak = frame->data[1 + uid_len + 2];

    char uid_hex[24] = {0};
    size_t pos = 0;
    for(uint8_t i = 0; i < uid_len && pos + 2 < sizeof(uid_hex); i++) {
        int written = snprintf(uid_hex + pos, sizeof(uid_hex) - pos, "%02X", uid[i]);
        if(written > 0) pos += (size_t)written;
    }

    snprintf(out, out_capacity, "UID %s ATQA %02X%02X SAK %02X", uid_hex, atqa0, atqa1, sak);
}

void chameleon_format_uid_block(const uint8_t* block, char* out, size_t out_capacity) {
    uint8_t calculated_bcc = (uint8_t)(block[0] ^ block[1] ^ block[2] ^ block[3]);

    if(block[4] == calculated_bcc) {
        snprintf(
            out,
            out_capacity,
            "UID %02X%02X%02X%02X (4B, BCC ok)",
            block[0],
            block[1],
            block[2],
            block[3]);
    } else {
        snprintf(
            out,
            out_capacity,
            "UID %02X%02X%02X%02X%02X%02X%02X (7B assumed)",
            block[0],
            block[1],
            block[2],
            block[3],
            block[4],
            block[5],
            block[6]);
    }
}
