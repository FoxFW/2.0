#pragma once

#include "fatfs/ff.h"
#include "fatfs/ff_gen_drv.h"
#include "user_diskio.h"

#ifdef __cplusplus
extern "C" {
#endif

extern FATFS fatfs_object;

void fatfs_init(void);

#ifdef __cplusplus
}
#endif
