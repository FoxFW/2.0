#pragma once
#include "csight.h"

void csight_log_init(CSIghtApp* app);

void csight_log_event(CSIghtApp* app, const char* event, const char* detail);
