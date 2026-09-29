#ifndef TAGTINKER_WIFI_BMP_H
#define TAGTINKER_WIFI_BMP_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <storage/storage.h>

#define TAGTINKER_WIFI_TMP_BMP "/ext/apps_data/tagtinker/wifi_temp.bmp"

typedef struct {
    File*    file;
    Storage* storage;
    uint16_t width;
    uint16_t height;
    uint8_t  planes;
    uint8_t  accent_r;
    uint8_t  accent_g;
    uint8_t  accent_b;
    uint16_t row_stride;
    uint32_t bytes_written;

    uint8_t* pixel_buf;
    size_t   pixel_size;
    size_t   plane_size;
} TagTinkerWifiBmpWriter;

bool tagtinker_wifi_bmp_open(TagTinkerWifiBmpWriter* w,
                             uint16_t width, uint16_t height,
                             uint8_t planes,
                             uint8_t accent_r, uint8_t accent_g, uint8_t accent_b);
bool tagtinker_wifi_bmp_chunk(TagTinkerWifiBmpWriter* w, const uint8_t* data, size_t len);
bool tagtinker_wifi_bmp_close(TagTinkerWifiBmpWriter* w);
void tagtinker_wifi_bmp_abort(TagTinkerWifiBmpWriter* w);

#endif
