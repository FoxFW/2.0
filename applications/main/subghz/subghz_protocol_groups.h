#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SUBGHZ_PROTOCOL_GROUP_COUNT 17

extern const char* const subghz_protocol_group_names[SUBGHZ_PROTOCOL_GROUP_COUNT];

size_t subghz_protocol_group_for_name(const char* protocol_name);

#ifdef __cplusplus
}
#endif
