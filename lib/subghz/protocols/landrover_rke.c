#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define LR_PREAMBLE_HIGH_US    400u
#define LR_PREAMBLE_LOW_US     600u
#define LR_PREAMBLE_COUNT       20u

#define LR_SYNC_HIGH_US        400u
#define LR_SYNC_LOW_US        9600u

#define LR_BIT_PERIOD_US      1000u
#define LR_BIT1_HIGH_US        700u
#define LR_BIT1_LOW_US         300u
#define LR_BIT0_HIGH_US        300u
#define LR_BIT0_LOW_US         700u

#define LR_REPEAT_GAP_US      12000u
#define LR_REPEAT_COUNT          4u
#define LR_TOLERANCE_PCT        20u

#define LR_FRAME_BITS           66u

#define LR_BTN_LOCK             0x1u
#define LR_BTN_UNLOCK           0x2u
#define LR_BTN_BOOT             0x4u
#define LR_BTN_PANIC            0x8u

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

static bool lr_in_range(int32_t measured_us, uint32_t ref_us)
{
    int32_t ref  = (int32_t)ref_us;
    int32_t diff = measured_us - ref;
    if (diff < 0) diff = -diff;
    return (diff * 100) <= (ref * (int32_t)LR_TOLERANCE_PCT);
}

static void lr_push(LandRoverRawBuf *buf, int32_t val)
{
    if (buf->count < 512) buf->pulses[buf->count++] = val;
}

static void lr_push_pair(LandRoverRawBuf *buf, uint32_t hi, uint32_t lo)
{
    lr_push(buf,  (int32_t)hi);
    lr_push(buf, -(int32_t)lo);
}

void lr_encode(const LandRoverFrame *frame, LandRoverRawBuf *buf)
{
    buf->count = 0;

    uint8_t bits[66];
    memset(bits, 0, sizeof(bits));

    for (int i = 0; i < 32; i++) {
        bits[65 - i] = (frame->hop_code >> i) & 1u;
    }

    for (int i = 0; i < 24; i++) {
        bits[33 - i] = (frame->serial >> i) & 1u;
    }

    for (int i = 0; i < 4; i++) {
        bits[9 - i] = (frame->button >> i) & 1u;
    }

    for (int i = 0; i < 4; i++) {
        bits[5 - i] = (frame->func_bits >> i) & 1u;
    }

    bits[1] = (frame->status >> 1) & 1u;
    bits[0] =  frame->status       & 1u;

    for (uint32_t rep = 0; rep < LR_REPEAT_COUNT; rep++) {

        for (uint32_t p = 0; p < LR_PREAMBLE_COUNT; p++) {
            lr_push_pair(buf, LR_PREAMBLE_HIGH_US, LR_PREAMBLE_LOW_US);
        }

        lr_push_pair(buf, LR_SYNC_HIGH_US, LR_SYNC_LOW_US);

        for (int b = 65; b >= 0; b--) {
            if (bits[b]) {
                lr_push_pair(buf, LR_BIT1_HIGH_US, LR_BIT1_LOW_US);
            } else {
                lr_push_pair(buf, LR_BIT0_HIGH_US, LR_BIT0_LOW_US);
            }
        }

        if (rep < LR_REPEAT_COUNT - 1) {
            lr_push(buf, -(int32_t)LR_REPEAT_GAP_US);
        }
    }
}

bool lr_decode(const LandRoverRawBuf *buf, LandRoverFrame *frame)
{
    memset(frame, 0, sizeof(*frame));

    for (uint32_t i = 0; i + 1 < buf->count; i++) {

        if (!lr_in_range( buf->pulses[i],     LR_SYNC_HIGH_US)) continue;
        if (!lr_in_range(-buf->pulses[i + 1], LR_SYNC_LOW_US))  continue;

        uint32_t j = i + 2;
        if (j + LR_FRAME_BITS * 2 > buf->count) continue;

        uint8_t bits[66];
        bool ok = true;

        for (uint32_t b = 0; b < LR_FRAME_BITS; b++) {
            int32_t hi =  buf->pulses[j];
            int32_t lo = -buf->pulses[j + 1];
            j += 2;

            if (lr_in_range(hi, LR_BIT1_HIGH_US) && lr_in_range(lo, LR_BIT1_LOW_US)) {
                bits[65 - b] = 1;
            } else if (lr_in_range(hi, LR_BIT0_HIGH_US) && lr_in_range(lo, LR_BIT0_LOW_US)) {
                bits[65 - b] = 0;
            } else {
                ok = false;
                break;
            }
        }

        if (!ok) continue;

        frame->hop_code = 0;
        for (int k = 0; k < 32; k++) {
            frame->hop_code |= (uint32_t)bits[65 - k] << (31 - k);
        }
        frame->serial = 0;
        for (int k = 0; k < 24; k++) {
            frame->serial |= (uint32_t)bits[33 - k] << (23 - k);
        }
        frame->button = 0;
        for (int k = 0; k < 4; k++) {
            frame->button |= (uint8_t)bits[9 - k] << (3 - k);
        }
        frame->func_bits = 0;
        for (int k = 0; k < 4; k++) {
            frame->func_bits |= (uint8_t)bits[5 - k] << (3 - k);
        }
        frame->status = (bits[1] << 1) | bits[0];
        frame->valid  = true;
        return true;
    }

    return false;
}

bool lr_counter_valid(uint16_t stored, uint16_t received)
{
    uint16_t delta = (uint16_t)(received - stored);
    return (delta >= 1u && delta <= 32768u);
}
