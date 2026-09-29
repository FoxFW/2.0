#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Version Version;

const Version* version_get(void);

const char* version_get_githash(const Version* v);

const char* version_get_gitbranch(const Version* v);

const char* version_get_gitbranchnum(const Version* v);

const char* version_get_builddate(const Version* v);

const char* version_get_version(const Version* v);

const char* version_get_custom_name(const Version* v);

void version_set_custom_name(Version* v, const char* name);

uint8_t version_get_target(const Version* v);

bool version_get_dirty_flag(const Version* v);

const char* version_get_firmware_origin(const Version* v);

const char* version_get_git_origin(const Version* v);

#ifdef __cplusplus
}
#endif
