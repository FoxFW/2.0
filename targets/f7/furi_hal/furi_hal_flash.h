#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FURI_HAL_FLASH_OB_RAW_SIZE_BYTES 0x80
#define FURI_HAL_FLASH_OB_SIZE_WORDS     (FURI_HAL_FLASH_OB_RAW_SIZE_BYTES / sizeof(uint32_t))
#define FURI_HAL_FLASH_OB_TOTAL_VALUES   (FURI_HAL_FLASH_OB_SIZE_WORDS / 2)

typedef union {
    uint8_t bytes[FURI_HAL_FLASH_OB_RAW_SIZE_BYTES];
    union {
        struct {
            uint32_t base;
            uint32_t complementary_value;
        } values;
        uint64_t dword;
    } obs[FURI_HAL_FLASH_OB_TOTAL_VALUES];
} FuriHalFlashRawOptionByteData;

_Static_assert(
    sizeof(FuriHalFlashRawOptionByteData) == FURI_HAL_FLASH_OB_RAW_SIZE_BYTES,
    "UpdateManifestOptionByteData size error");

void furi_hal_flash_init(void);

size_t furi_hal_flash_get_base(void);

size_t furi_hal_flash_get_read_block_size(void);

size_t furi_hal_flash_get_write_block_size(void);

size_t furi_hal_flash_get_page_size(void);

size_t furi_hal_flash_get_cycles_count(void);

const void* furi_hal_flash_get_free_start_address(void);

const void* furi_hal_flash_get_free_end_address(void);

size_t furi_hal_flash_get_free_page_start_address(void);

size_t furi_hal_flash_get_free_page_count(void);

void furi_hal_flash_erase(uint8_t page);

void furi_hal_flash_write_dword(size_t address, uint64_t data);

void furi_hal_flash_program_page(const uint8_t page, const uint8_t* data, uint16_t length);

int16_t furi_hal_flash_get_page_number(size_t address);

bool furi_hal_flash_ob_set_word(size_t word_idx, const uint32_t value);

void furi_hal_flash_ob_apply(void);

const FuriHalFlashRawOptionByteData* furi_hal_flash_ob_get_raw_ptr(void);

#ifdef __cplusplus
}
#endif
