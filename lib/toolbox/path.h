#pragma once
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

void path_extract_filename_no_ext(const char* path, FuriString* filename);

void path_extract_filename(FuriString* path, FuriString* filename, bool trim_ext);

void path_extract_extension(FuriString* path, char* ext, size_t ext_len_max);

void path_extract_basename(const char* path, FuriString* basename);

void path_extract_dirname(const char* path, FuriString* dirname);

void path_append(FuriString* path, const char* suffix);

void path_concat(const char* path, const char* suffix, FuriString* out_path);

bool path_contains_only_ascii(const char* path);

#ifdef __cplusplus
}
#endif
