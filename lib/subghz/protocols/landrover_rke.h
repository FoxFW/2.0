#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LR_PREAMBLE_HIGH_US    400u
#define LR_PREAMBLE_LOW_US     600u
#define LR_PREAMBLE_COUNT       20u
#define LR_SYNC_HIGH_US        400u
#define LR_SYNC_LOW_US        9600u
#define LR_BIT1_HIGH_US        700u
#define LR_BIT1_LOW_US         300u
#define LR_BIT0_HIGH_US        300u
#define LR_BIT0_LOW_US         700u
#define LR_REPEAT_GAP_US     12000u
#define LR_REPEAT_COUNT          4u
#define LR_TOLERANCE_PCT        20u
#define LR_FRAME_BITS           66u

#define LR_FREQ_EU_HZ    433920000ul
#define LR_FREQ_US_HZ    315000000ul

#define LR_BTN_LOCK      0x1u
#define LR_BTN_UNLOCK    0x2u
#define LR_BTN_BOOT      0x4u
#define LR_BTN_PANIC     0x8u

#define LR_STATUS_BATTERY_LOW  0x1u
#define LR_STATUS_REPEAT       0x2u

typedef struct {
    uint32_t hop_code;
    uint32_t serial;
    uint8_t  button;
    uint8_t  func_bits;
    uint8_t  status;
    bool     valid;
} LandRoverFrame;

typedef struct {
    int32_t  pulses[512];
    uint32_t count;
} LandRoverRawBuf;

void lr_encode(const LandRoverFrame *frame, LandRoverRawBuf *buf);

bool lr_decode(const LandRoverRawBuf *buf, LandRoverFrame *frame);

bool lr_counter_valid(uint16_t stored, uint16_t received);

#ifdef __cplusplus
}
#endif
