#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INFRARED_COMMON_CARRIER_FREQUENCY ((uint32_t)38000)
#define INFRARED_COMMON_DUTY_CYCLE        ((float)0.33)

#define INFRARED_RAW_RX_TIMING_DELAY_US 150000
#define INFRARED_RAW_TX_TIMING_DELAY_US 180000

typedef struct InfraredDecoderHandler InfraredDecoderHandler;
typedef struct InfraredEncoderHandler InfraredEncoderHandler;

typedef enum {
    InfraredProtocolUnknown = -1,
    InfraredProtocolNEC = 0,
    InfraredProtocolNECext,
    InfraredProtocolNEC42,
    InfraredProtocolNEC42ext,
    InfraredProtocolSamsung32,
    InfraredProtocolRC6,
    InfraredProtocolRC5,
    InfraredProtocolRC5X,
    InfraredProtocolSIRC,
    InfraredProtocolSIRC15,
    InfraredProtocolSIRC20,
    InfraredProtocolKaseikyo,
    InfraredProtocolRCA,
    InfraredProtocolPioneer,

    InfraredProtocolMAX,
} InfraredProtocol;

typedef struct {
    InfraredProtocol protocol;
    uint32_t address;
    uint32_t command;
    bool repeat;
} InfraredMessage;

typedef enum {
    InfraredStatusError,
    InfraredStatusOk,
    InfraredStatusDone,
    InfraredStatusReady,
} InfraredStatus;

InfraredDecoderHandler* infrared_alloc_decoder(void);

const InfraredMessage*
    infrared_decode(InfraredDecoderHandler* handler, bool level, uint32_t duration);

const InfraredMessage* infrared_check_decoder_ready(InfraredDecoderHandler* handler);

void infrared_free_decoder(InfraredDecoderHandler* handler);

void infrared_reset_decoder(InfraredDecoderHandler* handler);

const char* infrared_get_protocol_name(InfraredProtocol protocol);

InfraredProtocol infrared_get_protocol_by_name(const char* protocol_name);

uint8_t infrared_get_protocol_address_length(InfraredProtocol protocol);

uint8_t infrared_get_protocol_command_length(InfraredProtocol protocol);

bool infrared_is_protocol_valid(InfraredProtocol protocol);

InfraredEncoderHandler* infrared_alloc_encoder(void);

void infrared_free_encoder(InfraredEncoderHandler* handler);

InfraredStatus infrared_encode(InfraredEncoderHandler* handler, uint32_t* duration, bool* level);

void infrared_reset_encoder(InfraredEncoderHandler* handler, const InfraredMessage* message);

uint32_t infrared_get_protocol_frequency(InfraredProtocol protocol);

float infrared_get_protocol_duty_cycle(InfraredProtocol protocol);

size_t infrared_get_protocol_min_repeat_count(InfraredProtocol protocol);

#ifdef __cplusplus
}
#endif
