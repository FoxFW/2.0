#pragma once
/**
 * auto_rke_protocols.h
 * Additional automotive RKE protocols — Pandora DXL 5000 → Flipper Zero port
 *
 * Protocols:
 *   - Subaru          (ID 0x06) | 433.92 MHz | 48-bit  | OOK PWM
 *   - Hyundai/KiaRIO  (ID 0x11) | 433.92 MHz | 64-bit  | OOK PWM
 *   - Mazda Siemens   (ID 0x15) | 433.92 MHz | 72-bit  | OOK PWM
 *   - VAG -2004       (ID 0x19) | 433.92 MHz | 64-bit  | OOK PWM
 *   - SantaFe 13-16   (ID 0x1A) | 433.92 MHz | 80-bit  | OOK PWM
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Shared raw pulse buffer
 * All encode functions write into RawBuf.
 * All decode functions read from RawBuf.
 * positive value = HIGH duration in µs
 * negative value = LOW  duration in µs
 * ========================================================================= */
typedef struct {
    int32_t  pulses[512];
    uint32_t count;
} RawBuf;

/* =========================================================================
 * 1. SUBARU  (firmware ID 0x06)
 * ========================================================================= */

#define SUBARU_FREQ_HZ       433920000ul
#define SUBARU_BITS               48u
#define SUBARU_REPEAT              3u
#define SUBARU_SYNC_US          8000u
#define SUBARU_BIT1_HI_US        600u
#define SUBARU_BIT1_LO_US        200u
#define SUBARU_BIT0_HI_US        200u
#define SUBARU_BIT0_LO_US        600u
#define SUBARU_TOL_PCT            15u

#define SUBARU_BTN_LOCK     0x1u
#define SUBARU_BTN_UNLOCK   0x2u
#define SUBARU_BTN_TRUNK    0x4u
#define SUBARU_BTN_PANIC    0x8u

/** Subaru RKE frame (Impreza/Forester/Legacy ~2000-2010) */
typedef struct {
    uint32_t fixed_id;   /**< 32-bit fixed fob ID                          */
    uint8_t  counter;    /**< 8-bit rolling counter                        */
    uint8_t  button;     /**< SUBARU_BTN_*                                 */
    bool     valid;      /**< true after decode if checksum matched        */
} SubaruFrame;

void subaru_encode(const SubaruFrame *frame, RawBuf *buf);
bool subaru_decode(const RawBuf *buf, SubaruFrame *frame);

/* =========================================================================
 * 2. HYUNDAI / KIA RIO  (firmware ID 0x11)
 * ========================================================================= */

#define HKR_FREQ_HZ          433920000ul
#define HKR_BITS                  64u
#define HKR_REPEAT                 3u
#define HKR_SYNC_HI_US           312u
#define HKR_SYNC_LO_US         10400u
#define HKR_BIT1_HI_US           728u
#define HKR_BIT1_LO_US           312u
#define HKR_BIT0_HI_US           312u
#define HKR_BIT0_LO_US           728u
#define HKR_GAP_US             10000u
#define HKR_TOL_PCT               15u

#define HKR_BTN_LOCK        0x0100u
#define HKR_BTN_UNLOCK      0x0200u
#define HKR_BTN_TRUNK       0x0400u
#define HKR_BTN_PANIC       0x0800u

/** Hyundai/Kia RIO RKE frame (Accent/Rio/Elantra ~2001-2008, fixed code) */
typedef struct {
    uint32_t serial;       /**< 32-bit fixed serial                        */
    uint16_t button_mask;  /**< HKR_BTN_* bitmask                          */
    bool     valid;        /**< true after decode if checksum matched      */
} HKRFrame;

void hkr_encode(const HKRFrame *frame, RawBuf *buf);
bool hkr_decode(const RawBuf *buf, HKRFrame *frame);

/* =========================================================================
 * 3. MAZDA SIEMENS  (firmware ID 0x15)
 * ========================================================================= */

#define MAZ_FREQ_HZ          433920000ul
#define MAZ_BITS                  72u
#define MAZ_REPEAT                 2u
#define MAZ_SYNC_HI_US           450u
#define MAZ_SYNC_LO_US         14400u
#define MAZ_BIT1_HI_US           450u
#define MAZ_BIT1_LO_US          1350u
#define MAZ_BIT0_HI_US           450u
#define MAZ_BIT0_LO_US           450u
#define MAZ_GAP_US             20000u
#define MAZ_TOL_PCT               15u

#define MAZ_BTN_LOCK        0x1u
#define MAZ_BTN_UNLOCK      0x2u
#define MAZ_BTN_TRUNK       0x4u

/**
 * Mazda Siemens VDO RKE frame (Mazda 3/6/CX-7 ~2003-2009).
 * hop is the raw Siemens ciphertext — inner cipher is proprietary.
 */
typedef struct {
    uint32_t hop;        /**< 32-bit Siemens encrypted hopping word        */
    uint32_t serial;     /**< 24-bit fixed serial                          */
    uint8_t  counter;    /**< 8-bit rolling counter                        */
    uint8_t  button;     /**< MAZ_BTN_*                                    */
    bool     valid;      /**< true after decode if checksum matched        */
} MazdaFrame;

void mazda_encode(const MazdaFrame *frame, RawBuf *buf);
bool mazda_decode(const RawBuf *buf, MazdaFrame *frame);

/* =========================================================================
 * 4. VAG -2004  (firmware ID 0x19)
 * ========================================================================= */

#define VAG_FREQ_HZ          433920000ul
#define VAG_BITS                  64u
#define VAG_REPEAT                 3u
#define VAG_SYNC_HI_US           550u
#define VAG_SYNC_LO_US         11000u
#define VAG_BIT1_HI_US           550u
#define VAG_BIT1_LO_US           250u
#define VAG_BIT0_HI_US           250u
#define VAG_BIT0_LO_US           550u
#define VAG_GAP_US              9000u
#define VAG_TOL_PCT               15u

#define VAG_BTN_LOCK        0x01u
#define VAG_BTN_UNLOCK      0x02u
#define VAG_BTN_TRUNK       0x04u
#define VAG_BTN_PANIC       0x08u

/** VW/Audi/Seat/Skoda pre-2004 RKE frame */
typedef struct {
    uint32_t transponder_id;  /**< 32-bit fixed transponder ID             */
    uint16_t counter;         /**< 16-bit rolling counter                  */
    uint8_t  button;          /**< VAG_BTN_*                               */
    bool     valid;           /**< true after decode if checksum matched   */
} VAGFrame;

void vag_encode(const VAGFrame *frame, RawBuf *buf);
bool vag_decode(const RawBuf *buf, VAGFrame *frame);

/** Counter window validation — VAG accepts [stored+1, stored+255] */
static inline bool vag_counter_valid(uint16_t stored, uint16_t received) {
    uint16_t delta = (uint16_t)(received - stored);
    return (delta >= 1u && delta <= 255u);
}

/* =========================================================================
 * 5. HYUNDAI SANTA FE 2013-2016  (firmware ID 0x1A)
 * ========================================================================= */

#define SFE_FREQ_HZ          433920000ul
#define SFE_BITS                  80u
#define SFE_REPEAT                 3u
#define SFE_SYNC_HI_US           375u
#define SFE_SYNC_LO_US         12000u
#define SFE_BIT1_HI_US           375u
#define SFE_BIT1_LO_US           125u
#define SFE_BIT0_HI_US           125u
#define SFE_BIT0_LO_US           375u
#define SFE_GAP_US             15000u
#define SFE_TOL_PCT               15u

#define SFE_BTN_LOCK        0x01u
#define SFE_BTN_UNLOCK      0x02u
#define SFE_BTN_TRUNK       0x04u
#define SFE_BTN_PANIC       0x08u

/**
 * Hyundai Santa Fe / Solaris RKE frame (TRW fob ~2013-2016).
 * rolling is Hitag2-derived ciphertext.
 */
typedef struct {
    uint32_t rolling;    /**< 32-bit Hitag2-derived encrypted word         */
    uint32_t serial;     /**< 24-bit fixed serial                          */
    uint8_t  counter;    /**< 8-bit rolling counter                        */
    uint8_t  button;     /**< SFE_BTN_*                                    */
    bool     valid;      /**< true after decode if CRC8 matched            */
} SantaFeFrame;

void santafe_encode(const SantaFeFrame *frame, RawBuf *buf);
bool santafe_decode(const RawBuf *buf, SantaFeFrame *frame);

/** Counter window validation — SantaFe accepts [stored+1, stored+32] */
static inline bool santafe_counter_valid(uint8_t stored, uint8_t received) {
    uint8_t delta = (uint8_t)(received - stored);
    return (delta >= 1u && delta <= 32u);
}

#ifdef __cplusplus
}
#endif
