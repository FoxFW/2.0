#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Frame layout, per RfidResearchGroup/ChameleonUltraDocs protocol.md:
 *
 *   SOF(1) LRC1(1) CMD(2) STATUS(2) LEN(2) LRC2(1) DATA(LEN) LRC3(1)
 *
 * SOF is always 0x11. LRC1 covers SOF only, LRC2 covers CMD|STATUS|LEN,
 * LRC3 covers DATA. All multi-byte fields are big-endian. STATUS is
 * always 0x0000 on frames sent to the device.
 */

#define CHAMELEON_FRAME_OVERHEAD 10
#define CHAMELEON_MAX_DATA       512
#define CHAMELEON_MAX_FRAME      (CHAMELEON_FRAME_OVERHEAD + CHAMELEON_MAX_DATA)

#define CHAMELEON_CMD_GET_APP_VERSION         1000
#define CHAMELEON_CMD_CHANGE_DEVICE_MODE      1001
#define CHAMELEON_CMD_GET_DEVICE_MODE         1002
#define CHAMELEON_CMD_SET_ACTIVE_SLOT         1003
#define CHAMELEON_CMD_GET_DEVICE_CHIP_ID      1011
#define CHAMELEON_CMD_GET_DEVICE_ADDRESS      1012
#define CHAMELEON_CMD_GET_GIT_VERSION         1017
#define CHAMELEON_CMD_GET_ACTIVE_SLOT         1018
#define CHAMELEON_CMD_GET_ENABLED_SLOTS       1023
#define CHAMELEON_CMD_GET_BATTERY_INFO        1025
#define CHAMELEON_CMD_GET_DEVICE_MODEL        1033
#define CHAMELEON_CMD_HF14A_SCAN              2000
#define CHAMELEON_CMD_MF1_DETECT_SUPPORT      2001
#define CHAMELEON_CMD_MF1_READ_ONE_BLOCK      2008
#define CHAMELEON_CMD_MF1_READ_EMU_BLOCK_DATA 4008

typedef struct {
    uint16_t cmd;
    uint16_t status;
    uint16_t data_len;
    const uint8_t* data;
} ChameleonFrame;

/* Distinguishes *why* chameleon_parse_frame_ex() rejected a buffer, so a
   caller logging a failure (e.g. the Terminal log) can say something more
   useful than "malformed" - genuinely different problems (garbled bytes
   vs. a real LRC bug vs. a truncated BLE notification) look different
   here and should look different in the log too. */
typedef enum {
    ChameleonFrameOk,
    ChameleonFrameErrTooShort,        /* length < CHAMELEON_FRAME_OVERHEAD */
    ChameleonFrameErrBadSof,          /* buffer[0] != 0x11 */
    ChameleonFrameErrBadLrc1,         /* buffer[1] != LRC(SOF) */
    ChameleonFrameErrBadLrc2,         /* buffer[8] != LRC(CMD|STATUS|LEN) */
    ChameleonFrameErrLenExceedsBuffer, /* declared LEN needs more bytes than arrived - e.g. a fragmented BLE notification */
    ChameleonFrameErrBadLrc3,         /* trailing byte != LRC(DATA) */
} ChameleonFrameError;

/* Short, human-readable string for error_out from chameleon_parse_frame_ex() - for logs, not for control flow. */
const char* chameleon_frame_error_str(ChameleonFrameError error);

/* Writes a command frame into out. Returns the frame length, or 0 if
   out_capacity is too small or data_len exceeds CHAMELEON_MAX_DATA. */
size_t chameleon_build_frame(
    uint16_t cmd,
    const uint8_t* data,
    uint16_t data_len,
    uint8_t* out,
    size_t out_capacity);

/* Validates SOF and all three LRCs before filling out. On success,
   out->data points into buffer - buffer must outlive out. error_out may
   be NULL if the caller doesn't care why a failure happened. */
bool chameleon_parse_frame_ex(
    const uint8_t* buffer,
    size_t length,
    ChameleonFrame* out,
    ChameleonFrameError* error_out);

/* Convenience wrapper over chameleon_parse_frame_ex() for callers that
   only care whether parsing succeeded. */
bool chameleon_parse_frame(const uint8_t* buffer, size_t length, ChameleonFrame* out);

size_t chameleon_build_get_app_version(uint8_t* out, size_t out_capacity);
size_t chameleon_build_get_git_version(uint8_t* out, size_t out_capacity);
size_t chameleon_build_get_battery_info(uint8_t* out, size_t out_capacity);
size_t chameleon_build_get_active_slot(uint8_t* out, size_t out_capacity);
size_t chameleon_build_get_device_model(uint8_t* out, size_t out_capacity);
size_t chameleon_build_get_device_chip_id(uint8_t* out, size_t out_capacity);
size_t chameleon_build_get_device_address(uint8_t* out, size_t out_capacity);
size_t chameleon_build_get_enabled_slots(uint8_t* out, size_t out_capacity);
size_t chameleon_build_get_device_mode(uint8_t* out, size_t out_capacity);
size_t chameleon_build_mf1_detect_support(uint8_t* out, size_t out_capacity);
size_t chameleon_build_hf14a_scan(uint8_t* out, size_t out_capacity);

/* slot must be 0-7; returns 0 (build failure) otherwise. */
size_t chameleon_build_set_active_slot(uint8_t slot, uint8_t* out, size_t out_capacity);

/* mode: 0x00 = emulator, 0x01 = reader. */
size_t chameleon_build_change_device_mode(uint8_t mode, uint8_t* out, size_t out_capacity);

/* block_count is 1-32 per the protocol; keeping it at 1 per call in this
   codebase avoids depending on BLE MTU/fragmentation behavior that
   hasn't been tested against real hardware - see README.md. */
size_t chameleon_build_mf1_read_emu_block(
    uint8_t block_start,
    uint8_t block_count,
    uint8_t* out,
    size_t out_capacity);

/* type is 0x60 for key A, 0x61 for key B. Authenticates and reads in one
   round trip: per the protocol's general status rule, a key that fails
   authentication gets an empty response body rather than 16 bytes of
   data, which is what this codebase checks for success - see the
   comment on action_read_card_with_dictionary() in main.c for why that,
   and not the response status code, is used as the pass/fail signal. */
size_t chameleon_build_mf1_read_one_block(
    uint8_t type,
    uint8_t block,
    const uint8_t key[6],
    uint8_t* out,
    size_t out_capacity);

/* Formats a parsed response for on-screen display. Falls back to a raw
   byte count for any command without a specific formatter. */
void chameleon_format_response(const ChameleonFrame* frame, char* out, size_t out_capacity);

/* HF14A_SCAN's response is variable-length (uidlen|uid|atqa|sak|ats) and
   needs its own formatter rather than a chameleon_format_response case. */
void chameleon_format_hf14a_scan(const ChameleonFrame* frame, char* out, size_t out_capacity);

/* Interprets a 16-byte Mifare Classic block as a manufacturer block
   (typically block 0 of sector 0). The 4-byte-vs-7-byte UID distinction
   is inferred from whether byte 4 matches the XOR of bytes 0-3 (the
   standard BCC convention for a 4-byte UID) - a heuristic, not something
   the protocol states explicitly, since the response is just raw block
   bytes with no length tag of its own. block must point to at least 16
   readable bytes. */
void chameleon_format_uid_block(const uint8_t* block, char* out, size_t out_capacity);
