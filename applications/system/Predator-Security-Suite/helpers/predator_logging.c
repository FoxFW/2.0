#include "predator_logging.h"
#include "../predator_i.h"
#include <storage/storage.h>
#include <furi.h>
#include <string.h>

#define LOG_DIR  "/ext/apps_data/predator"
#define LOG_PATH "/ext/apps_data/predator/predator_logs.txt"

bool predator_log_append(struct PredatorApp* app, const char* line) {
    if(!line) return false;
    
    // CRITICAL: Only use app->storage if available, otherwise skip logging to prevent memory leaks
    if(!app || !app->storage) {
        FURI_LOG_W("PredatorLog", "Storage not available - skipping log to prevent memory leak");
        return false;
    }
    
    Storage* storage = app->storage;

    File* file = storage_file_alloc(storage);
    bool ok = false;
    // Ensure directory exists (ignore error if already exists)
    storage_common_mkdir(storage, LOG_DIR);
    if(storage_file_open(file, LOG_PATH, FSAM_WRITE, FSOM_OPEN_ALWAYS)) {
        storage_file_seek(file, 0, true); // seek to end
        // Prepend a simple tick-based timestamp for readability
        char ts[24];
        snprintf(ts, sizeof(ts), "[%lu] ", (unsigned long)furi_get_tick());
        storage_file_write(file, ts, strlen(ts));
        storage_file_write(file, line, strlen(line));
        storage_file_write(file, "\n", 1);
        storage_file_close(file);
        ok = true;
    }
    storage_file_free(file);
    return ok;
}
