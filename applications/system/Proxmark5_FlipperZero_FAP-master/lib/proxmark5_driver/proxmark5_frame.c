#include <furi_hal.h>
#include <inttypes.h>
#include <string.h>

#include "proxmark5_com.h"
#include "proxmark5_frame.h"

#define PROXMARK5_LOG_TAG "Proxmark5_FRAME"

#define PM3_FLAG_LOG     0x01U
#define PM3_FLAG_NEWLINE 0x02U
#define PM3_FLAG_INPLACE 0x04U

#define PM3_RESPONSE_CACHE_SIZE      16U
#define PM3_OLD_RESPONSE_HEADER_SIZE (sizeof(uint64_t) * 4U)

typedef struct {
    PacketResponseNG cache[PM3_RESPONSE_CACHE_SIZE];
    uint8_t head;
    uint8_t tail;
    // The SPI RX thread enqueues while worker threads take; the ring needs a lock.
    FuriMutex* mutex;
} Proxmark5FrameContext;

static Proxmark5FrameContext frame_context;

static void proxmark5_frame_lock(void) {
    if(frame_context.mutex) {
        furi_mutex_acquire(frame_context.mutex, FuriWaitForever);
    }
}

static void proxmark5_frame_unlock(void) {
    if(frame_context.mutex) {
        furi_mutex_release(frame_context.mutex);
    }
}

static uint16_t pm5_u16_le(uint8_t* data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t pm5_u32_le(uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint64_t pm5_u64_le(uint8_t* data) {
    return (uint64_t)data[0] | ((uint64_t)data[1] << 8) | ((uint64_t)data[2] << 16) |
           ((uint64_t)data[3] << 24) | ((uint64_t)data[4] << 32) | ((uint64_t)data[5] << 40) |
           ((uint64_t)data[6] << 48) | ((uint64_t)data[7] << 56);
}

static uint16_t proxmark5_frame_crc14443a(uint8_t* data, size_t length) {
    uint16_t crc = 0x6363U;

    for(size_t i = 0; i < length; i++) {
        uint8_t ch = data[i] ^ (uint8_t)(crc & 0x00FFU);
        ch ^= (uint8_t)(ch << 4);
        crc = (crc >> 8) ^ ((uint16_t)ch << 8) ^ ((uint16_t)ch << 3) ^ ((uint16_t)ch >> 4);
    }

    // Return CRC as little-endian ordered value.
    return (uint16_t)((crc >> 8) | (crc << 8));
}

static bool proxmark5_frame_enqueue(PacketResponseNG* packet) {
    proxmark5_frame_lock();

    uint8_t next_head = (uint8_t)((frame_context.head + 1U) % PM3_RESPONSE_CACHE_SIZE);
    if(next_head == frame_context.tail) {
        FURI_LOG_W(PROXMARK5_LOG_TAG, "Packet cache overflow, dropping oldest entry");
        frame_context.tail = (uint8_t)((frame_context.tail + 1U) % PM3_RESPONSE_CACHE_SIZE);
    }

    frame_context.cache[frame_context.head] = *packet;
    frame_context.head = next_head;

    proxmark5_frame_unlock();
    return true;
}

static void proxmark5_frame_log_print_string(uint8_t* data, size_t length, uint16_t flag) {
    char log_message[PM3_CMD_DATA_SIZE + 1U];
    size_t copy_len = length > PM3_CMD_DATA_SIZE ? PM3_CMD_DATA_SIZE : length;
    memcpy(log_message, data, copy_len);
    log_message[copy_len] = '\0';

    if(flag & PM3_FLAG_LOG) {
        FURI_LOG_I(PROXMARK5_LOG_TAG, "[#] %s", log_message);
    } else if(flag & PM3_FLAG_INPLACE) {
        FURI_LOG_I(PROXMARK5_LOG_TAG, "[inplace] %s", log_message);
    } else if(flag & PM3_FLAG_NEWLINE) {
        FURI_LOG_I(PROXMARK5_LOG_TAG, "%s", log_message);
    } else {
        FURI_LOG_I(PROXMARK5_LOG_TAG, "%s", log_message);
    }
}

static bool proxmark5_frame_handle_ng_packet(uint8_t* packet, size_t packet_len) {
    uint16_t length_and_ng = pm5_u16_le(packet + 4);
    bool ng = (length_and_ng & 0x8000U) != 0;
    uint16_t payload_length = length_and_ng & 0x7FFFU;
    int8_t status = (int8_t)packet[6];
    int8_t reason = (int8_t)packet[7];
    uint16_t cmd = pm5_u16_le(packet + 8);

    size_t expected_len =
        sizeof(PacketResponseNGPreamble) + payload_length + sizeof(PacketResponseNGPostamble);
    if(packet_len < expected_len) {
        return false;
    }

    // Log the data packet
    char* hex_array = (char*)malloc(2 * packet_len);
    if(hex_array) {
        for(size_t i = 0; i < packet_len; i++) {
            snprintf(hex_array + (i * 2), 2 * packet_len, "%02X", packet[i]);
        }
        FURI_LOG_I(PROXMARK5_LOG_TAG, "Packet (hex): %s", hex_array);
        free(hex_array);
    }

    // Log the packet info
    FURI_LOG_I(
        PROXMARK5_LOG_TAG,
        "Received NG packet: cmd=0x%04X status=%d reason=%d ng=%d payload_length=%u",
        cmd,
        status,
        reason,
        ng,
        payload_length);

    uint16_t packet_crc = pm5_u16_le(packet + sizeof(PacketResponseNGPreamble) + payload_length);
    if(packet_crc != RESPONSENG_POSTAMBLE_MAGIC) {
        uint16_t computed_crc =
            proxmark5_frame_crc14443a(packet, sizeof(PacketResponseNGPreamble) + payload_length);
        if(packet_crc != computed_crc) {
            FURI_LOG_W(
                PROXMARK5_LOG_TAG,
                "Invalid NG response CRC: got 0x%04X expected 0x%04X",
                packet_crc,
                computed_crc);
            return false;
        }
    }

    uint8_t* payload = packet + sizeof(PacketResponseNGPreamble);
    if(cmd == CMD_DEBUG_PRINT_STRING) {
        if(ng) {
            if(payload_length < sizeof(uint16_t)) {
                return true;
            }
            uint16_t flag = pm5_u16_le(payload);
            proxmark5_frame_log_print_string(
                payload + sizeof(uint16_t), payload_length - sizeof(uint16_t), flag);
            return true;
        }

        if(payload_length < (sizeof(uint64_t) * 3U)) {
            return true;
        }

        uint64_t old_len64 = pm5_u64_le(payload);
        uint64_t old_flag64 = pm5_u64_le(payload + sizeof(uint64_t));
        size_t text_len = (size_t)old_len64;
        size_t max_text_len = payload_length - (sizeof(uint64_t) * 3U);
        if(text_len > max_text_len) {
            text_len = max_text_len;
        }

        proxmark5_frame_log_print_string(
            payload + (sizeof(uint64_t) * 3U), text_len, (uint16_t)old_flag64);
        return true;
    }

    if(cmd == CMD_DEBUG_PRINT_INTEGERS) {
        if(!ng && payload_length >= (sizeof(uint64_t) * 3U)) {
            uint64_t a0 = pm5_u64_le(payload);
            uint64_t a1 = pm5_u64_le(payload + sizeof(uint64_t));
            uint64_t a2 = pm5_u64_le(payload + (sizeof(uint64_t) * 2U));
            FURI_LOG_I(PROXMARK5_LOG_TAG, "[#] %" PRIx64 ", %" PRIx64 ", %" PRIx64, a0, a1, a2);
        } else {
            FURI_LOG_I(PROXMARK5_LOG_TAG, "[#] debug integers packet received");
        }
        return true;
    }

    PacketResponseNG response;
    memset(&response, 0, sizeof(response));
    response.magic = RESPONSENG_PREAMBLE_MAGIC;
    response.ng = ng;
    response.cmd = cmd;
    response.status = status;
    response.reason = reason;
    response.crc = packet_crc;

    if(ng) {
        size_t copy_len = payload_length > PM3_CMD_DATA_SIZE ? PM3_CMD_DATA_SIZE : payload_length;
        memcpy(response.data.asBytes, payload, copy_len);
        response.length = (uint16_t)copy_len;
    } else {
        if(payload_length >= (sizeof(uint64_t) * 3U)) {
            response.oldarg[0] = pm5_u64_le(payload);
            response.oldarg[1] = pm5_u64_le(payload + sizeof(uint64_t));
            response.oldarg[2] = pm5_u64_le(payload + (sizeof(uint64_t) * 2U));
            size_t data_len = payload_length - (sizeof(uint64_t) * 3U);
            size_t copy_len = data_len > PM3_CMD_DATA_SIZE ? PM3_CMD_DATA_SIZE : data_len;
            memcpy(response.data.asBytes, payload + (sizeof(uint64_t) * 3U), copy_len);
            response.length = (uint16_t)copy_len;
        }
    }

    return proxmark5_frame_enqueue(&response);
}

static bool proxmark5_frame_handle_old_packet(uint8_t* packet, size_t packet_len) {
    if(packet_len < PM3_OLD_RESPONSE_HEADER_SIZE) {
        return false;
    }

    uint16_t cmd = (uint16_t)pm5_u64_le(packet);
    if(cmd == CMD_DEBUG_PRINT_STRING) {
        uint64_t old_len64 = pm5_u64_le(packet + sizeof(uint64_t));
        uint64_t old_flag64 = pm5_u64_le(packet + (sizeof(uint64_t) * 2U));
        size_t text_len = (size_t)old_len64;
        size_t max_text_len = packet_len - PM3_OLD_RESPONSE_HEADER_SIZE;
        if(text_len > max_text_len) {
            text_len = max_text_len;
        }

        proxmark5_frame_log_print_string(
            packet + PM3_OLD_RESPONSE_HEADER_SIZE, text_len, (uint16_t)old_flag64);
        return true;
    }

    if(cmd == CMD_DEBUG_PRINT_INTEGERS) {
        uint64_t a0 = pm5_u64_le(packet + sizeof(uint64_t));
        uint64_t a1 = pm5_u64_le(packet + (sizeof(uint64_t) * 2U));
        uint64_t a2 = pm5_u64_le(packet + (sizeof(uint64_t) * 3U));
        FURI_LOG_I(PROXMARK5_LOG_TAG, "[#] %" PRIx64 ", %" PRIx64 ", %" PRIx64, a0, a1, a2);
        return true;
    }

    PacketResponseNG response;
    memset(&response, 0, sizeof(response));
    response.ng = false;
    response.cmd = cmd;
    response.oldarg[0] = pm5_u64_le(packet + sizeof(uint64_t));
    response.oldarg[1] = pm5_u64_le(packet + (sizeof(uint64_t) * 2U));
    response.oldarg[2] = pm5_u64_le(packet + (sizeof(uint64_t) * 3U));

    size_t data_len = packet_len - PM3_OLD_RESPONSE_HEADER_SIZE;
    size_t copy_len = data_len > PM3_CMD_DATA_SIZE ? PM3_CMD_DATA_SIZE : data_len;
    memcpy(response.data.asBytes, packet + PM3_OLD_RESPONSE_HEADER_SIZE, copy_len);
    response.length = (uint16_t)copy_len;

    return proxmark5_frame_enqueue(&response);
}

void proxmark5_frame_init(void) {
    memset(&frame_context, 0, sizeof(frame_context));
    frame_context.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
}

void proxmark5_frame_deinit(void) {
    if(frame_context.mutex) {
        FuriMutex* mutex = frame_context.mutex;
        frame_context.mutex = NULL;
        furi_mutex_free(mutex);
    }
}

void proxmark5_frame_reset(void) {
    proxmark5_frame_lock();
    frame_context.head = 0;
    frame_context.tail = 0;
    proxmark5_frame_unlock();
}

void clearCommandBuffer(void) {
    proxmark5_frame_reset();
}

bool proxmark5_frame_handle_packet(uint8_t* packet, size_t packet_len) {
    if(packet == NULL || packet_len == 0) {
        return false;
    }

    if(packet_len >= sizeof(PacketResponseNGPreamble) &&
       pm5_u32_le(packet) == RESPONSENG_PREAMBLE_MAGIC) {
        return proxmark5_frame_handle_ng_packet(packet, packet_len);
    }

    return proxmark5_frame_handle_old_packet(packet, packet_len);
}

bool proxmark5_frame_pop(PacketResponseNG* packet) {
    if(packet == NULL) {
        return false;
    }

    proxmark5_frame_lock();

    if(frame_context.head == frame_context.tail) {
        proxmark5_frame_unlock();
        return false;
    }

    *packet = frame_context.cache[frame_context.tail];
    frame_context.tail = (uint8_t)((frame_context.tail + 1U) % PM3_RESPONSE_CACHE_SIZE);

    proxmark5_frame_unlock();
    return true;
}

bool proxmark5_frame_get_response(PacketResponseNG* packet) {
    return proxmark5_frame_pop(packet);
}

// Compacts the ring in place, preserving the order of the surviving entries.
//
// This previously built a `PacketResponseNG temp[PM3_RESPONSE_CACHE_SIZE]` local. With
// sizeof(PacketResponseNG) at ~560 bytes that is ~9KB of stack, far past the budget of
// the caller's thread, so it overflowed as soon as the ring was non-empty -- i.e. only
// after the PM3 had actually replied, which is why it looked like a response-handling
// bug rather than a stack one.
bool proxmark5_frame_take_by_cmd(uint16_t cmd, PacketResponseNG* packet) {
    if(packet == NULL) {
        return false;
    }

    proxmark5_frame_lock();

    if(frame_context.head == frame_context.tail) {
        proxmark5_frame_unlock();
        return false;
    }

    uint8_t read_idx = frame_context.tail;
    uint8_t write_idx = frame_context.tail;
    bool found = false;

    while(read_idx != frame_context.head) {
        PacketResponseNG* current = &frame_context.cache[read_idx];

        if(!found && current->cmd == cmd) {
            *packet = *current;
            found = true;
        } else {
            if(write_idx != read_idx) {
                frame_context.cache[write_idx] = *current;
            }
            write_idx = (uint8_t)((write_idx + 1U) % PM3_RESPONSE_CACHE_SIZE);
        }

        read_idx = (uint8_t)((read_idx + 1U) % PM3_RESPONSE_CACHE_SIZE);
    }

    frame_context.head = write_idx;

    proxmark5_frame_unlock();
    return found;
}

bool WaitForResponseTimeout(uint16_t cmd, PacketResponseNG* packet, uint32_t timeout_ms) {
    uint32_t start_time = furi_get_tick();

    while((furi_get_tick() - start_time) < timeout_ms) {
        if(proxmark5_frame_take_by_cmd(cmd, packet)) {
            return true;
        }
        furi_delay_ms(10);
    }

    return proxmark5_frame_take_by_cmd(cmd, packet);
}

static void proxmark5_frame_send_ng_internal(uint16_t cmd, uint8_t* data, size_t len, bool ng) {
    PacketCommandNGRaw tx;
    memset(&tx, 0, sizeof(tx));

    if(len > PM3_CMD_DATA_SIZE) {
        len = PM3_CMD_DATA_SIZE;
    }

    tx.pre.magic = COMMANDNG_PREAMBLE_MAGIC;
    tx.pre.ng = ng;
    tx.pre.length = (uint16_t)len;
    tx.pre.cmd = cmd;
    if(len != 0U && data != NULL) {
        memcpy(tx.data, data, len);
    }

    PacketCommandNGPostamble* tx_post =
        (PacketCommandNGPostamble*)((uint8_t*)&tx.pre + sizeof(PacketCommandNGPreamble) + len);
    tx_post->crc = COMMANDNG_POSTAMBLE_MAGIC;

    (void)proxmark5_com_send_spi(
        (uint8_t*)&tx, sizeof(PacketCommandNGPreamble) + len + sizeof(PacketCommandNGPostamble));
}

void SendCommandOLD(
    uint64_t cmd,
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    void* data,
    size_t len) {
    PacketCommandOLD tx;
    memset(&tx, 0, sizeof(tx));

    tx.cmd = cmd;
    tx.arg[0] = arg0;
    tx.arg[1] = arg1;
    tx.arg[2] = arg2;

    if(data != NULL && len != 0U) {
        size_t copy_len = len > PM3_CMD_DATA_SIZE ? PM3_CMD_DATA_SIZE : len;
        memcpy(tx.d.asBytes, data, copy_len);
    }

    (void)proxmark5_com_send_spi((uint8_t*)&tx, sizeof(tx));
}

void SendCommandNG(uint16_t cmd, uint8_t* data, size_t len) {
    proxmark5_frame_send_ng_internal(cmd, data, len, true);
}

void SendCommandMIX(
    uint64_t cmd,
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    void* data,
    size_t len) {
    uint8_t cmddata[PM3_CMD_DATA_SIZE];
    uint64_t arg[3] = {arg0, arg1, arg2};
    size_t payload_len = len;

    if(payload_len > PM3_CMD_DATA_SIZE - sizeof(arg)) {
        payload_len = PM3_CMD_DATA_SIZE - sizeof(arg);
    }

    memcpy(cmddata, arg, sizeof(arg));
    if(data != NULL && payload_len != 0U) {
        memcpy(cmddata + sizeof(arg), data, payload_len);
    }

    proxmark5_frame_send_ng_internal(
        (uint16_t)(cmd & 0xFFFFU), cmddata, payload_len + sizeof(arg), false);
}
