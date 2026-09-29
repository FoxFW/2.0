#pragma once

#include <furi.h>
#include "registry.h"

#include "subghz_keystore.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SubGhzEnvironment SubGhzEnvironment;
typedef struct SubGhzProtocolRegistry SubGhzProtocolRegistry;

SubGhzEnvironment* subghz_environment_alloc(void);

void subghz_environment_free(SubGhzEnvironment* instance);

bool subghz_environment_load_keystore(SubGhzEnvironment* instance, const char* filename);

SubGhzKeystore* subghz_environment_get_keystore(SubGhzEnvironment* instance);

void subghz_environment_set_came_atomo_rainbow_table_file_name(
    SubGhzEnvironment* instance,
    const char* filename);

const char* subghz_environment_get_came_atomo_rainbow_table_file_name(SubGhzEnvironment* instance);

void subghz_environment_set_alutech_at_4n_rainbow_table_file_name(
    SubGhzEnvironment* instance,
    const char* filename);

const char*
    subghz_environment_get_alutech_at_4n_rainbow_table_file_name(SubGhzEnvironment* instance);

void subghz_environment_set_nice_flor_s_rainbow_table_file_name(
    SubGhzEnvironment* instance,
    const char* filename);

const char*
    subghz_environment_get_nice_flor_s_rainbow_table_file_name(SubGhzEnvironment* instance);

void subghz_environment_set_protocol_registry(
    SubGhzEnvironment* instance,
    const SubGhzProtocolRegistry* protocol_registry_items);

const SubGhzProtocolRegistry*
    subghz_environment_get_protocol_registry(SubGhzEnvironment* instance);

const char* subghz_environment_get_protocol_name_registry(SubGhzEnvironment* instance, size_t idx);

void subghz_environment_reset_keeloq(SubGhzEnvironment* instance);

#ifdef __cplusplus
}
#endif
