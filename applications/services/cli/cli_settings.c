#include "cli_settings.h"
#include "cli_settings_filename.h"

#include <saved_struct.h>
#include <storage/storage.h>

#define CLI_SETTINGS_VER (1)

#define CLI_SETTINGS_PATH  INT_PATH(CLI_SETTINGS_FILE_NAME)
#define CLI_SETTINGS_MAGIC (0x19)

void cli_settings_load(CliSettings* settings) {
    furi_assert(settings);

    bool success = saved_struct_load(
        CLI_SETTINGS_PATH,
        settings,
        sizeof(CliSettings),
        CLI_SETTINGS_MAGIC,
        CLI_SETTINGS_VER);

    if(!success) {
        settings->shell_color_index = 0;
        cli_settings_save(settings);
    }
}

void cli_settings_save(const CliSettings* settings) {
    furi_assert(settings);

    saved_struct_save(
        CLI_SETTINGS_PATH,
        settings,
        sizeof(CliSettings),
        CLI_SETTINGS_MAGIC,
        CLI_SETTINGS_VER);
}
