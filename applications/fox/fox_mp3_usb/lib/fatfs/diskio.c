#include "diskio.h"
#include "fox_vfat_diskio.h"

DSTATUS disk_initialize(BYTE pdrv) {
    (void)pdrv;
    return fox_vfat_diskio_ready() ? 0 : STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv) {
    (void)pdrv;
    return fox_vfat_diskio_ready() ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
    (void)pdrv;
    return fox_vfat_diskio_read((uint32_t)sector, buff, (uint32_t)count) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
    (void)pdrv;
    return fox_vfat_diskio_write((uint32_t)sector, buff, (uint32_t)count) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    (void)pdrv;
    switch(cmd) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        *(DWORD*)buff = (DWORD)fox_vfat_diskio_sector_count();
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD*)buff = 512;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD*)buff = 1;
        return RES_OK;
    case CTRL_TRIM:
        return RES_OK;
    default:
        return RES_PARERR;
    }
}
