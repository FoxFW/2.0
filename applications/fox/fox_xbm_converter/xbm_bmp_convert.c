#include "xbm_bmp_convert.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XBM_MAX_TEXT_BYTES (128u * 1024u)
#define IMG_MAX_PACKED_BYTES (16u * 1024u)
#define IMG_DEFAULT_W 128u
#define IMG_DEFAULT_H 64u
#define READ_CHUNK 512u

#define BMP_HEADER_SIZE 14u
#define BMP_DIB_HEADER_SIZE 40u
#define BMP_PALETTE_SIZE 8u
#define BMP_PIXEL_OFFSET (BMP_HEADER_SIZE + BMP_DIB_HEADER_SIZE + BMP_PALETTE_SIZE)

const char* xbm_bmp_result_text(XbmBmpResult result) {
    switch(result) {
    case XbmBmpOk:
        return "OK";
    case XbmBmpErrorOpenSource:
        return "Can't read source file";
    case XbmBmpErrorOpenDest:
        return "Can't write output file";
    case XbmBmpErrorBadFormat:
        return "Unrecognized file format";
    case XbmBmpErrorTooLarge:
        return "Image too large";
    case XbmBmpErrorOutOfMemory:
        return "Out of memory";
    default:
        return "Unknown error";
    }
}

static bool read_exact(File* file, uint8_t* buf, size_t len) {
    size_t total = 0;
    while(total < len) {
        size_t remaining = len - total;
        uint16_t chunk = remaining > READ_CHUNK ? (uint16_t)READ_CHUNK : (uint16_t)remaining;
        uint16_t got = storage_file_read(file, buf + total, chunk);
        if(got == 0) return false;
        total += got;
    }
    return true;
}

static uint32_t read_u32_le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int32_t read_i32_le(const uint8_t* p) {
    return (int32_t)read_u32_le(p);
}
static uint16_t read_u16_le(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static void write_u16_le(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}
static void write_u32_le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}
static void write_i32_le(uint8_t* p, int32_t v) {
    write_u32_le(p, (uint32_t)v);
}

static uint8_t hex_nibble(char c) {
    if(c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if(c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if(c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0;
}

static bool find_uint_after(const char* text, size_t text_len, const char* key, uint32_t* out) {
    size_t key_len = strlen(key);
    if(key_len == 0 || key_len > text_len) return false;
    for(size_t i = 0; i + key_len <= text_len; i++) {
        if(memcmp(text + i, key, key_len) != 0) continue;
        size_t j = i + key_len;
        while(j < text_len && (text[j] < '0' || text[j] > '9')) {
            if(text[j] == '{') return false;
            j++;
        }
        if(j >= text_len) return false;
        uint32_t value = 0;
        bool any = false;
        while(j < text_len && text[j] >= '0' && text[j] <= '9') {
            value = value * 10 + (uint32_t)(text[j] - '0');
            j++;
            any = true;
        }
        return any ? (*out = value, true) : false;
    }
    return false;
}

static XbmBmpResult read_xbm(
    Storage* storage,
    const char* path,
    uint32_t* out_width,
    uint32_t* out_height,
    uint8_t** out_bits,
    uint32_t* out_row_bytes) {
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return XbmBmpErrorOpenSource;
    }

    uint64_t file_size = storage_file_size(file);
    if(file_size == 0 || file_size > XBM_MAX_TEXT_BYTES) {
        storage_file_close(file);
        storage_file_free(file);
        return file_size == 0 ? XbmBmpErrorBadFormat : XbmBmpErrorTooLarge;
    }

    char* text = malloc((size_t)file_size + 1);
    if(!text) {
        storage_file_close(file);
        storage_file_free(file);
        return XbmBmpErrorOutOfMemory;
    }
    bool read_ok = read_exact(file, (uint8_t*)text, (size_t)file_size);
    storage_file_close(file);
    storage_file_free(file);
    if(!read_ok) {
        free(text);
        return XbmBmpErrorOpenSource;
    }
    text[file_size] = '\0';

    uint32_t width = 0, height = 0;
    bool has_w = find_uint_after(text, (size_t)file_size, "_width", &width);
    bool has_h = find_uint_after(text, (size_t)file_size, "_height", &height);
    if(!has_w || !has_h || width == 0 || height == 0) {

        width = IMG_DEFAULT_W;
        height = IMG_DEFAULT_H;
    }

    uint32_t row_bytes = (width + 7) / 8;
    uint64_t total_bytes = (uint64_t)row_bytes * height;
    if(total_bytes == 0 || total_bytes > IMG_MAX_PACKED_BYTES) {
        free(text);
        return XbmBmpErrorTooLarge;
    }

    uint8_t* bits = malloc((size_t)total_bytes);
    if(!bits) {
        free(text);
        return XbmBmpErrorOutOfMemory;
    }
    memset(bits, 0, (size_t)total_bytes);

    size_t count = 0;
    uint8_t state = 0;
    char hi_digit = 0;
    bool finished = false;
    for(size_t i = 0; i < (size_t)file_size && count < total_bytes && !finished; i++) {
        char c = text[i];
        switch(state) {
        case 0:
            if(c == '{') state = 1;
            break;
        case 1:
            if(c == '}') {
                finished = true;
            } else if(c == '0') {
                state = 2;
            }
            break;
        case 2:
            state = (c == 'x' || c == 'X') ? 3 : 1;
            break;
        case 3:
            hi_digit = c;
            state = 4;
            break;
        case 4:
            bits[count++] = (uint8_t)((hex_nibble(hi_digit) << 4) | hex_nibble(c));
            state = 1;
            break;
        }
    }
    free(text);

    if(count == 0) {
        free(bits);
        return XbmBmpErrorBadFormat;
    }

    *out_width = width;
    *out_height = height;
    *out_bits = bits;
    *out_row_bytes = row_bytes;
    return XbmBmpOk;
}

static XbmBmpResult write_bmp_1bpp(
    Storage* storage,
    const char* dest_path,
    uint32_t width,
    uint32_t height,
    const uint8_t* bits,
    uint32_t src_row_bytes,
    XbmBmpProgressCb progress_cb,
    void* progress_context) {
    uint32_t bmp_row_bytes = ((width + 31) / 32) * 4;
    uint32_t pixel_bytes = bmp_row_bytes * height;
    uint32_t file_size = BMP_PIXEL_OFFSET + pixel_bytes;

    uint8_t* row_buf = malloc(bmp_row_bytes);
    if(!row_buf) return XbmBmpErrorOutOfMemory;

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, dest_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        free(row_buf);
        return XbmBmpErrorOpenDest;
    }

    uint8_t header[BMP_PIXEL_OFFSET];
    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    write_u32_le(header + 2, file_size);
    write_u32_le(header + 10, BMP_PIXEL_OFFSET);

    write_u32_le(header + 14, BMP_DIB_HEADER_SIZE);
    write_i32_le(header + 18, (int32_t)width);
    write_i32_le(header + 22, (int32_t)height);
    write_u16_le(header + 26, 1);
    write_u16_le(header + 28, 1);
    write_u32_le(header + 30, 0);
    write_u32_le(header + 34, pixel_bytes);
    write_u32_le(header + 38, 2835);
    write_u32_le(header + 42, 2835);
    write_u32_le(header + 46, 2);
    write_u32_le(header + 50, 2);

    header[54] = 0x00;
    header[55] = 0x00;
    header[56] = 0x00;
    header[57] = 0x00;
    header[58] = 0xFF;
    header[59] = 0xFF;
    header[60] = 0xFF;
    header[61] = 0x00;

    bool ok = storage_file_write(file, header, (uint16_t)sizeof(header)) == sizeof(header);

    for(uint32_t row = 0; ok && row < height; row++) {
        uint32_t src_y = height - 1 - row;
        memset(row_buf, 0, bmp_row_bytes);
        const uint8_t* src_row = bits + (uint64_t)src_y * src_row_bytes;
        for(uint32_t x = 0; x < width; x++) {
            uint8_t src_bit = (src_row[x / 8] >> (x % 8)) & 1;
            if(!src_bit) {

                row_buf[x / 8] |= (uint8_t)(0x80 >> (x % 8));
            }
        }
        ok = storage_file_write(file, row_buf, (uint16_t)bmp_row_bytes) == bmp_row_bytes;
        if(progress_cb) progress_cb((uint8_t)(((row + 1) * 100) / height), progress_context);
    }

    storage_file_close(file);
    storage_file_free(file);
    free(row_buf);
    return ok ? XbmBmpOk : XbmBmpErrorOpenDest;
}

XbmBmpResult xbm_to_bmp_convert(
    Storage* storage,
    const char* source_path,
    const char* dest_path,
    XbmBmpProgressCb progress_cb,
    void* progress_context) {
    uint32_t width, height, row_bytes;
    uint8_t* bits = NULL;
    XbmBmpResult res = read_xbm(storage, source_path, &width, &height, &bits, &row_bytes);
    if(res != XbmBmpOk) return res;
    res = write_bmp_1bpp(storage, dest_path, width, height, bits, row_bytes, progress_cb, progress_context);
    free(bits);
    return res;
}

static XbmBmpResult read_bmp(
    Storage* storage,
    const char* path,
    uint32_t* out_width,
    uint32_t* out_height,
    uint8_t** out_bits,
    uint32_t* out_row_bytes,
    XbmBmpProgressCb progress_cb,
    void* progress_context) {
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return XbmBmpErrorOpenSource;
    }

    uint8_t file_header[BMP_HEADER_SIZE];
    if(!read_exact(file, file_header, sizeof(file_header)) || file_header[0] != 'B' ||
       file_header[1] != 'M') {
        storage_file_close(file);
        storage_file_free(file);
        return XbmBmpErrorBadFormat;
    }
    uint32_t pixel_offset = read_u32_le(file_header + 10);

    uint8_t dib_size_buf[4];
    if(!read_exact(file, dib_size_buf, sizeof(dib_size_buf))) {
        storage_file_close(file);
        storage_file_free(file);
        return XbmBmpErrorBadFormat;
    }
    uint32_t dib_size = read_u32_le(dib_size_buf);
    if(dib_size < BMP_DIB_HEADER_SIZE) {

        storage_file_close(file);
        storage_file_free(file);
        return XbmBmpErrorBadFormat;
    }

    uint8_t dib_rest[36];
    if(!read_exact(file, dib_rest, sizeof(dib_rest))) {
        storage_file_close(file);
        storage_file_free(file);
        return XbmBmpErrorBadFormat;
    }
    int32_t width_i = read_i32_le(dib_rest + 0);
    int32_t height_i = read_i32_le(dib_rest + 4);
    uint16_t bpp = read_u16_le(dib_rest + 10);
    uint32_t compression = read_u32_le(dib_rest + 12);
    uint32_t colors_used = read_u32_le(dib_rest + 28);

    if(compression != 0 ) {
        storage_file_close(file);
        storage_file_free(file);
        return XbmBmpErrorBadFormat;
    }
    if(bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32) {
        storage_file_close(file);
        storage_file_free(file);
        return XbmBmpErrorBadFormat;
    }

    bool top_down = height_i < 0;
    uint32_t width = (uint32_t)(width_i < 0 ? -width_i : width_i);
    uint32_t height = (uint32_t)(top_down ? -height_i : height_i);
    if(width == 0 || height == 0) {
        storage_file_close(file);
        storage_file_free(file);
        return XbmBmpErrorBadFormat;
    }

    uint32_t row_bytes = (width + 7) / 8;
    uint64_t total_bytes = (uint64_t)row_bytes * height;
    if(total_bytes == 0 || total_bytes > IMG_MAX_PACKED_BYTES) {
        storage_file_close(file);
        storage_file_free(file);
        return XbmBmpErrorTooLarge;
    }

    uint8_t palette[256 * 4];
    uint32_t palette_colors = 0;
    if(bpp <= 8) {
        palette_colors = colors_used ? colors_used : (1u << bpp);
        if(palette_colors > 256) palette_colors = 256;
        storage_file_seek(file, BMP_HEADER_SIZE + dib_size, true);
        if(!read_exact(file, palette, (size_t)palette_colors * 4)) {
            storage_file_close(file);
            storage_file_free(file);
            return XbmBmpErrorBadFormat;
        }
    }

    uint32_t black_index = 0;
    if(bpp == 1) {

        uint32_t lum0 = (uint32_t)palette[0] + palette[1] + palette[2];
        uint32_t lum1 = (palette_colors > 1) ? ((uint32_t)palette[4] + palette[5] + palette[6]) : (lum0 + 1);
        black_index = (lum1 < lum0) ? 1u : 0u;
    }

    uint8_t* bits = malloc((size_t)total_bytes);
    if(!bits) {
        storage_file_close(file);
        storage_file_free(file);
        return XbmBmpErrorOutOfMemory;
    }
    memset(bits, 0, (size_t)total_bytes);

    uint32_t bmp_row_bytes = ((width * bpp + 31) / 32) * 4;
    uint8_t* row_buf = malloc(bmp_row_bytes);
    if(!row_buf) {
        free(bits);
        storage_file_close(file);
        storage_file_free(file);
        return XbmBmpErrorOutOfMemory;
    }

    bool ok = true;
    for(uint32_t row = 0; ok && row < height; row++) {
        uint32_t file_row = top_down ? row : (height - 1 - row);
        uint32_t offset = pixel_offset + file_row * bmp_row_bytes;
        storage_file_seek(file, offset, true);
        if(!read_exact(file, row_buf, bmp_row_bytes)) {
            ok = false;
            break;
        }

        uint8_t* dest_row = bits + (uint64_t)row * row_bytes;
        for(uint32_t x = 0; x < width; x++) {
            bool is_black = false;
            uint32_t lum = 0;
            switch(bpp) {
            case 1: {
                uint8_t bit = (row_buf[x / 8] >> (7 - (x % 8))) & 1;
                is_black = (bit == black_index);
                break;
            }
            case 4: {
                uint8_t byte = row_buf[x / 2];
                uint8_t idx = (x % 2 == 0) ? (uint8_t)(byte >> 4) : (uint8_t)(byte & 0x0F);
                if(idx >= palette_colors) idx = 0;
                lum = (uint32_t)palette[idx * 4] + palette[idx * 4 + 1] + palette[idx * 4 + 2];
                is_black = lum < 384;
                break;
            }
            case 8: {
                uint8_t idx = row_buf[x];
                if(idx >= palette_colors) idx = 0;
                lum = (uint32_t)palette[idx * 4] + palette[idx * 4 + 1] + palette[idx * 4 + 2];
                is_black = lum < 384;
                break;
            }
            case 24: {
                const uint8_t* p = row_buf + (size_t)x * 3;
                lum = (uint32_t)p[0] + p[1] + p[2];
                is_black = lum < 384;
                break;
            }
            case 32: {
                const uint8_t* p = row_buf + (size_t)x * 4;
                lum = (uint32_t)p[0] + p[1] + p[2];
                is_black = lum < 384;
                break;
            }
            }
            if(is_black) dest_row[x / 8] |= (uint8_t)(1 << (x % 8));
        }

        if(progress_cb) progress_cb((uint8_t)(((row + 1) * 50) / height), progress_context);
    }

    free(row_buf);
    storage_file_close(file);
    storage_file_free(file);

    if(!ok) {
        free(bits);
        return XbmBmpErrorOpenSource;
    }

    *out_width = width;
    *out_height = height;
    *out_bits = bits;
    *out_row_bytes = row_bytes;
    return XbmBmpOk;
}

static void sanitize_identifier(const char* path, char* out, size_t out_size) {

    const char* slash = strrchr(path, '/');
    const char* base = slash ? slash + 1 : path;
    const char* dot = strrchr(base, '.');
    size_t len = dot ? (size_t)(dot - base) : strlen(base);
    if(len == 0) {
        strlcpy(out, "image", out_size);
        return;
    }
    if(len >= out_size) len = out_size - 1;

    size_t o = 0;
    for(size_t i = 0; i < len; i++) {
        char c = base[i];
        bool valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                     c == '_';
        out[o++] = valid ? c : '_';
    }
    out[o] = '\0';

    if(out[0] >= '0' && out[0] <= '9') {

        if(o + 2 < out_size) {
            memmove(out + 1, out, o + 1);
            out[0] = '_';
        } else {
            out[0] = '_';
        }
    }
}

static XbmBmpResult write_xbm(
    Storage* storage,
    const char* dest_path,
    uint32_t width,
    uint32_t height,
    const uint8_t* bits,
    uint32_t row_bytes,
    uint8_t progress_base,
    uint8_t progress_span,
    XbmBmpProgressCb progress_cb,
    void* progress_context) {
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, dest_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        return XbmBmpErrorOpenDest;
    }

    char name[32];
    sanitize_identifier(dest_path, name, sizeof(name));

    char header[256];
    int header_len = snprintf(
        header,
        sizeof(header),
        "#define %s_width %lu\n#define %s_height %lu\nstatic unsigned char %s_bits[] = {\n",
        name,
        (unsigned long)width,
        name,
        (unsigned long)height,
        name);
    bool ok = header_len > 0 && (size_t)header_len < sizeof(header) &&
              storage_file_write(file, header, (uint16_t)header_len) == (uint16_t)header_len;

    uint64_t total_bytes = (uint64_t)row_bytes * height;
    char line[16];
    uint8_t col = 0;
    for(uint64_t i = 0; ok && i < total_bytes; i++) {
        bool last = (i + 1 == total_bytes);
        bool eol = last || col == 11;
        int n = snprintf(line, sizeof(line), "0x%02X,%s", bits[i], eol ? "\n" : " ");
        ok = n > 0 && storage_file_write(file, line, (uint16_t)n) == (uint16_t)n;
        col = eol ? 0 : (uint8_t)(col + 1);

        if(progress_cb && ((i % row_bytes) == row_bytes - 1 || last)) {
            uint32_t row = (uint32_t)(i / row_bytes);
            uint8_t percent =
                (uint8_t)(progress_base + ((uint32_t)(row + 1) * progress_span) / height);
            progress_cb(percent, progress_context);
        }
    }

    if(ok) {
        static const char footer[] = "};\n";
        ok = storage_file_write(file, footer, (uint16_t)(sizeof(footer) - 1)) ==
             (uint16_t)(sizeof(footer) - 1);
    }

    storage_file_close(file);
    storage_file_free(file);
    return ok ? XbmBmpOk : XbmBmpErrorOpenDest;
}

XbmBmpResult bmp_to_xbm_convert(
    Storage* storage,
    const char* source_path,
    const char* dest_path,
    XbmBmpProgressCb progress_cb,
    void* progress_context) {
    uint32_t width, height, row_bytes;
    uint8_t* bits = NULL;
    XbmBmpResult res =
        read_bmp(storage, source_path, &width, &height, &bits, &row_bytes, progress_cb, progress_context);
    if(res != XbmBmpOk) return res;
    res = write_xbm(
        storage, dest_path, width, height, bits, row_bytes, 50, 50, progress_cb, progress_context);
    free(bits);
    return res;
}
