#pragma once

#include "app.h"

#define FOX_DOWNLOAD_QUEUE_PATH "/ext/download.txt"

#define FOX_DOWNLOAD_STATE_PATH FOX_DOWNLOAD_DATA_DIR "/download_state.txt"

void download_queue_ensure_file(void);

bool download_queue_take_next(char* url_out, size_t url_out_size);

void download_state_mark_started(const char* url, const char* path);

void download_state_set_total(uint32_t total);

void download_state_clear(void);

bool download_state_load(
    char* url_out,
    size_t url_out_size,
    char* path_out,
    size_t path_out_size,
    uint32_t* total_out);
