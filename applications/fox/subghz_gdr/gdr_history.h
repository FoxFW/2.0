// gdr_history.h
#pragma once

#include <stddef.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/protocols/base.h>

#define GDR_HISTORY_MAX 10

typedef struct SubGhzEnvironment SubGhzEnvironment;
typedef struct GDRHistory GDRHistory;

typedef enum {
    GDRHistorySourceUnknown = 0,
    GDRHistorySourceExternal,
    GDRHistorySourceInternal,
    GDRHistorySourceCount,
} GDRHistorySource;

GDRHistory* gdr_history_alloc(void);
void gdr_history_free(GDRHistory* instance);
void gdr_history_reset(GDRHistory* instance);
uint16_t gdr_history_get_item(GDRHistory* instance);
uint16_t gdr_history_get_last_index(GDRHistory* instance);
GDRHistorySource gdr_history_get_source(
    GDRHistory* instance,
    uint16_t idx);
const char* gdr_history_source_name(GDRHistorySource source);
void gdr_history_format_status_text(
    GDRHistory* instance,
    char* output,
    size_t output_size);
void gdr_history_get_status_text(GDRHistory* instance, FuriString* output);

bool gdr_history_get_capture_path(
    GDRHistory* instance,
    uint16_t idx,
    FuriString* out_path);
bool gdr_history_capture_path_equals(
    GDRHistory* instance,
    uint16_t idx,
    const char* path);

bool gdr_history_add_to_history(
    GDRHistory* instance,
    void* context,
    SubGhzRadioPreset* preset,
    GDRHistorySource source);
void gdr_history_delete_item(GDRHistory* instance, uint16_t idx);
void gdr_history_get_text_item_menu(
    GDRHistory* instance,
    FuriString* output,
    uint16_t idx);
void gdr_history_get_text_item_detail(
    GDRHistory* instance,
    uint16_t idx,
    FuriString* output,
    SubGhzEnvironment* environment);
FlipperFormat* gdr_history_get_raw_data(GDRHistory* instance, uint16_t idx);

void gdr_history_release_scratch(GDRHistory* instance);

void gdr_history_set_item_str(GDRHistory* instance, uint16_t idx, const char* str);
