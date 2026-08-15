#pragma once

#include <furi.h>
#include <stdbool.h>

#define CHAT_FILE_DISPLAY_MAX_CHARS 4000

bool chat_file_exists(void);
void chat_file_create_blank(void);
void chat_file_ensure_exists(void);
bool chat_file_append_line(const char* line);
bool chat_file_load(FuriString* out);
