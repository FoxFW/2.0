// gdr_history.c
#include "gdr_history.h"
#include "helpers/gdr_storage.h"
#include <lib/subghz/receiver.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>
#include <furi.h>
#include "defines.h"

#define TAG                          "GDRHistory"
#define HISTORY_SCRATCH_TEXT_RESERVE 256U
#define HISTORY_SCRATCH_PATH_RESERVE 128U
#define HISTORY_ARENA_RESERVE        1024U

static uint32_t gdr_history_next_capture_seq = 0;

typedef struct {
    uint32_t seq_id;
    uint16_t text_offset;
    uint16_t text_len;
    uint8_t type;
    GDRHistorySource source;
} GDRHistoryItem;

ARRAY_DEF(GDRHistoryItemArray, GDRHistoryItem, M_POD_OPLIST)

struct GDRHistory {
    GDRHistoryItemArray_t data;
    uint16_t last_index;
    uint32_t last_update_timestamp[GDRHistorySourceCount];
    uint8_t code_last_hash_data[GDRHistorySourceCount];
    Storage* storage;
    FlipperFormat* loaded_ff;
    int16_t loaded_idx;

    FuriString* scratch_text;
    FuriString* scratch_path;
    FuriString* text_arena;
};

static uint32_t gdr_history_allocate_capture_seq(void) {
    uint32_t tick_seed = (uint32_t)(furi_get_tick() & 0x0FFFFFFF);
    if(tick_seed == 0) {
        tick_seed = 1;
    }

    FURI_CRITICAL_ENTER();
    if(gdr_history_next_capture_seq == 0 ||
       gdr_history_next_capture_seq < tick_seed) {
        gdr_history_next_capture_seq = tick_seed;
    }
    uint32_t seq = gdr_history_next_capture_seq++;
    if(gdr_history_next_capture_seq == 0) {
        gdr_history_next_capture_seq = 1;
    }
    FURI_CRITICAL_EXIT();
    return seq;
}

void gdr_history_release_scratch(GDRHistory* instance) {
    furi_check(instance);
    if(instance->loaded_ff) {
        flipper_format_free(instance->loaded_ff);
        instance->loaded_ff = NULL;
    }
    instance->loaded_idx = -1;
}

static void
    gdr_history_build_path(GDRHistory* instance, uint32_t seq_id, FuriString* out) {
    UNUSED(instance);
    gdr_storage_build_history_path(seq_id, out);
}

static void
    gdr_history_delete_capture_file(GDRHistory* instance, uint32_t seq_id) {
    gdr_history_build_path(instance, seq_id, instance->scratch_path);
    gdr_storage_delete_file(furi_string_get_cstr(instance->scratch_path));
}

static void gdr_history_delete_all_capture_files(GDRHistory* instance) {
    size_t item_count = GDRHistoryItemArray_size(instance->data);
    for(size_t i = 0; i < item_count; i++) {
        GDRHistoryItem* item = GDRHistoryItemArray_get(instance->data, i);
        gdr_history_delete_capture_file(instance, item->seq_id);
    }
}

static void
    gdr_history_arena_remove(GDRHistory* instance, uint16_t offset, uint16_t len) {
    if(len == 0) return;

    size_t arena_size = furi_string_size(instance->text_arena);

    furi_check((size_t)offset + (size_t)len <= arena_size);

    const char* arena = furi_string_get_cstr(instance->text_arena);
    FuriString* rebuilt = furi_string_alloc();
    furi_check(rebuilt);
    furi_string_reserve(rebuilt, arena_size);
    furi_string_set_strn(rebuilt, arena, offset);
    furi_string_cat_str(rebuilt, arena + offset + len);
    furi_string_move(instance->text_arena, rebuilt);

    size_t n = GDRHistoryItemArray_size(instance->data);
    for(size_t i = 0; i < n; i++) {
        GDRHistoryItem* it = GDRHistoryItemArray_get(instance->data, i);
        if(it->text_offset > offset) {
            it->text_offset -= len;
        }
    }
}

GDRHistory* gdr_history_alloc(void) {
    GDRHistory* instance = malloc(sizeof(GDRHistory));
    furi_check(instance);
    GDRHistoryItemArray_init(instance->data);
    instance->last_index = 0;
    memset(instance->last_update_timestamp, 0, sizeof(instance->last_update_timestamp));
    memset(instance->code_last_hash_data, 0, sizeof(instance->code_last_hash_data));
    instance->storage = furi_record_open(RECORD_STORAGE);
    instance->loaded_ff = NULL;
    instance->loaded_idx = -1;

    instance->scratch_text = furi_string_alloc();
    furi_check(instance->scratch_text);
    furi_string_reserve(instance->scratch_text, HISTORY_SCRATCH_TEXT_RESERVE);

    instance->scratch_path = furi_string_alloc();
    furi_check(instance->scratch_path);
    furi_string_reserve(instance->scratch_path, HISTORY_SCRATCH_PATH_RESERVE);

    instance->text_arena = furi_string_alloc();
    furi_check(instance->text_arena);
    furi_string_reserve(instance->text_arena, HISTORY_ARENA_RESERVE);

    return instance;
}

void gdr_history_free(GDRHistory* instance) {
    furi_check(instance);
    gdr_history_release_scratch(instance);
    gdr_history_delete_all_capture_files(instance);
    GDRHistoryItemArray_clear(instance->data);

    if(instance->scratch_text) {
        furi_string_free(instance->scratch_text);
        instance->scratch_text = NULL;
    }
    if(instance->scratch_path) {
        furi_string_free(instance->scratch_path);
        instance->scratch_path = NULL;
    }
    if(instance->text_arena) {
        furi_string_free(instance->text_arena);
        instance->text_arena = NULL;
    }

    if(instance->storage) {
        furi_record_close(RECORD_STORAGE);
        instance->storage = NULL;
    }
    free(instance);
}

void gdr_history_reset(GDRHistory* instance) {
    furi_check(instance);
    gdr_history_release_scratch(instance);
    gdr_history_delete_all_capture_files(instance);
    GDRHistoryItemArray_reset(instance->data);
    furi_string_reset(instance->text_arena);
    instance->last_index = 0;
    memset(instance->last_update_timestamp, 0, sizeof(instance->last_update_timestamp));
    memset(instance->code_last_hash_data, 0, sizeof(instance->code_last_hash_data));
}

uint16_t gdr_history_get_item(GDRHistory* instance) {
    furi_check(instance);
    return GDRHistoryItemArray_size(instance->data);
}

uint16_t gdr_history_get_last_index(GDRHistory* instance) {
    furi_check(instance);
    return instance->last_index;
}

void gdr_history_format_status_text(
    GDRHistory* instance,
    char* output,
    size_t output_size) {
    furi_check(instance);
    furi_check(output);

    if(output_size == 0) {
        return;
    }

    uint16_t n = gdr_history_get_item(instance);
    if(n >= GDR_HISTORY_MAX) {
        snprintf(output, output_size, "FULL");
    } else {
        snprintf(output, output_size, "%u/%u", n, GDR_HISTORY_MAX);
    }
}

void gdr_history_get_status_text(GDRHistory* instance, FuriString* output) {
    furi_check(instance);
    furi_check(output);

    char status_text[16];
    gdr_history_format_status_text(instance, status_text, sizeof(status_text));
    furi_string_set_str(output, status_text);
}

bool gdr_history_get_capture_path(
    GDRHistory* instance,
    uint16_t idx,
    FuriString* out_path) {
    furi_check(instance);
    furi_check(out_path);

    if(idx >= GDRHistoryItemArray_size(instance->data)) {
        return false;
    }
    GDRHistoryItem* item = GDRHistoryItemArray_get(instance->data, idx);
    gdr_history_build_path(instance, item->seq_id, out_path);
    return true;
}

bool gdr_history_capture_path_equals(
    GDRHistory* instance,
    uint16_t idx,
    const char* path) {
    furi_check(instance);

    if(!path || idx >= GDRHistoryItemArray_size(instance->data)) {
        return false;
    }

    GDRHistoryItem* item = GDRHistoryItemArray_get(instance->data, idx);
    gdr_history_build_path(instance, item->seq_id, instance->scratch_path);
    return strcmp(furi_string_get_cstr(instance->scratch_path), path) == 0;
}

GDRHistorySource gdr_history_get_source(
    GDRHistory* instance,
    uint16_t idx) {
    furi_check(instance);
    if(idx >= GDRHistoryItemArray_size(instance->data)) {
        return GDRHistorySourceUnknown;
    }
    return GDRHistoryItemArray_get(instance->data, idx)->source;
}

const char* gdr_history_source_name(GDRHistorySource source) {
    switch(source) {
    case GDRHistorySourceExternal:
        return "External";
    case GDRHistorySourceInternal:
        return "Internal";
    default:
        return "Unknown";
    }
}

bool gdr_history_add_to_history(
    GDRHistory* instance,
    void* context,
    SubGhzRadioPreset* preset,
    GDRHistorySource source) {
    furi_check(instance);
    furi_check(context);

    if(source >= GDRHistorySourceCount) {
        source = GDRHistorySourceUnknown;
    }

    if(GDRHistoryItemArray_size(instance->data) >= GDR_HISTORY_MAX) {
        return false;
    }

    SubGhzProtocolDecoderBase* decoder_base = context;

    if((instance->code_last_hash_data[source] ==
        subghz_protocol_decoder_base_get_hash_data(decoder_base)) &&
       ((furi_get_tick() - instance->last_update_timestamp[source]) < 500)) {
        instance->last_update_timestamp[source] = furi_get_tick();
        return false;
    }

    gdr_history_release_scratch(instance);

    furi_string_reset(instance->scratch_text);
    furi_string_reset(instance->scratch_path);

    subghz_protocol_decoder_base_get_string(decoder_base, instance->scratch_text);

    FlipperFormat* temp_ff = flipper_format_string_alloc();
    furi_check(temp_ff);

    SubGhzProtocolStatus ser =
        subghz_protocol_decoder_base_serialize(decoder_base, temp_ff, preset);
    if(ser != SubGhzProtocolStatusOk) {
        FURI_LOG_E(TAG, "Serialize failed");
        flipper_format_free(temp_ff);
        return false;
    }

    if(source != GDRHistorySourceUnknown) {
        flipper_format_insert_or_update_string_cstr(
            temp_ff, "RadioDevice", gdr_history_source_name(source));
    }

    uint32_t seq = gdr_history_allocate_capture_seq();
    bool saved = gdr_storage_save_history_capture(temp_ff, seq, instance->scratch_path);
    flipper_format_free(temp_ff);

    if(!saved) {
        FURI_LOG_E(TAG, "Failed to save history file");
        return false;
    }

    instance->code_last_hash_data[source] =
        subghz_protocol_decoder_base_get_hash_data(decoder_base);
    instance->last_update_timestamp[source] = furi_get_tick();

    const char* text_cstr = furi_string_get_cstr(instance->scratch_text);
    size_t text_len = furi_string_size(instance->scratch_text);
    size_t offset = furi_string_size(instance->text_arena);
    furi_check(text_len <= UINT16_MAX);
    furi_check(offset <= UINT16_MAX);
    furi_string_cat_str(instance->text_arena, text_cstr);

    GDRHistoryItem* item = GDRHistoryItemArray_push_raw(instance->data);
    item->seq_id = seq;
    item->text_offset = (uint16_t)offset;
    item->text_len = (uint16_t)text_len;
    item->type = 0;
    item->source = source;

    instance->last_index++;

    FURI_LOG_I(
        TAG,
        "Added item %u to history (size: %zu) seq=%lu",
        instance->last_index,
        GDRHistoryItemArray_size(instance->data),
        (unsigned long)seq);

    return true;
}

void gdr_history_delete_item(GDRHistory* instance, uint16_t idx) {
    furi_check(instance);

    size_t item_count = GDRHistoryItemArray_size(instance->data);
    if(idx >= item_count) {
        return;
    }

    if(instance->loaded_ff) {
        if(instance->loaded_idx == (int16_t)idx) {
            gdr_history_release_scratch(instance);
        } else if(instance->loaded_idx > (int16_t)idx) {
            instance->loaded_idx--;
        }
    }

    GDRHistoryItem* item = GDRHistoryItemArray_get(instance->data, idx);
    uint32_t seq_id = item->seq_id;
    uint16_t text_offset = item->text_offset;
    uint16_t text_len = item->text_len;

    gdr_history_delete_capture_file(instance, seq_id);
    GDRHistoryItemArray_pop_at(NULL, instance->data, idx);
    gdr_history_arena_remove(instance, text_offset, text_len);

    FURI_LOG_I(
        TAG,
        "Deleted history item %u (size: %zu)",
        idx,
        GDRHistoryItemArray_size(instance->data));
}

void gdr_history_get_text_item_menu(
    GDRHistory* instance,
    FuriString* output,
    uint16_t idx) {
    furi_check(instance);
    furi_check(output);

    if(idx >= GDRHistoryItemArray_size(instance->data)) {
        furi_string_set(output, "---");
        return;
    }

    GDRHistoryItem* item = GDRHistoryItemArray_get(instance->data, idx);
    const char* arena = furi_string_get_cstr(instance->text_arena);
    const char* str = arena + item->text_offset;
    size_t remaining = item->text_len;

    size_t len = 0;
    while(len < remaining && str[len] != '\r' && str[len] != '\n') {
        len++;
    }

    uint16_t display_idx = idx + 1;
    const char* source_tag = "";
    if(item->source == GDRHistorySourceExternal) {
        source_tag = "[E] ";
    } else if(item->source == GDRHistorySourceInternal) {
        source_tag = "[I] ";
    }
    furi_string_printf(output, "%u. %s%.*s", display_idx, source_tag, (int)len, str);
}

void gdr_history_get_text_item_detail(
    GDRHistory* instance,
    uint16_t idx,
    FuriString* output,
    SubGhzEnvironment* environment) {
    furi_check(instance);
    furi_check(output);
    UNUSED(environment);

    if(idx >= GDRHistoryItemArray_size(instance->data)) {
        furi_string_set(output, "---");
        return;
    }

    GDRHistoryItem* item = GDRHistoryItemArray_get(instance->data, idx);
    const char* arena = furi_string_get_cstr(instance->text_arena);
    furi_string_set_strn(output, arena + item->text_offset, item->text_len);
}

FlipperFormat* gdr_history_get_raw_data(GDRHistory* instance, uint16_t idx) {
    furi_check(instance);

    if(idx >= GDRHistoryItemArray_size(instance->data)) {
        return NULL;
    }

    if(instance->loaded_idx == (int16_t)idx && instance->loaded_ff) {
        return instance->loaded_ff;
    }

    gdr_history_release_scratch(instance);

    GDRHistoryItem* item = GDRHistoryItemArray_get(instance->data, idx);
    gdr_history_build_path(instance, item->seq_id, instance->scratch_path);

    instance->loaded_ff = flipper_format_file_alloc(instance->storage);
    furi_check(instance->loaded_ff);
    if(!flipper_format_file_open_existing(
           instance->loaded_ff, furi_string_get_cstr(instance->scratch_path))) {
        FURI_LOG_E(
            TAG, "Failed open history capture %s", furi_string_get_cstr(instance->scratch_path));
        flipper_format_free(instance->loaded_ff);
        instance->loaded_ff = NULL;
        return NULL;
    }
    instance->loaded_idx = (int16_t)idx;
    return instance->loaded_ff;
}

void gdr_history_set_item_str(GDRHistory* instance, uint16_t idx, const char* str) {
    furi_check(instance);
    furi_check(str);

    if(idx >= GDRHistoryItemArray_size(instance->data)) {
        return;
    }

    GDRHistoryItem* item = GDRHistoryItemArray_get(instance->data, idx);
    uint16_t old_offset = item->text_offset;
    uint16_t old_len = item->text_len;

    gdr_history_arena_remove(instance, old_offset, old_len);

    size_t new_offset = furi_string_size(instance->text_arena);
    size_t new_len = strlen(str);
    furi_check(new_offset <= UINT16_MAX);
    furi_check(new_len <= UINT16_MAX);
    furi_string_cat_str(instance->text_arena, str);
    item->text_offset = (uint16_t)new_offset;
    item->text_len = (uint16_t)new_len;
}
