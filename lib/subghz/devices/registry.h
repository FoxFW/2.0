#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SubGhzDevice SubGhzDevice;

void subghz_device_registry_init(void);

void subghz_device_registry_init_internal_only(void);

bool subghz_device_registry_load_external(void);

void subghz_device_registry_deinit(void);

bool subghz_device_registry_is_valid(void);

const SubGhzDevice* subghz_device_registry_get_by_name(const char* name);

const SubGhzDevice* subghz_device_registry_get_by_index(size_t index);

size_t subghz_device_registry_count(void);

#ifdef __cplusplus
}
#endif
