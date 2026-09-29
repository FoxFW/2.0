#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    XbmBmpOk = 0,
    XbmBmpErrorOpenSource,
    XbmBmpErrorOpenDest,
    XbmBmpErrorBadFormat,
    XbmBmpErrorTooLarge,
    XbmBmpErrorOutOfMemory,
} XbmBmpResult;

const char* xbm_bmp_result_text(XbmBmpResult result);

typedef void (*XbmBmpProgressCb)(uint8_t percent, void* context);

XbmBmpResult xbm_to_bmp_convert(
    Storage* storage,
    const char* source_path,
    const char* dest_path,
    XbmBmpProgressCb progress_cb,
    void* progress_context);

XbmBmpResult bmp_to_xbm_convert(
    Storage* storage,
    const char* source_path,
    const char* dest_path,
    XbmBmpProgressCb progress_cb,
    void* progress_context);

#ifdef __cplusplus
}
#endif
