#include "nyx_store.h"
#include "ir_sense.h"
#include "../nyx_i.h"

#include <furi.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#define NYX_SETTINGS_PATH    APP_DATA_PATH("settings.bin")
#define NYX_SETTINGS_MAGIC   0x4E // 'N'
#define NYX_SETTINGS_VERSION 2 // 2 added `intro`

/* Exactly the v1 layout. saved_struct validates the stored size, so adding
 * `intro` made every v1.1 file fail to load and silently reset Mode,
 * Sensitivity, probe pin and the three alert toggles on first launch. Reading
 * the old shape explicitly and carrying the values forward costs a few lines
 * and spares anyone who had already set the app up. */
typedef struct {
    uint8_t mode_index;
    uint8_t sensitivity_index;
    uint8_t probe_pin_index;
    uint8_t sound;
    uint8_t vibro;
    uint8_t led;
} NyxSettingsV1;

static void nyx_store_ensure_dir(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    furi_record_close(RECORD_STORAGE);
}

void nyx_store_settings_save(const NyxSettings* s) {
    furi_assert(s);
    nyx_store_ensure_dir();
    saved_struct_save(
        NYX_SETTINGS_PATH, s, sizeof(NyxSettings), NYX_SETTINGS_MAGIC, NYX_SETTINGS_VERSION);
}

void nyx_store_settings_load(NyxSettings* s) {
    furi_assert(s);
    NyxSettings loaded;
    if(!saved_struct_load(
           NYX_SETTINGS_PATH,
           &loaded,
           sizeof(NyxSettings),
           NYX_SETTINGS_MAGIC,
           NYX_SETTINGS_VERSION)) {
        /* Not a v2 file. Try the v1 shape before giving up on it. */
        NyxSettingsV1 v1;
        if(!saved_struct_load(
               NYX_SETTINGS_PATH, &v1, sizeof(v1), NYX_SETTINGS_MAGIC, 1)) {
            return; // nothing valid on disk — caller keeps its defaults
        }
        loaded = *s; // start from the caller's defaults, notably `intro`
        loaded.mode_index = v1.mode_index;
        loaded.sensitivity_index = v1.sensitivity_index;
        loaded.probe_pin_index = v1.probe_pin_index;
        loaded.sound = v1.sound;
        loaded.vibro = v1.vibro;
        loaded.led = v1.led;
    }

    /* Never trust a file to index an array. Clamp everything that later
     * subscripts a table or drives a HAL enum. */
    if(loaded.mode_index > IrSenseModeProbe) loaded.mode_index = IrSenseModeAuto;
    if(loaded.sensitivity_index > 2) loaded.sensitivity_index = 1;
    uint8_t pin_count = ir_sense_probe_pin_count();
    if(pin_count == 0 || loaded.probe_pin_index >= pin_count) loaded.probe_pin_index = 0;
    /* These are plain bytes off a card anyone can write, and they reach
     * variable_item_set_current_value_index(), so clamp rather than trust. */
    loaded.sound = !!loaded.sound;
    loaded.vibro = !!loaded.vibro;
    loaded.led = !!loaded.led;
    loaded.intro = !!loaded.intro;

    *s = loaded;
}
