#include "fox_usb_builder.h"
#include "fox_vfat.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>

#define FF_MIN_IMAGE_BYTES (16UL * 1024UL * 1024UL)
#define FF_MAX_IMAGE_BYTES (4000UL * 1024UL * 1024UL)
#define FF_SLACK_PER_FILE  (65536ULL)
#define FF_SLACK_FIXED     (4ULL * 1024ULL * 1024ULL)

typedef struct {
    char* rel;
    uint32_t size;
} FoxUsbEntry;

typedef struct {
    FoxUsbEntry* items;
    size_t count;
    size_t cap;
} FoxUsbList;

static void list_add(FoxUsbList* l, const char* rel, uint32_t size) {
    if(l->count == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 16;
        l->items = realloc(l->items, l->cap * sizeof(FoxUsbEntry));
    }
    l->items[l->count].rel = strdup(rel);
    l->items[l->count].size = size;
    l->count++;
}

static void list_free(FoxUsbList* l) {
    for(size_t i = 0; i < l->count; i++) free(l->items[i].rel);
    free(l->items);
}

static void fox_usb_scan(
    Storage* storage,
    FuriString* base_full,
    FuriString* rel_prefix,
    bool recursive,
    FoxUsbList* list) {
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, furi_string_get_cstr(base_full))) {
        FileInfo info;
        char name[256];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(name[0] == '\0' || name[0] == '.') continue;
            FuriString* child_rel = furi_string_alloc();
            if(furi_string_size(rel_prefix) > 0) {
                furi_string_printf(child_rel, "%s/%s", furi_string_get_cstr(rel_prefix), name);
            } else {
                furi_string_set_str(child_rel, name);
            }
            FuriString* child_full = furi_string_alloc();
            furi_string_printf(child_full, "%s/%s", furi_string_get_cstr(base_full), name);
            if(file_info_is_dir(&info)) {
                if(recursive) {
                    fox_usb_scan(storage, child_full, child_rel, recursive, list);
                }
            } else {
                list_add(list, furi_string_get_cstr(child_rel), (uint32_t)info.size);
            }
            furi_string_free(child_full);
            furi_string_free(child_rel);
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
}

static void fox_usb_make_dirs_for(FoxVfat* v, const char* rel) {
    FuriString* prefix = furi_string_alloc();
    for(const char* p = rel; *p != '\0'; p++) {
        if(*p == '/') {
            fox_vfat_build_mkdir(v, furi_string_get_cstr(prefix));
        }
        furi_string_push_back(prefix, *p);
    }
    furi_string_free(prefix);
}

FoxUsbBuildResult fox_usb_builder_run(
    const FoxUsbBuildConfig* cfg,
    FoxVfat** out_vfat,
    FoxUsbBuildProgress cb,
    void* cb_ctx) {
    Storage* storage = cfg->storage;
    *out_vfat = NULL;

    storage_common_mkdir(storage, cfg->source_dir);
    for(size_t i = 0; i < cfg->ensure_dir_count; i++) {
        FuriString* d = furi_string_alloc();
        furi_string_printf(d, "%s/%s", cfg->source_dir, cfg->ensure_dirs[i]);
        storage_common_mkdir(storage, furi_string_get_cstr(d));
        furi_string_free(d);
    }

    FoxUsbList list = {0};
    FuriString* base = furi_string_alloc_set_str(cfg->source_dir);
    FuriString* empty_prefix = furi_string_alloc();
    fox_usb_scan(storage, base, empty_prefix, cfg->recursive, &list);
    furi_string_free(empty_prefix);
    furi_string_free(base);

    uint64_t total = 0;
    for(size_t i = 0; i < list.count; i++) total += list.items[i].size;
    uint64_t bytes = total + (uint64_t)list.count * FF_SLACK_PER_FILE + FF_SLACK_FIXED;
    if(bytes < FF_MIN_IMAGE_BYTES) bytes = FF_MIN_IMAGE_BYTES;
    if(bytes > FF_MAX_IMAGE_BYTES) bytes = FF_MAX_IMAGE_BYTES;
    bytes = (bytes + 511ULL) & ~511ULL;

    FoxVfat* v = fox_vfat_alloc(storage, cfg->meta_path);
    if(v == NULL || !fox_vfat_build_begin(v, bytes, cfg->label)) {
        if(v) fox_vfat_free(v);
        list_free(&list);
        return FoxUsbBuildResultError;
    }

    for(size_t i = 0; i < cfg->ensure_dir_count; i++) {
        fox_vfat_build_mkdir(v, cfg->ensure_dirs[i]);
    }

    for(size_t i = 0; i < list.count; i++) {
        const char* rel = list.items[i].rel;
        const char* leaf = strrchr(rel, '/');
        leaf = leaf ? leaf + 1 : rel;
        if(cb != NULL) cb(cb_ctx, (uint32_t)i, (uint32_t)list.count, leaf);
        fox_usb_make_dirs_for(v, rel);
        FuriString* src_full = furi_string_alloc();
        furi_string_printf(src_full, "%s/%s", cfg->source_dir, rel);
        fox_vfat_build_add(v, rel, furi_string_get_cstr(src_full), list.items[i].size);
        furi_string_free(src_full);
    }
    if(cb != NULL) cb(cb_ctx, (uint32_t)list.count, (uint32_t)list.count, "");

    fox_vfat_build_end(v);
    list_free(&list);
    *out_vfat = v;
    return FoxUsbBuildResultBuilt;
}
