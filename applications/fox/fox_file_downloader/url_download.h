#pragma once

#include "app.h"

#define FOX_DOWNLOAD_EVENT_WORKER_DONE 3

void download_start_worker(App* app);
void download_worker_free_if_done(App* app);

int32_t download_worker_thread(void* context);

void download_work_path(const char* final_path, char* out, size_t out_size);
