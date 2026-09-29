#ifndef FOX_VFAT_DISKIO_H
#define FOX_VFAT_DISKIO_H

#include <stdint.h>
#include "fox_vfat.h"

void fox_vfat_diskio_bind(FoxVfat* v);
void fox_vfat_diskio_unbind(void);
int fox_vfat_diskio_ready(void);
uint32_t fox_vfat_diskio_sector_count(void);
int fox_vfat_diskio_read(uint32_t sector, uint8_t* buf, uint32_t count);
int fox_vfat_diskio_write(uint32_t sector, const uint8_t* buf, uint32_t count);

#endif
