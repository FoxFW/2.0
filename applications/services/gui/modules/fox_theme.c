#include "fox_theme.h"
#include <storage/storage.h>
#include <furi_hal.h>

static uint8_t g_fox_theme = 255u;

uint8_t fox_theme_get_style(void) {
    if(g_fox_theme == 255u) {
        uint8_t val  = 1u;
        bool    found = false;
        Storage* st = furi_record_open(RECORD_STORAGE);
        if(st) {
            File* f = storage_file_alloc(st);
            if(storage_file_open(f, FOX_THEME_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
                storage_file_read(f, &val, 1);
                storage_file_close(f);
                found = true;
            }
            storage_file_free(f);
            furi_record_close(RECORD_STORAGE);
        }

        g_fox_theme = found ? ((val <= 4u) ? val : 1u) : 1u;
    }
    return g_fox_theme;
}

bool fox_theme_is_active(void) {
    return fox_theme_get_style() != 0u;
}

void fox_theme_set_style(uint8_t style) {
    g_fox_theme = (style <= 4u) ? style : 1u;
    Storage* st = furi_record_open(RECORD_STORAGE);
    if(st) {
        File* f = storage_file_alloc(st);
        if(storage_file_open(f, FOX_THEME_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(f, &g_fox_theme, 1);
            storage_file_close(f);
        }
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
    }
}

void fox_theme_set(bool active) {
    fox_theme_set_style(active ? 1u : 0u);
}
