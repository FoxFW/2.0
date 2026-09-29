#ifndef BAMBU_PARSER_H
#define BAMBU_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define BLOCK_MATERIAL_IDS      1
#define BLOCK_FILAMENT_TYPE     2
#define BLOCK_DETAILED_TYPE     4
#define BLOCK_COLOR_WEIGHT      5
#define BLOCK_TEMPERATURES      6
#define BLOCK_NOZZLE            8
#define BLOCK_SPOOL_EXTRA       9
#define BLOCK_SPOOL_WIDTH      10
#define BLOCK_PRODUCTION_DATE  12
#define BLOCK_PROD_EXTRA       13
#define BLOCK_FILAMENT_LENGTH  14

static const uint8_t BAMBU_SPEC_BLOCKS[] = {
    BLOCK_NOZZLE,
    BLOCK_SPOOL_EXTRA,
    BLOCK_SPOOL_WIDTH,
    BLOCK_PRODUCTION_DATE,
    BLOCK_PROD_EXTRA,
    BLOCK_FILAMENT_LENGTH,
};
#define BAMBU_NUM_SPEC_BLOCKS (sizeof(BAMBU_SPEC_BLOCKS) / sizeof(BAMBU_SPEC_BLOCKS[0]))

static inline uint16_t bambu_read_le16(const uint8_t* data) {
    return (uint16_t)(data[0] | (data[1] << 8));
}

static inline float bambu_read_le_float(const uint8_t* data) {
    union {
        uint32_t u;
        float f;
    } val;
    val.u = (uint32_t)(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
    return val.f;
}

static inline bool bambu_is_printable_ascii(const uint8_t* data, size_t len) {
    bool found_printable = false;
    for(size_t i = 0; i < len; i++) {
        if(data[i] == 0) continue;
        if(data[i] < 0x20 || data[i] > 0x7E) return false;
        found_printable = true;
    }
    return found_printable;
}

static inline void bambu_copy_ascii_string(char* dest, const uint8_t* src, size_t max_len) {
    size_t i;
    for(i = 0; i < max_len && src[i] != 0 && src[i] >= 0x20 && src[i] <= 0x7E; i++) {
        dest[i] = (char)src[i];
    }
    dest[i] = '\0';
}

static inline bool bambu_spec_blocks_are_zero(const MfClassicData* data) {
    for(size_t i = 0; i < BAMBU_NUM_SPEC_BLOCKS; i++) {
        const uint8_t* block = data->block[BAMBU_SPEC_BLOCKS[i]].data;
        for(size_t j = 0; j < sizeof(data->block[0].data); j++) {
            if(block[j] != 0) {
                return false;
            }
        }
    }
    return true;
}

static const char* const BAMBU_KNOWN_FILAMENT_TYPES[] = {
    "PLA", "PETG", "ABS", "TPU", "PA", "PC", "ASA", "PVA", "HIPS", "PET"};
#define BAMBU_NUM_FILAMENT_TYPES \
    (sizeof(BAMBU_KNOWN_FILAMENT_TYPES) / sizeof(BAMBU_KNOWN_FILAMENT_TYPES[0]))

static inline bool bambu_tag_is_valid(const MfClassicData* data) {

    if(data->type != MfClassicType1k) {
        return false;
    }

    const uint8_t* block1 = data->block[BLOCK_MATERIAL_IDS].data;
    if(block1[8] != 'G' || block1[9] != 'F') {
        return false;
    }

    const uint8_t* block2 = data->block[BLOCK_FILAMENT_TYPE].data;
    bool valid_type = false;
    for(size_t i = 0; i < BAMBU_NUM_FILAMENT_TYPES; i++) {
        size_t len = strlen(BAMBU_KNOWN_FILAMENT_TYPES[i]);
        if(memcmp(block2, BAMBU_KNOWN_FILAMENT_TYPES[i], len) == 0) {
            valid_type = true;
            break;
        }
    }
    if(!valid_type) {
        return false;
    }

    const uint8_t* block4 = data->block[BLOCK_DETAILED_TYPE].data;
    if(!bambu_is_printable_ascii(block4, 16)) {
        return false;
    }

    const uint8_t* block5 = data->block[BLOCK_COLOR_WEIGHT].data;
    float diameter = bambu_read_le_float(&block5[8]);
    bool valid_diameter = (diameter >= 1.6f && diameter <= 2.0f) ||
                          (diameter >= 2.7f && diameter <= 3.0f);
    if(!valid_diameter) {
        return false;
    }

    if(bambu_spec_blocks_are_zero(data)) {
        return false;
    }

    return true;
}

#endif
