#pragma once

#include "app.h"

#define FOX_LOG_DIR "/ext/apps_data/fox_esp32_terminal/logs"

void session_log_open(App* app);
void session_log_write_line(App* app, const char* line);
void session_log_close(App* app);

void log_list_scan(App* app);

bool log_read_file(App* app, const char* filename, FuriString* out);
