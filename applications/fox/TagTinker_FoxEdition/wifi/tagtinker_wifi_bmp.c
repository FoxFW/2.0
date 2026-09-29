#include "tagtinker_wifi_bmp.h"

#include <furi.h>
#include <stdlib.h>
#include <string.h>

#define BMP_FILE_HDR    14U
#define BMP_DIB_HDR     40U
#define BMP_PALETTE_2    8U
#define BMP_PALETTE_3   12U

static void put_le16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

bool tagtinker_wifi_bmp_open(TagTinkerWifiBmpWriter* w,
                             uint16_t width, uint16_t height,
                             uint8_t planes,
                             uint8_t accent_r, uint8_t accent_g, uint8_t accent_b) {
    memset(w, 0, sizeof(*w));
    w->width = width;
    w->height = height;
    w->planes = (planes >= 2) ? 2 : 1;
    w->accent_r = accent_r ? accent_r : 0xE0;
    w->accent_g = accent_g;
    w->accent_b = accent_b;
    w->row_stride = (uint16_t)(((width + 31U) / 32U) * 4U);
    w->plane_size = (size_t)w->row_stride * height;
    w->pixel_size = w->plane_size * w->planes;

    w->pixel_buf = malloc(w->pixel_size);
    if(!w->pixel_buf) return false;

    memset(w->pixel_buf, 0x00, w->pixel_size);

    w->storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(w->storage, "/ext/apps_data/tagtinker");
    w->file = storage_file_alloc(w->storage);
    if(!storage_file_open(w->file, TAGTINKER_WIFI_TMP_BMP,
                          FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(w->file); w->file = NULL;
        furi_record_close(RECORD_STORAGE); w->storage = NULL;
        free(w->pixel_buf); w->pixel_buf = NULL;
        return false;
    }
    return true;
}

bool tagtinker_wifi_bmp_chunk(TagTinkerWifiBmpWriter* w, const uint8_t* data, size_t len) {
    if(!w->pixel_buf) return false;

    size_t off = (size_t)w->bytes_written;
    if(off >= w->pixel_size) return true;
    size_t remain = w->pixel_size - off;
    size_t take = (len < remain) ? len : remain;
    memcpy(w->pixel_buf + off, data, take);
    w->bytes_written += (uint32_t)take;
    return true;
}

bool tagtinker_wifi_bmp_close(TagTinkerWifiBmpWriter* w) {
    if(!w->file) return false;

    const uint16_t pal_bytes = (w->planes == 2) ? BMP_PALETTE_3 : BMP_PALETTE_2;
    const uint16_t hdr_total = BMP_FILE_HDR + BMP_DIB_HDR + pal_bytes;
    const uint32_t pixel_section = (uint32_t)w->pixel_size;
    const uint32_t total_size = hdr_total + pixel_section;

    uint8_t hdr[BMP_FILE_HDR + BMP_DIB_HDR + BMP_PALETTE_3] = {0};

    hdr[0] = 'B'; hdr[1] = 'M';
    put_le32(&hdr[2],  total_size);
    put_le32(&hdr[10], hdr_total);

    put_le32(&hdr[14], BMP_DIB_HDR);
    put_le32(&hdr[18], (uint32_t)w->width);
    put_le32(&hdr[22], (uint32_t)w->height);
    put_le16(&hdr[26], 1);
    put_le16(&hdr[28], (uint16_t)w->planes);
    put_le32(&hdr[30], 0);
    put_le32(&hdr[34], pixel_section);
    put_le32(&hdr[38], 2835);
    put_le32(&hdr[42], 2835);
    put_le32(&hdr[46], (uint32_t)(w->planes == 2 ? 3 : 2));
    put_le32(&hdr[50], 0);

    hdr[54] = 0xFF; hdr[55] = 0xFF; hdr[56] = 0xFF; hdr[57] = 0x00;
    hdr[58] = 0x00; hdr[59] = 0x00; hdr[60] = 0x00; hdr[61] = 0x00;
    if(w->planes == 2) {
        hdr[62] = w->accent_b; hdr[63] = w->accent_g;
        hdr[64] = w->accent_r; hdr[65] = 0x00;
    }

    if(storage_file_write(w->file, hdr, hdr_total) != hdr_total) goto fail;

    for(uint8_t pl = 0; pl < w->planes; pl++) {
        const uint8_t* plane_base = w->pixel_buf + pl * w->plane_size;
        for(int32_t row = (int32_t)w->height - 1; row >= 0; row--) {
            const uint8_t* src = plane_base + (size_t)row * w->row_stride;
            if(storage_file_write(w->file, src, w->row_stride) != w->row_stride) goto fail;
        }
    }

    storage_file_close(w->file);
    storage_file_free(w->file);
    furi_record_close(RECORD_STORAGE);
    free(w->pixel_buf);
    memset(w, 0, sizeof(*w));
    return true;

fail:
    tagtinker_wifi_bmp_abort(w);
    return false;
}

void tagtinker_wifi_bmp_abort(TagTinkerWifiBmpWriter* w) {
    if(w->file) { storage_file_close(w->file); storage_file_free(w->file); }
    if(w->storage) furi_record_close(RECORD_STORAGE);
    free(w->pixel_buf);
    memset(w, 0, sizeof(*w));
}
