#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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

typedef enum {
    ChameleonFrameOk,
    ChameleonFrameErrTooShort,
    ChameleonFrameErrBadSof,
    ChameleonFrameErrBadLrc1,
    ChameleonFrameErrBadLrc2,
    ChameleonFrameErrLenExceedsBuffer,
    ChameleonFrameErrBadLrc3,
} ChameleonFrameError;

const char* chameleon_frame_error_str(ChameleonFrameError error);

size_t chameleon_build_frame(
    uint16_t cmd,
    const uint8_t* data,
    uint16_t data_len,
    uint8_t* out,
    size_t out_capacity);

bool chameleon_parse_frame_ex(
    const uint8_t* buffer,
    size_t length,
    ChameleonFrame* out,
    ChameleonFrameError* error_out);

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

size_t chameleon_build_set_active_slot(uint8_t slot, uint8_t* out, size_t out_capacity);

size_t chameleon_build_change_device_mode(uint8_t mode, uint8_t* out, size_t out_capacity);

size_t chameleon_build_mf1_read_emu_block(
    uint8_t block_start,
    uint8_t block_count,
    uint8_t* out,
    size_t out_capacity);

size_t chameleon_build_mf1_read_one_block(
    uint8_t type,
    uint8_t block,
    const uint8_t key[6],
    uint8_t* out,
    size_t out_capacity);

void chameleon_format_response(const ChameleonFrame* frame, char* out, size_t out_capacity);

void chameleon_format_hf14a_scan(const ChameleonFrame* frame, char* out, size_t out_capacity);

void chameleon_format_uid_block(const uint8_t* block, char* out, size_t out_capacity);
