#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t  pulses[512];
    uint32_t count;
} RawBuf;

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

typedef struct {
    uint32_t fixed_id;
    uint8_t  counter;
    uint8_t  button;
    bool     valid;
} SubaruFrame;

void subaru_encode(const SubaruFrame *frame, RawBuf *buf);
bool subaru_decode(const RawBuf *buf, SubaruFrame *frame);

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

typedef struct {
    uint32_t serial;
    uint16_t button_mask;
    bool     valid;
} HKRFrame;

void hkr_encode(const HKRFrame *frame, RawBuf *buf);
bool hkr_decode(const RawBuf *buf, HKRFrame *frame);

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

typedef struct {
    uint32_t hop;
    uint32_t serial;
    uint8_t  counter;
    uint8_t  button;
    bool     valid;
} MazdaFrame;

void mazda_encode(const MazdaFrame *frame, RawBuf *buf);
bool mazda_decode(const RawBuf *buf, MazdaFrame *frame);

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

typedef struct {
    uint32_t transponder_id;
    uint16_t counter;
    uint8_t  button;
    bool     valid;
} VAGFrame;

void vag_encode(const VAGFrame *frame, RawBuf *buf);
bool vag_decode(const RawBuf *buf, VAGFrame *frame);

static inline bool vag_counter_valid(uint16_t stored, uint16_t received) {
    uint16_t delta = (uint16_t)(received - stored);
    return (delta >= 1u && delta <= 255u);
}

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

typedef struct {
    uint32_t rolling;
    uint32_t serial;
    uint8_t  counter;
    uint8_t  button;
    bool     valid;
} SantaFeFrame;

void santafe_encode(const SantaFeFrame *frame, RawBuf *buf);
bool santafe_decode(const RawBuf *buf, SantaFeFrame *frame);

static inline bool santafe_counter_valid(uint8_t stored, uint8_t received) {
    uint8_t delta = (uint8_t)(received - stored);
    return (delta >= 1u && delta <= 32u);
}

#ifdef __cplusplus
}
#endif
