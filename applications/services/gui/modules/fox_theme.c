/* fox_theme.c — Fox firmware menu-theme implementation.
 * Compiled alongside submenu.c / variable_item_list.c in
 * applications/services/gui/modules/. */

#include "fox_theme.h"
#include <storage/storage.h>
#include <furi_hal.h>

/* 0 = Classic, 1 = Fox, 255 = not yet loaded from file */
static uint8_t g_fox_theme = 255u;

bool fox_theme_is_active(void) {
    if(g_fox_theme == 255u) {
        /* First access — read persisted value from internal storage.
         * Default is Fox Theme (1) when the file doesn't exist yet
         * (i.e. fresh install / factory reset). */
        uint8_t val  = 1u;   /* default: Fox Theme */
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
        /* File not found → fresh install: use Fox as default.
         * Do NOT call fox_theme_set() here — is_active() may be called
         * from a GUI draw callback and fox_theme_set() writes to storage,
         * which is unsafe from that context and can cause g_fox_theme to
         * be written back to Fox even after the user selects Classic.
         * The file will be written by fox_theme_set() when the user first
         * visits Desktop Settings → Menu Style. */
        g_fox_theme = found ? ((val != 0u) ? 1u : 0u) : 1u;
    }
    return g_fox_theme != 0u;
}

void fox_theme_set(bool active) {
    g_fox_theme = active ? 1u : 0u;
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
