#pragma once
/**
 * landrover_rke.h
 * Land Rover RKE protocol — Pandora DXL 5000 → Flipper Zero port
 * Protocol ID: 0x0E | 433.92 MHz / 315.00 MHz | OOK PWM | 66-bit KeeLoq frame
 *
 * NOTE: hop_code is the raw KeeLoq ciphertext. Use the existing
 *       keeloq.c in the Flipper firmware to decrypt/verify.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Timing constants (microseconds)
 * ------------------------------------------------------------------------- */
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

/* Frequency options */
#define LR_FREQ_EU_HZ    433920000ul
#define LR_FREQ_US_HZ    315000000ul

/* Button codes — bits [9:6] of frame */
#define LR_BTN_LOCK      0x1u
#define LR_BTN_UNLOCK    0x2u
#define LR_BTN_BOOT      0x4u   /**< Boot / tailgate */
#define LR_BTN_PANIC     0x8u

/* Status bits [1:0] */
#define LR_STATUS_BATTERY_LOW  0x1u
#define LR_STATUS_REPEAT       0x2u

/* -------------------------------------------------------------------------
 * Data types
 * ------------------------------------------------------------------------- */

/**
 * Land Rover RKE frame.
 * hop_code must be KeeLoq-encrypted before encode, and can be decrypted
 * after decode using subghz_protocol_keeloq_decrypt() from the Flipper firmware.
 */
typedef struct {
    uint32_t hop_code;   /**< 32-bit KeeLoq encrypted hopping word         */
    uint32_t serial;     /**< 24-bit fixed fob serial                      */
    uint8_t  button;     /**< 4-bit button code: LR_BTN_*                  */
    uint8_t  func_bits;  /**< 4-bit function/repeat flags                  */
    uint8_t  status;     /**< 2-bit status: LR_STATUS_*                    */
    bool     valid;      /**< true after decode if geometry is correct     */
} LandRoverFrame;

/** Raw pulse buffer */
typedef struct {
    int32_t  pulses[512];
    uint32_t count;
} LandRoverRawBuf;

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/**
 * Encode a LandRoverFrame into a SubGHz RAW pulse buffer.
 * hop_code must already be KeeLoq-encrypted by the caller.
 * Emits LR_REPEAT_COUNT (4) repetitions.
 */
void lr_encode(const LandRoverFrame *frame, LandRoverRawBuf *buf);

/**
 * Decode a raw pulse buffer into a LandRoverFrame.
 * Sets valid=true if frame geometry (preamble+sync+66 bits) passes timing checks.
 * Does NOT verify the KeeLoq hop code — do that separately.
 */
bool lr_decode(const LandRoverRawBuf *buf, LandRoverFrame *frame);

/**
 * Validate a received 16-bit KeeLoq counter (extracted from decrypted hop_code)
 * against the last stored value. Land Rover window: [stored+1, stored+32768].
 */
bool lr_counter_valid(uint16_t stored, uint16_t received);

/* -------------------------------------------------------------------------
 * KeeLoq integration hint
 *
 * After lr_decode() succeeds, decrypt and verify like this:
 *
 *   uint64_t dev_key = keeloq_normal_learning(manufacturer_key, frame.serial);
 *   uint32_t plain   = keeloq_decrypt(frame.hop_code, dev_key);
 *   uint16_t counter = plain & 0xFFFFu;
 *   uint8_t  btn_chk = (plain >> 16) & 0xFu;   // must equal frame.button
 *   uint8_t  disc    = (plain >> 28) & 0xFu;   // 0x6 for Land Rover
 * ------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif
