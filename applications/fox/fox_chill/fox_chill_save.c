#include "app.h"

#include <storage/storage.h>
#include <string.h>

#define FOX_CHILL_SAVE_DIR  "/ext/apps_data/fox_chill"
#define FOX_CHILL_SAVE_PATH FOX_CHILL_SAVE_DIR "/.cache"
#define FOX_CHILL_SAVE_MAGIC 0x464C4331u // 'FLC1'
#define FOX_CHILL_XOR_SEED    0x9E3779B1u

static void xor_crypt(uint8_t* data, size_t len) {
    uint32_t s = FOX_CHILL_XOR_SEED;
    for(size_t i = 0; i < len; i++) {
        s = s * 1103515245u + 12345u;
        data[i] ^= (uint8_t)(s >> 16);
    }
}

void fox_chill_save_load(App* app) {
    memset(&app->save, 0, sizeof(app->save));
    app->save.magic = FOX_CHILL_SAVE_MAGIC;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, FOX_CHILL_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FoxChillSaveData loaded;
        size_t got = storage_file_read(file, &loaded, sizeof(loaded));
        if(got == sizeof(loaded)) {
            xor_crypt((uint8_t*)&loaded, sizeof(loaded));
            if(loaded.magic == FOX_CHILL_SAVE_MAGIC) {
                app->save = loaded;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void fox_chill_save_write(App* app) {
    app->save.magic = FOX_CHILL_SAVE_MAGIC;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, FOX_CHILL_SAVE_DIR);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FOX_CHILL_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FoxChillSaveData out = app->save;
        xor_crypt((uint8_t*)&out, sizeof(out));
        storage_file_write(file, &out, sizeof(out));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void fox_chill_save_note_read(App* app, ContentKind kind) {
    switch(kind) {
    case ContentKindJoke:
        app->save.jokes_read++;
        break;
    case ContentKindRiddle:
        app->save.riddles_read++;
        break;
    case ContentKindFact:
        app->save.facts_read++;
        break;
    case ContentKindStatistic:
        app->save.statistics_read++;
        break;
    case ContentKindYoMama:
        app->save.yo_mama_read++;
        break;
    }
    fox_chill_save_write(app);
}

void fox_chill_save_note_score(App* app, uint32_t score) {
    if(score > app->save.high_score) {
        app->save.high_score = score;
        fox_chill_save_write(app);
    }
}

void fox_chill_save_note_mindful(App* app) {
    app->save.mindful_sessions++;
    fox_chill_save_write(app);
}

void fox_chill_save_note_long_wait(App* app) {
    app->save.long_waits++;
    fox_chill_save_write(app);
}
