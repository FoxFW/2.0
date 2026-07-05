/**
 * auto_rke_protocols.c
 * Additional automotive RKE protocols — ported from Pandora DXL 5000 firmware
 * Target: Flipper Zero
 *
 * Protocols included (all found in firmware string table 0x0000ecc4-0x0000ee00):
 *   - Subaru         (ID 0x06, 433.92 MHz)
 *   - Hyundai/KiaRIO (ID 0x11, 433.92 MHz)
 *   - Mazda Siemens  (ID 0x15, 433.92 MHz)
 *   - VAG -2004      (ID 0x19, 433.92 MHz)  [VW/Audi/Seat/Skoda pre-2004]
 *   - SantaFe 13-16  (ID 0x1A, 433.92 MHz)  [Hyundai Santa Fe 2013-2016]
 *
 * All use OOK AM modulation. Timing constants extracted from firmware
 * FUN_000007cc (period calculator) and FUN_00000840 (timer init).
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* =========================================================================
 * Common helpers
 * ========================================================================= */

typedef struct {
    int32_t  pulses[512];
    uint32_t count;
} RawBuf;

static void raw_push(RawBuf *b, int32_t v)
{
    if (b->count < 512) b->pulses[b->count++] = v;
}
static void raw_pair(RawBuf *b, uint32_t hi, uint32_t lo)
{
    raw_push(b,  (int32_t)hi);
    raw_push(b, -(int32_t)lo);
}
static bool in_range(int32_t m, uint32_t ref, uint32_t tol_pct)
{
    int32_t r = (int32_t)ref, d = m - r;
    if (d < 0) d = -d;
    return (d * 100) <= (r * (int32_t)tol_pct);
}

/* =========================================================================
 * 1. SUBARU RKE  (firmware case 0x06, protocol type 6, iVar7=0x18)
 *
 * Subaru legacy fob (Impreza/Forester/Legacy ~2000-2010):
 *   Freq  : 433.92 MHz
 *   Bits  : 48 (LSB-first)
 *   Layout: [47:16] 32-bit fixed ID  [15:8] rolling counter  [7:0] button+cksum
 *   PWM   : period 800 µs; 1 = 600 µs HI + 200 µs LO; 0 = 200 µs HI + 600 µs LO
 *   Sync  : 8000 µs LOW preamble, then 600 µs HI start-bit
 *   Repeat: 3×
 * ========================================================================= */

#define SUBARU_FREQ_HZ    433920000ul
#define SUBARU_BITS            48u
#define SUBARU_REPEAT           3u
#define SUBARU_SYNC_US       8000u
#define SUBARU_TOL_PCT         15u
#define SUBARU_BIT1_HI_US     600u
#define SUBARU_BIT1_LO_US     200u
#define SUBARU_BIT0_HI_US     200u
#define SUBARU_BIT0_LO_US     600u

#define SUBARU_BTN_LOCK       0x1u
#define SUBARU_BTN_UNLOCK     0x2u
#define SUBARU_BTN_TRUNK      0x4u
#define SUBARU_BTN_PANIC      0x8u

typedef struct {
    uint32_t fixed_id;
    uint8_t  counter;
    uint8_t  button;
    bool     valid;
} SubaruFrame;

static uint8_t subaru_cksum(uint32_t id, uint8_t ctr, uint8_t btn)
{
    /* Simple nibble-XOR checksum (derived from analysis of firmware data loop) */
    uint8_t c = 0;
    for (int i = 0; i < 4; i++) c ^= (id >> (i * 8)) & 0xFFu;
    c ^= ctr ^ (btn & 0xFu);
    return c & 0xFFu;
}

void subaru_encode(const SubaruFrame *f, RawBuf *buf)
{
    buf->count = 0;
    uint8_t ck = subaru_cksum(f->fixed_id, f->counter, f->button);

    /* Pack 48 bits LSB-first: fixed_id[31:0] | counter[7:0] | (btn<<4)|ck */
    uint64_t word = (uint64_t)f->fixed_id |
                    ((uint64_t)f->counter  << 32) |
                    ((uint64_t)((f->button << 4) | ck) << 40);

    for (uint32_t rep = 0; rep < SUBARU_REPEAT; rep++) {
        /* Sync gap then start burst */
        raw_push(buf, -(int32_t)SUBARU_SYNC_US);
        raw_pair(buf, SUBARU_BIT1_HI_US, SUBARU_BIT1_LO_US); /* start bit=1 */

        for (uint32_t b = 0; b < SUBARU_BITS; b++) {
            bool bit = (word >> b) & 1u;
            raw_pair(buf,
                bit ? SUBARU_BIT1_HI_US : SUBARU_BIT0_HI_US,
                bit ? SUBARU_BIT1_LO_US : SUBARU_BIT0_LO_US);
        }
    }
}

bool subaru_decode(const RawBuf *buf, SubaruFrame *frame)
{
    memset(frame, 0, sizeof(*frame));
    for (uint32_t i = 0; i + 1 < buf->count; i++) {
        /* Sync: long LOW */
        if (!in_range(-buf->pulses[i], SUBARU_SYNC_US, SUBARU_TOL_PCT)) continue;
        /* Start bit: long HI */
        if (i + 1 >= buf->count) continue;
        if (!in_range(buf->pulses[i + 1], SUBARU_BIT1_HI_US, SUBARU_TOL_PCT)) continue;

        uint32_t j = i + 2; /* skip LOW half of start bit */
        if (j + 1 >= buf->count) continue;
        j++; /* skip the LOW portion already in pair */
        if (j + SUBARU_BITS * 2 > buf->count) continue;

        uint64_t word = 0;
        bool ok = true;
        for (uint32_t b = 0; b < SUBARU_BITS; b++) {
            int32_t hi =  buf->pulses[j];
            int32_t lo = -buf->pulses[j + 1];
            j += 2;
            if (in_range(hi, SUBARU_BIT1_HI_US, SUBARU_TOL_PCT) &&
                in_range(lo, SUBARU_BIT1_LO_US, SUBARU_TOL_PCT)) {
                word |= (uint64_t)1 << b;
            } else if (in_range(hi, SUBARU_BIT0_HI_US, SUBARU_TOL_PCT) &&
                       in_range(lo, SUBARU_BIT0_LO_US, SUBARU_TOL_PCT)) {
                /* bit 0 */
            } else { ok = false; break; }
        }
        if (!ok) continue;

        frame->fixed_id = (uint32_t)(word & 0xFFFFFFFFu);
        frame->counter  = (uint8_t)((word >> 32) & 0xFFu);
        frame->button   = (uint8_t)((word >> 44) & 0xFu);
        uint8_t rx_ck   = (uint8_t)((word >> 40) & 0xFu);
        frame->valid    = (rx_ck == (subaru_cksum(frame->fixed_id, frame->counter, frame->button) & 0xFu));
        if (frame->valid) return true;
    }
    return false;
}

/* =========================================================================
 * 2. HYUNDAI / KIA RIO  (firmware case 0x11, iVar7=1, 14-bit display)
 *
 * Early Hyundai/Kia fobs (Accent, Rio, Elantra ~2001-2008):
 *   Freq  : 433.92 MHz
 *   Bits  : 64 (MSB-first), plain fixed-code (no rolling — very old fobs)
 *   Layout: [63:32] 32-bit serial  [31:16] 16-bit button mask repeated
 *            [15:0] ~16-bit checksum (XOR block)
 *   PWM   : period 1040 µs; 1 = 728 µs HI + 312 µs LO; 0 = 312 µs HI + 728 µs LO
 *   Sync  : 312 µs HI + 10400 µs LO
 *   Repeat: 3×
 * ========================================================================= */

#define HKR_BITS          64u
#define HKR_REPEAT         3u
#define HKR_SYNC_HI_US   312u
#define HKR_SYNC_LO_US  10400u
#define HKR_BIT1_HI_US   728u
#define HKR_BIT1_LO_US   312u
#define HKR_BIT0_HI_US   312u
#define HKR_BIT0_LO_US   728u
#define HKR_TOL_PCT       15u
#define HKR_GAP_US      10000u

#define HKR_BTN_LOCK     0x0100u
#define HKR_BTN_UNLOCK   0x0200u
#define HKR_BTN_TRUNK    0x0400u
#define HKR_BTN_PANIC    0x0800u

typedef struct {
    uint32_t serial;
    uint16_t button_mask;
    bool     valid;
} HKRFrame;

static uint16_t hkr_cksum(uint32_t serial, uint16_t btn)
{
    uint16_t c = (uint16_t)(serial ^ (serial >> 16));
    c ^= btn;
    return ~c;
}

void hkr_encode(const HKRFrame *f, RawBuf *buf)
{
    buf->count = 0;
    uint16_t ck = hkr_cksum(f->serial, f->button_mask);
    uint64_t word = ((uint64_t)f->serial      << 32) |
                    ((uint64_t)f->button_mask  << 16) |
                    (uint64_t)ck;

    for (uint32_t rep = 0; rep < HKR_REPEAT; rep++) {
        raw_pair(buf, HKR_SYNC_HI_US, HKR_SYNC_LO_US);
        for (int b = 63; b >= 0; b--) {
            bool bit = (word >> b) & 1u;
            raw_pair(buf,
                bit ? HKR_BIT1_HI_US : HKR_BIT0_HI_US,
                bit ? HKR_BIT1_LO_US : HKR_BIT0_LO_US);
        }
        raw_push(buf, -(int32_t)HKR_GAP_US);
    }
}

bool hkr_decode(const RawBuf *buf, HKRFrame *frame)
{
    memset(frame, 0, sizeof(*frame));
    for (uint32_t i = 0; i + 1 < buf->count; i++) {
        if (!in_range( buf->pulses[i],     HKR_SYNC_HI_US, HKR_TOL_PCT)) continue;
        if (!in_range(-buf->pulses[i + 1], HKR_SYNC_LO_US, HKR_TOL_PCT)) continue;
        uint32_t j = i + 2;
        if (j + HKR_BITS * 2 > buf->count) continue;
        uint64_t word = 0;
        bool ok = true;
        for (int b = 63; b >= 0; b--) {
            int32_t hi =  buf->pulses[j];
            int32_t lo = -buf->pulses[j + 1];
            j += 2;
            if      (in_range(hi, HKR_BIT1_HI_US, HKR_TOL_PCT) && in_range(lo, HKR_BIT1_LO_US, HKR_TOL_PCT)) word |= (uint64_t)1 << b;
            else if (in_range(hi, HKR_BIT0_HI_US, HKR_TOL_PCT) && in_range(lo, HKR_BIT0_LO_US, HKR_TOL_PCT)) { /* 0 */ }
            else { ok = false; break; }
        }
        if (!ok) continue;
        frame->serial      = (uint32_t)(word >> 32);
        frame->button_mask = (uint16_t)((word >> 16) & 0xFFFFu);
        uint16_t rx_ck     = (uint16_t)(word & 0xFFFFu);
        frame->valid       = (rx_ck == hkr_cksum(frame->serial, frame->button_mask));
        if (frame->valid) return true;
    }
    return false;
}

/* =========================================================================
 * 3. MAZDA SIEMENS RKE  (firmware case 0x15, iVar7=5, 13-bit display)
 *
 * Mazda 3/6/CX-7 with Siemens VDO fob (~2003-2009):
 *   Freq  : 433.92 MHz
 *   Bits  : 72 (MSB-first), Siemens rolling code
 *   Layout: [71:40] 32-bit hop (Siemens proprietary cipher)
 *            [39:16] 24-bit serial
 *            [15:8]  8-bit counter (low byte)
 *            [7:4]   4-bit button  [3:0] 4-bit checksum
 *   PWM   : 1 = 450 µs HI + 1350 µs LO; 0 = 450 µs HI + 450 µs LO
 *   Sync  : 450 µs HI + 14400 µs LO
 *   Repeat: 2×
 * ========================================================================= */

#define MAZ_BITS          72u
#define MAZ_REPEAT         2u
#define MAZ_SYNC_HI_US   450u
#define MAZ_SYNC_LO_US  14400u
#define MAZ_BIT1_HI_US   450u
#define MAZ_BIT1_LO_US  1350u
#define MAZ_BIT0_HI_US   450u
#define MAZ_BIT0_LO_US   450u
#define MAZ_GAP_US      20000u
#define MAZ_TOL_PCT        15u

#define MAZ_BTN_LOCK    0x1u
#define MAZ_BTN_UNLOCK  0x2u
#define MAZ_BTN_TRUNK   0x4u

typedef struct {
    uint32_t hop;       /* Siemens encrypted hopping word — decrypt separately */
    uint32_t serial;    /* 24-bit */
    uint8_t  counter;
    uint8_t  button;
    bool     valid;
} MazdaFrame;

static uint8_t maz_cksum(uint32_t hop, uint32_t serial, uint8_t ctr, uint8_t btn)
{
    uint8_t c = 0;
    for (int i = 0; i < 4; i++) c ^= (hop    >> (i * 8)) & 0xFFu;
    for (int i = 0; i < 3; i++) c ^= (serial >> (i * 8)) & 0xFFu;
    c ^= ctr ^ (btn & 0xFu);
    return c & 0xFu;
}

void mazda_encode(const MazdaFrame *f, RawBuf *buf)
{
    buf->count = 0;
    uint8_t ck = maz_cksum(f->hop, f->serial, f->counter, f->button);
    /* Pack 72 bits into 9 bytes, MSB of hop is bit 71 */
    uint8_t pkt[9];
    pkt[0] = (f->hop >> 24) & 0xFFu;
    pkt[1] = (f->hop >> 16) & 0xFFu;
    pkt[2] = (f->hop >>  8) & 0xFFu;
    pkt[3] =  f->hop        & 0xFFu;
    pkt[4] = (f->serial >> 16) & 0xFFu;
    pkt[5] = (f->serial >>  8) & 0xFFu;
    pkt[6] =  f->serial        & 0xFFu;
    pkt[7] = f->counter;
    pkt[8] = (uint8_t)((f->button << 4) | ck);

    for (uint32_t rep = 0; rep < MAZ_REPEAT; rep++) {
        raw_pair(buf, MAZ_SYNC_HI_US, MAZ_SYNC_LO_US);
        for (int byte = 0; byte < 9; byte++) {
            for (int bit = 7; bit >= 0; bit--) {
                bool b = (pkt[byte] >> bit) & 1u;
                raw_pair(buf,
                    b ? MAZ_BIT1_HI_US : MAZ_BIT0_HI_US,
                    b ? MAZ_BIT1_LO_US : MAZ_BIT0_LO_US);
            }
        }
        raw_push(buf, -(int32_t)MAZ_GAP_US);
    }
}

bool mazda_decode(const RawBuf *buf, MazdaFrame *frame)
{
    memset(frame, 0, sizeof(*frame));
    for (uint32_t i = 0; i + 1 < buf->count; i++) {
        if (!in_range( buf->pulses[i],     MAZ_SYNC_HI_US, MAZ_TOL_PCT)) continue;
        if (!in_range(-buf->pulses[i + 1], MAZ_SYNC_LO_US, MAZ_TOL_PCT)) continue;
        uint32_t j = i + 2;
        if (j + MAZ_BITS * 2 > buf->count) continue;
        uint8_t pkt[9] = {0};
        bool ok = true;
        for (uint32_t bt = 0; bt < MAZ_BITS; bt++) {
            int32_t hi =  buf->pulses[j];
            int32_t lo = -buf->pulses[j + 1];
            j += 2;
            bool b;
            if      (in_range(lo, MAZ_BIT1_LO_US, MAZ_TOL_PCT)) b = true;
            else if (in_range(lo, MAZ_BIT0_LO_US, MAZ_TOL_PCT)) b = false;
            else { ok = false; break; }
            (void)hi;
            if (b) pkt[bt / 8] |= (uint8_t)(1u << (7 - (bt % 8)));
        }
        if (!ok) continue;
        frame->hop     = ((uint32_t)pkt[0]<<24)|((uint32_t)pkt[1]<<16)|((uint32_t)pkt[2]<<8)|pkt[3];
        frame->serial  = ((uint32_t)pkt[4]<<16)|((uint32_t)pkt[5]<<8)|pkt[6];
        frame->counter = pkt[7];
        frame->button  = pkt[8] >> 4;
        uint8_t rx_ck  = pkt[8] & 0xFu;
        frame->valid   = (rx_ck == maz_cksum(frame->hop, frame->serial, frame->counter, frame->button));
        if (frame->valid) return true;
    }
    return false;
}

/* =========================================================================
 * 4. VAG -2004  (firmware case 0x19, Princeton-style, ID 0x19)
 *
 * VW/Audi/Seat/Skoda fobs before 2004 (ID48 era, 3-button):
 *   Freq  : 433.92 MHz
 *   Bits  : 64 (MSB-first), simple rolling code (16-bit counter)
 *   Layout: [63:32] 32-bit fixed transponder ID
 *            [31:16] 16-bit counter
 *            [15:8]  8-bit button+flags
 *            [7:0]   8-bit checksum (sum of all prior bytes mod 256, inverted)
 *   PWM   : period 800 µs; 1 = 550 µs HI + 250 µs LO; 0 = 250 µs HI + 550 µs LO
 *   Sync  : 550 µs HI + 11000 µs LO
 *   Repeat: 3×
 * ========================================================================= */

#define VAG_BITS          64u
#define VAG_REPEAT         3u
#define VAG_SYNC_HI_US   550u
#define VAG_SYNC_LO_US  11000u
#define VAG_BIT1_HI_US   550u
#define VAG_BIT1_LO_US   250u
#define VAG_BIT0_HI_US   250u
#define VAG_BIT0_LO_US   550u
#define VAG_GAP_US       9000u
#define VAG_TOL_PCT        15u

#define VAG_BTN_LOCK    0x01u
#define VAG_BTN_UNLOCK  0x02u
#define VAG_BTN_TRUNK   0x04u
#define VAG_BTN_PANIC   0x08u

typedef struct {
    uint32_t transponder_id;
    uint16_t counter;
    uint8_t  button;
    bool     valid;
} VAGFrame;

static uint8_t vag_cksum(uint32_t tid, uint16_t ctr, uint8_t btn)
{
    uint8_t s = 0;
    s += (tid >> 24) & 0xFFu;
    s += (tid >> 16) & 0xFFu;
    s += (tid >>  8) & 0xFFu;
    s +=  tid        & 0xFFu;
    s += (ctr >>  8) & 0xFFu;
    s +=  ctr        & 0xFFu;
    s += btn;
    return (uint8_t)(~s);
}

void vag_encode(const VAGFrame *f, RawBuf *buf)
{
    buf->count = 0;
    uint8_t ck = vag_cksum(f->transponder_id, f->counter, f->button);
    uint64_t word = ((uint64_t)f->transponder_id << 32) |
                    ((uint64_t)f->counter         << 16) |
                    ((uint64_t)f->button          <<  8) |
                    ck;

    for (uint32_t rep = 0; rep < VAG_REPEAT; rep++) {
        raw_pair(buf, VAG_SYNC_HI_US, VAG_SYNC_LO_US);
        for (int b = 63; b >= 0; b--) {
            bool bit = (word >> b) & 1u;
            raw_pair(buf,
                bit ? VAG_BIT1_HI_US : VAG_BIT0_HI_US,
                bit ? VAG_BIT1_LO_US : VAG_BIT0_LO_US);
        }
        raw_push(buf, -(int32_t)VAG_GAP_US);
    }
}

bool vag_decode(const RawBuf *buf, VAGFrame *frame)
{
    memset(frame, 0, sizeof(*frame));
    for (uint32_t i = 0; i + 1 < buf->count; i++) {
        if (!in_range( buf->pulses[i],     VAG_SYNC_HI_US, VAG_TOL_PCT)) continue;
        if (!in_range(-buf->pulses[i + 1], VAG_SYNC_LO_US, VAG_TOL_PCT)) continue;
        uint32_t j = i + 2;
        if (j + VAG_BITS * 2 > buf->count) continue;
        uint64_t word = 0;
        bool ok = true;
        for (int b = 63; b >= 0; b--) {
            int32_t hi =  buf->pulses[j];
            int32_t lo = -buf->pulses[j + 1];
            j += 2;
            if      (in_range(hi, VAG_BIT1_HI_US, VAG_TOL_PCT) && in_range(lo, VAG_BIT1_LO_US, VAG_TOL_PCT)) word |= (uint64_t)1 << b;
            else if (in_range(hi, VAG_BIT0_HI_US, VAG_TOL_PCT) && in_range(lo, VAG_BIT0_LO_US, VAG_TOL_PCT)) {}
            else { ok = false; break; }
        }
        if (!ok) continue;
        frame->transponder_id = (uint32_t)(word >> 32);
        frame->counter        = (uint16_t)((word >> 16) & 0xFFFFu);
        frame->button         = (uint8_t) ((word >>  8) & 0xFFu);
        uint8_t rx_ck         = (uint8_t) (word & 0xFFu);
        frame->valid = (rx_ck == vag_cksum(frame->transponder_id, frame->counter, frame->button));
        if (frame->valid) return true;
    }
    return false;
}

/* =========================================================================
 * 5. HYUNDAI SANTA FE 2013-2016  (firmware case 0x1A, paired with HU Solaris)
 *
 * Hyundai Santa Fe / Solaris RKE (TRW fob variant):
 *   Freq  : 433.92 MHz
 *   Bits  : 80 (MSB-first)
 *   Layout: [79:48] 32-bit rolling code (Hitag2 derived)
 *            [47:24] 24-bit serial
 *            [23:16] 8-bit counter
 *            [15:8]  8-bit button flags
 *            [7:0]   8-bit CRC8 (poly 0x31, init 0xFF)
 *   PWM   : period 500 µs; 1 = 375 µs HI + 125 µs LO; 0 = 125 µs HI + 375 µs LO
 *   Sync  : 375 µs HI + 12000 µs LO
 *   Repeat: 3×
 * ========================================================================= */

#define SFE_BITS           80u
#define SFE_REPEAT          3u
#define SFE_SYNC_HI_US    375u
#define SFE_SYNC_LO_US   12000u
#define SFE_BIT1_HI_US    375u
#define SFE_BIT1_LO_US    125u
#define SFE_BIT0_HI_US    125u
#define SFE_BIT0_LO_US    375u
#define SFE_GAP_US        15000u
#define SFE_TOL_PCT          15u

#define SFE_BTN_LOCK      0x01u
#define SFE_BTN_UNLOCK    0x02u
#define SFE_BTN_TRUNK     0x04u
#define SFE_BTN_PANIC     0x08u

typedef struct {
    uint32_t rolling;   /* Hitag2-derived ciphertext — decrypt separately */
    uint32_t serial;    /* 24-bit */
    uint8_t  counter;
    uint8_t  button;
    bool     valid;
} SantaFeFrame;

static uint8_t sfe_crc8(const uint8_t *data, uint32_t len)
{
    uint8_t crc = 0xFFu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80u) crc = (uint8_t)((crc << 1) ^ 0x31u);
            else             crc = (uint8_t) (crc << 1);
        }
    }
    return crc;
}

void santafe_encode(const SantaFeFrame *f, RawBuf *buf)
{
    buf->count = 0;
    uint8_t pkt[10];
    pkt[0] = (f->rolling >> 24) & 0xFFu;
    pkt[1] = (f->rolling >> 16) & 0xFFu;
    pkt[2] = (f->rolling >>  8) & 0xFFu;
    pkt[3] =  f->rolling        & 0xFFu;
    pkt[4] = (f->serial >> 16) & 0xFFu;
    pkt[5] = (f->serial >>  8) & 0xFFu;
    pkt[6] =  f->serial        & 0xFFu;
    pkt[7] = f->counter;
    pkt[8] = f->button;
    pkt[9] = sfe_crc8(pkt, 9);

    for (uint32_t rep = 0; rep < SFE_REPEAT; rep++) {
        raw_pair(buf, SFE_SYNC_HI_US, SFE_SYNC_LO_US);
        for (int byte = 0; byte < 10; byte++) {
            for (int bit = 7; bit >= 0; bit--) {
                bool b = (pkt[byte] >> bit) & 1u;
                raw_pair(buf,
                    b ? SFE_BIT1_HI_US : SFE_BIT0_HI_US,
                    b ? SFE_BIT1_LO_US : SFE_BIT0_LO_US);
            }
        }
        raw_push(buf, -(int32_t)SFE_GAP_US);
    }
}

bool santafe_decode(const RawBuf *buf, SantaFeFrame *frame)
{
    memset(frame, 0, sizeof(*frame));
    for (uint32_t i = 0; i + 1 < buf->count; i++) {
        if (!in_range( buf->pulses[i],     SFE_SYNC_HI_US, SFE_TOL_PCT)) continue;
        if (!in_range(-buf->pulses[i + 1], SFE_SYNC_LO_US, SFE_TOL_PCT)) continue;
        uint32_t j = i + 2;
        if (j + SFE_BITS * 2 > buf->count) continue;
        uint8_t pkt[10] = {0};
        bool ok = true;
        for (uint32_t bt = 0; bt < SFE_BITS; bt++) {
            int32_t hi =  buf->pulses[j];
            int32_t lo = -buf->pulses[j + 1];
            j += 2;
            bool b;
            if      (in_range(lo, SFE_BIT1_LO_US, SFE_TOL_PCT)) b = true;
            else if (in_range(lo, SFE_BIT0_LO_US, SFE_TOL_PCT)) b = false;
            else { ok = false; break; }
            (void)hi;
            if (b) pkt[bt / 8] |= (uint8_t)(1u << (7 - (bt % 8)));
        }
        if (!ok) continue;
        frame->rolling  = ((uint32_t)pkt[0]<<24)|((uint32_t)pkt[1]<<16)|((uint32_t)pkt[2]<<8)|pkt[3];
        frame->serial   = ((uint32_t)pkt[4]<<16)|((uint32_t)pkt[5]<<8)|pkt[6];
        frame->counter  = pkt[7];
        frame->button   = pkt[8];
        uint8_t rx_crc  = pkt[9];
        frame->valid    = (rx_crc == sfe_crc8(pkt, 9));
        if (frame->valid) return true;
    }
    return false;
}
