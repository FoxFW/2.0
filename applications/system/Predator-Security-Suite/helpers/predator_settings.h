#pragma once
#include <stdbool.h>
#include <stdint.h>

// Simple key-value settings stored in /ext/predator_settings.cfg
// Format per line: KEY=VALUE\n
#ifdef __cplusplus
extern "C" {
#endif

struct PredatorApp;

bool predator_settings_get_int(struct PredatorApp* app, const char* key, int32_t default_value, int32_t* out_value);
bool predator_settings_set_int(struct PredatorApp* app, const char* key, int32_t value);

#ifdef __cplusplus
}
#endif
