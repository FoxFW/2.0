#pragma once

#include "canvas.h"
#include <u8g2.h>
#include <toolbox/compress.h>
#include <m-array.h>
#include <m-algo.h>
#include <furi.h>

#define ICON_DECOMPRESSOR_BUFFER_SIZE (128u * 64 / 8)

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*CanvasCommitCallback)(
    uint8_t* data,
    size_t size,
    CanvasOrientation orientation,
    void* context);

typedef struct {
    CanvasCommitCallback callback;
    void* context;
} CanvasCallbackPair;

ARRAY_DEF(CanvasCallbackPairArray, CanvasCallbackPair, M_POD_OPLIST);

#define M_OPL_CanvasCallbackPairArray_t() ARRAY_OPLIST(CanvasCallbackPairArray, M_POD_OPLIST)

ALGO_DEF(CanvasCallbackPairArray, CanvasCallbackPairArray_t);

struct Canvas {
    u8g2_t fb;
    CanvasOrientation orientation;
    size_t offset_x;
    size_t offset_y;
    size_t width;
    size_t height;
    CompressIcon* compress_icon;
    CanvasCallbackPairArray_t canvas_callback_pair;
    FuriMutex* mutex;
};

Canvas* canvas_init(void);

void canvas_free(Canvas* canvas);

uint8_t* canvas_get_buffer(Canvas* canvas);

size_t canvas_get_buffer_size(const Canvas* canvas);

void canvas_frame_set(
    Canvas* canvas,
    int32_t offset_x,
    int32_t offset_y,
    size_t width,
    size_t height);

void canvas_set_orientation(Canvas* canvas, CanvasOrientation orientation);

CanvasOrientation canvas_get_orientation(const Canvas* canvas);

void canvas_draw_u8g2_bitmap(
    u8g2_t* u8g2,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height,
    const uint8_t* bitmap,
    IconRotation rotation);

void canvas_add_framebuffer_callback(Canvas* canvas, CanvasCommitCallback callback, void* context);

void canvas_remove_framebuffer_callback(
    Canvas* canvas,
    CanvasCommitCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
