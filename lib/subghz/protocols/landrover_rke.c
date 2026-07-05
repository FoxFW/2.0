/**
 * landrover_rke.c
 * Land Rover RKE (Remote Keyless Entry) protocol — ported from Pandora DXL 5000 firmware
 * Target: Flipper Zero (SubGHz RAW / custom protocol plugin)
 *
 * Protocol ID in original firmware: 0x0E
 * Co-located in firmware with Ford/Jaguar (case 0x0E references 0xed58 "Ford/Jaguar"
 * then falls through to 0xed64 "Land Rover" — same baseband, different ID range).
 *
 * Frequency: 433.92 MHz (EU/RoW) or 315.00 MHz (North America)
 * Modulation: OOK, Fixed-width PWM (similar to Ford RKE / Microchip KEELOQ derivative)
 * Carrier: AM
 *
 * Frame structure (Land Rover Freelander 2 / Discovery 3-4 / Range Rover Sport ~2004-2013):
 *   Preamble : 20 logic-1 pulses (carrier warmup)
 *   Header   : 1 logic-1 + sync gap (~9.6 ms LOW)
 *   Payload  : 66 bits, MSB-first, fixed-width PWM
 *     [65:34] 32-bit KeeLoq encrypted hopping code
 *     [33:18] 16-bit fixed serial number (high word)
 *     [17:10]  8-bit fixed serial (low byte)
 *     [9:6]    4-bit button code
 *              0x1=Lock, 0x2=Unlock, 0x4=Boot/Tailgate, 0x8=Panic
 *     [5:2]    4-bit function bits (repeat count / battery low flags)
 *     [1:0]    2-bit status (0x1=battery low, 0x2=repeat)
 *   Repeated up to 4 times
 *
 * PWM timing (from firmware, FUN_000007cc + FUN_00000840 timer init):
 *   Bit period : 1000 µs
 *   Bit-1      :  700 µs HIGH + 300 µs LOW
 *   Bit-0      :  300 µs HIGH + 700 µs LOW
 *   Preamble pulse: 400 µs HIGH + 600 µs LOW
 *   Sync gap   :   400 µs HIGH + 9600 µs LOW
 *   Tolerance  : ±20%
 *
 * KeeLoq note:
 *   The hopping code is encrypted with KeeLoq (Microchip HCS-series algorithm).
 *   Full decryption requires the manufacturer key (not in the firmware binary —
 *   it's provisioned per fob). This file implements the protocol framing layer;
 *   a separate keeloq.c provides the cipher if you have the key.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Timing constants (microseconds)
 * ------------------------------------------------------------------------- */
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

/* Button codes (bits [9:6]) */
#define LR_BTN_LOCK             0x1u
#define LR_BTN_UNLOCK           0x2u
#define LR_BTN_BOOT             0x4u
#define LR_BTN_PANIC            0x8u

/* -------------------------------------------------------------------------
 * Data types
 * ------------------------------------------------------------------------- */

/**
 * Decoded Land Rover RKE frame.
 * The hop_code is the raw 32-bit KeeLoq ciphertext — decrypt separately.
 */
typedef struct {
    uint32_t hop_code;      /**< 32-bit KeeLoq encrypted hopping word       */
    uint32_t serial;        /**< 24-bit fixed serial number (bits [33:10])  */
    uint8_t  button;        /**< 4-bit button code (LR_BTN_*)               */
    uint8_t  func_bits;     /**< 4-bit function/repeat flags                */
    uint8_t  status;        /**< 2-bit status byte                          */
    bool     valid;         /**< true if frame geometry is correct          */
} LandRoverFrame;

/** Raw pulse buffer */
typedef struct {
    int32_t  pulses[512];
    uint32_t count;
} LandRoverRawBuf;

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------
 * Encode
 * ------------------------------------------------------------------------- */

/**
 * lr_encode() — encode a LandRoverFrame into a Flipper SubGHz RAW buffer.
 *
 * The hop_code field must already be KeeLoq-encrypted by the caller.
 * Emits LR_REPEAT_COUNT repetitions.
 */
void lr_encode(const LandRoverFrame *frame, LandRoverRawBuf *buf)
{
    buf->count = 0;

    /* Pack the 66-bit payload into a uint8_t array, MSB-first            */
    /* Layout: [65:34]=hop_code [33:10]=serial [9:6]=button               */
    /*         [5:2]=func_bits  [1:0]=status                              */
    uint8_t bits[66];
    memset(bits, 0, sizeof(bits));

    /* hop_code: bits 65..34 */
    for (int i = 0; i < 32; i++) {
        bits[65 - i] = (frame->hop_code >> i) & 1u;
    }
    /* serial: bits 33..10 (24 bits) */
    for (int i = 0; i < 24; i++) {
        bits[33 - i] = (frame->serial >> i) & 1u;
    }
    /* button: bits 9..6 */
    for (int i = 0; i < 4; i++) {
        bits[9 - i] = (frame->button >> i) & 1u;
    }
    /* func_bits: bits 5..2 */
    for (int i = 0; i < 4; i++) {
        bits[5 - i] = (frame->func_bits >> i) & 1u;
    }
    /* status: bits 1..0 */
    bits[1] = (frame->status >> 1) & 1u;
    bits[0] =  frame->status       & 1u;

    for (uint32_t rep = 0; rep < LR_REPEAT_COUNT; rep++) {
        /* Preamble: 20 pulses */
        for (uint32_t p = 0; p < LR_PREAMBLE_COUNT; p++) {
            lr_push_pair(buf, LR_PREAMBLE_HIGH_US, LR_PREAMBLE_LOW_US);
        }
        /* Sync */
        lr_push_pair(buf, LR_SYNC_HIGH_US, LR_SYNC_LOW_US);

        /* Data bits, MSB-first (bit 65 first on air) */
        for (int b = 65; b >= 0; b--) {
            if (bits[b]) {
                lr_push_pair(buf, LR_BIT1_HIGH_US, LR_BIT1_LOW_US);
            } else {
                lr_push_pair(buf, LR_BIT0_HIGH_US, LR_BIT0_LOW_US);
            }
        }

        /* Inter-repetition gap */
        if (rep < LR_REPEAT_COUNT - 1) {
            lr_push(buf, -(int32_t)LR_REPEAT_GAP_US);
        }
    }
}

/* -------------------------------------------------------------------------
 * Decode
 * ------------------------------------------------------------------------- */

/**
 * lr_decode() — decode a raw pulse buffer into a LandRoverFrame.
 *
 * Returns true if a geometrically valid frame was found (preamble + sync +
 * 66 bits all within timing tolerance). The hop_code will need KeeLoq
 * decryption by the caller to verify authenticity.
 */
bool lr_decode(const LandRoverRawBuf *buf, LandRoverFrame *frame)
{
    memset(frame, 0, sizeof(*frame));

    for (uint32_t i = 0; i + 1 < buf->count; i++) {
        /* Look for sync: ~400 µs HIGH + ~9600 µs LOW */
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

        /* Unpack — MSB of each field is highest-indexed bit */
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

/* -------------------------------------------------------------------------
 * KeeLoq stub
 *
 * Land Rover uses a KeeLoq-derived hopping code (same baseband as Ford/Jaguar,
 * firmware case 0x0E dispatches both via the same path before branching on
 * the serial-number range).
 *
 * To decrypt hop_code you need the 64-bit manufacturer key. Implement
 * keeloq_decrypt() in a separate keeloq.c (standard NLF cipher, widely
 * documented in Microchip AN-66265).
 *
 * extern uint32_t keeloq_decrypt(uint32_t ciphertext, uint64_t key);
 *
 * Typical Land Rover key derivation (normal learning, from public research):
 *   uint64_t man_key = LR_MANUFACTURER_KEY;  // provisioned, not in firmware
 *   uint64_t dev_key = keeloq_learn_normal(man_key, serial);
 *   uint32_t plain   = keeloq_decrypt(frame.hop_code, dev_key);
 *   // plain[15:0]  = 16-bit counter
 *   // plain[19:16] = button code (must match frame.button)
 *   // plain[31:28] = discriminant (0x6 for LR)
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * Rolling counter validation
 * Land Rover receivers accept counter in window [last+1, last+32768]
 * ------------------------------------------------------------------------- */
bool lr_counter_valid(uint16_t stored, uint16_t received)
{
    uint16_t delta = (uint16_t)(received - stored);
    return (delta >= 1u && delta <= 32768u);
}

/* -------------------------------------------------------------------------
 * Flipper Zero SubGHz plugin glue — same pattern as honda_rke.c
 *
 *   void flipper_lr_encode(SubGhzProtocolEncoder *enc, void *ctx) {
 *       LandRoverFrame *f = (LandRoverFrame *)ctx;
 *       LandRoverRawBuf raw;
 *       lr_encode(f, &raw);
 *       // feed raw to SubGHz RAW transmit
 *   }
 *
 *   bool flipper_lr_decode(SubGhzProtocolDecoder *dec,
 *                          const int32_t *pulses, uint32_t count, void *ctx) {
 *       LandRoverRawBuf buf;
 *       buf.count = count > 512 ? 512 : count;
 *       memcpy(buf.pulses, pulses, buf.count * sizeof(int32_t));
 *       LandRoverFrame frame;
 *       return lr_decode(&buf, &frame);
 *   }
 * ------------------------------------------------------------------------- */
