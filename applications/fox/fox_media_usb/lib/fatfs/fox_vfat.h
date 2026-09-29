#ifndef FOX_VFAT_H
#define FOX_VFAT_H

#include <stdint.h>
#include <stdbool.h>
#include "fox_vfat_plat.h"

#define FOX_VFAT_SECTOR 512u

typedef struct FoxVfat FoxVfat;

FoxVfat* fox_vfat_alloc(void* storage, const char* meta_path);
void fox_vfat_free(FoxVfat* v);

bool fox_vfat_build_begin(FoxVfat* v, uint64_t volume_bytes, const char* label);

bool fox_vfat_build_mkdir(FoxVfat* v, const char* dir);

bool fox_vfat_build_add(FoxVfat* v, const char* rel_path, const char* src_path, uint32_t size);

bool fox_vfat_build_end(FoxVfat* v);

uint32_t fox_vfat_total_sectors(FoxVfat* v);

bool fox_vfat_read(FoxVfat* v, uint32_t lba, uint32_t count, uint8_t* out, uint32_t out_cap, uint32_t* out_len);

#endif
