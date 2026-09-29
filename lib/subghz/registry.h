#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SubGhzEnvironment SubGhzEnvironment;

typedef struct SubGhzProtocolRegistry SubGhzProtocolRegistry;
typedef struct SubGhzProtocol SubGhzProtocol;

struct SubGhzProtocolRegistry {
    const SubGhzProtocol* const* items;
    const size_t size;
};

const SubGhzProtocol* subghz_protocol_registry_get_by_name(
    const SubGhzProtocolRegistry* protocol_registry,
    const char* name);

const SubGhzProtocol* subghz_protocol_registry_get_by_index(
    const SubGhzProtocolRegistry* protocol_registry,
    size_t index);

size_t subghz_protocol_registry_count(const SubGhzProtocolRegistry* protocol_registry);

#ifdef __cplusplus
}
#endif
