#pragma once

#include "app.h"

typedef struct {
    bool found;
    UpdaterFlow flow;
    char tag[UPDATER_STR_LEN];
    char commit[16];
    char board_folder[16];
    char asset_path[UPDATER_PATH_LEN];
    uint32_t size;
} DownloadedMeta;

void update_meta_write(
    Storage* storage,
    const char* asset_path,
    UpdaterFlow flow,
    const char* tag,
    const char* commit,
    const char* board_folder,
    uint32_t size);

void update_meta_find(Storage* storage, UpdaterFlow flow, const char* board_folder, DownloadedMeta* out);

void update_meta_delete(Storage* storage, const char* asset_path);
