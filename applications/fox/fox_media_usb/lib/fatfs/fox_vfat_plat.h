#ifndef FOX_VFAT_PLAT_H
#define FOX_VFAT_PLAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct FoxVfatFile FoxVfatFile;

FoxVfatFile* fvp_open_read(void* storage, const char* path);
FoxVfatFile* fvp_open_rw(void* storage, const char* path);
uint32_t fvp_read(FoxVfatFile* f, void* buf, uint32_t len);
uint32_t fvp_write(FoxVfatFile* f, const void* buf, uint32_t len);
bool fvp_seek(FoxVfatFile* f, uint64_t off);
uint64_t fvp_size(FoxVfatFile* f);
void fvp_close(FoxVfatFile* f);

#endif
