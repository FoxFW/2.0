#pragma once

#include <furi.h>

#include "loader.h"

#define LOADER_QUEUE_MAX_SIZE 4

typedef struct {
    char* name_or_path;
    char* args;
    LoaderDeferredLaunchFlag flags;
} LoaderDeferredLaunchRecord;

typedef struct {
    LoaderDeferredLaunchRecord items[LOADER_QUEUE_MAX_SIZE];
    size_t item_cnt;
} LoaderLaunchQueue;

void loader_queue_item_clear(LoaderDeferredLaunchRecord* item);

bool loader_queue_pop(LoaderLaunchQueue* queue, LoaderDeferredLaunchRecord* item);

bool loader_queue_push(LoaderLaunchQueue* queue, LoaderDeferredLaunchRecord* item);

void loader_queue_clear(LoaderLaunchQueue* queue);
