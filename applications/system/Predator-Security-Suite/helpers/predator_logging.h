#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PredatorApp;

// Append a single-line log to /ext/predator_logs.txt (with trailing \n)
bool predator_log_append(struct PredatorApp* app, const char* line);

#ifdef __cplusplus
}
#endif
