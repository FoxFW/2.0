#pragma once

#include <furi.h>
#include <m-array.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FuriString* name;
    uint64_t key;
    uint16_t type;
} SubGhzKey;

ARRAY_DEF(SubGhzKeyArray, SubGhzKey, M_POD_OPLIST)

#define M_OPL_SubGhzKeyArray_t() ARRAY_OPLIST(SubGhzKeyArray, M_POD_OPLIST)

typedef struct SubGhzKeystore SubGhzKeystore;

SubGhzKeystore* subghz_keystore_alloc(void);

void subghz_keystore_free(SubGhzKeystore* instance);

bool subghz_keystore_load(SubGhzKeystore* instance, const char* filename);

bool subghz_keystore_save(SubGhzKeystore* instance, const char* filename, uint8_t* iv);

SubGhzKeyArray_t* subghz_keystore_get_data(SubGhzKeystore* instance);

bool subghz_keystore_raw_encrypted_save(
    const char* input_file_name,
    const char* output_file_name,
    uint8_t* iv);

bool subghz_keystore_raw_get_data(const char* file_name, size_t offset, uint8_t* data, size_t len);

void subghz_keystore_reset_kl(SubGhzKeystore* instance);

#ifdef __cplusplus
}
#endif
