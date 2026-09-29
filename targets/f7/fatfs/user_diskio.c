#include <furi.h>
#include <furi_hal.h>
#include "user_diskio.h"
#include "sector_cache.h"

static DSTATUS driver_initialize(BYTE pdrv);
static DSTATUS driver_status(BYTE pdrv);
static DRESULT driver_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
static DRESULT driver_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count);
static DRESULT driver_ioctl(BYTE pdrv, BYTE cmd, void* buff);

const Diskio_drvTypeDef sd_fatfs_driver = {
    driver_initialize,
    driver_status,
    driver_read,
    driver_write,
    driver_ioctl,
};

static DSTATUS driver_initialize(BYTE pdrv) {
    UNUSED(pdrv);
    return RES_OK;
}

static DSTATUS driver_status(BYTE pdrv) {
    UNUSED(pdrv);
    DSTATUS status = 0;
    if(furi_hal_sd_get_card_state() != FuriStatusOk) {
        status = STA_NOINIT;
    }

    return status;
}

static DRESULT driver_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
    UNUSED(pdrv);
    FuriStatus status = furi_hal_sd_read_blocks((uint32_t*)buff, (uint32_t)(sector), count);
    return status == FuriStatusOk ? RES_OK : RES_ERROR;
}

static DRESULT driver_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
    UNUSED(pdrv);
    FuriStatus status = furi_hal_sd_write_blocks((uint32_t*)buff, (uint32_t)(sector), count);
    return status == FuriStatusOk ? RES_OK : RES_ERROR;
}

static DRESULT driver_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    DRESULT res = RES_ERROR;
    FuriHalSdInfo sd_info;

    DSTATUS status = driver_status(pdrv);
    if(status & STA_NOINIT) return RES_NOTRDY;

    switch(cmd) {

    case CTRL_SYNC:
        res = RES_OK;
        break;

    case GET_SECTOR_COUNT:
        furi_hal_sd_info(&sd_info);
        *(DWORD*)buff = sd_info.logical_block_count;
        res = RES_OK;
        break;

    case GET_SECTOR_SIZE:
        furi_hal_sd_info(&sd_info);
        *(WORD*)buff = sd_info.logical_block_size;
        res = RES_OK;
        break;

    case GET_BLOCK_SIZE:
        furi_hal_sd_info(&sd_info);
        *(DWORD*)buff = sd_info.logical_block_size;
        res = RES_OK;
        break;

    default:
        res = RES_PARERR;
    }

    return res;
}
