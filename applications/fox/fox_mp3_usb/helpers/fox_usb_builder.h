#pragma once

#include <furi.h>
#include <storage/storage.h>
#include "fox_vfat.h"

typedef void (*FoxUsbBuildProgress)(void* ctx, uint32_t done, uint32_t total, const char* name);

typedef enum {
    FoxUsbBuildResultBuilt,
    FoxUsbBuildResultError,
} FoxUsbBuildResult;

typedef struct {
    Storage* storage;
    const char* source_dir;
    const char* meta_path;
    const char* label;
    bool recursive;
    const char* const* ensure_dirs;
    size_t ensure_dir_count;
} FoxUsbBuildConfig;

FoxUsbBuildResult fox_usb_builder_run(
    const FoxUsbBuildConfig* cfg,
    FoxVfat** out_vfat,
    FoxUsbBuildProgress cb,
    void* cb_ctx);
