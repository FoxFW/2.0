#pragma once
#include "flipper_format_stream.h"

static const char flipper_format_delimiter = ':';
static const char flipper_format_comment = '#';
static const char flipper_format_eoln = '\n';
static const char flipper_format_eolr = '\r';

#ifdef __cplusplus
extern "C" {
#endif

bool flipper_format_stream_write_eol(Stream* stream);

bool flipper_format_stream_seek_to_key(Stream* stream, const char* key, bool strict_mode);

#ifdef __cplusplus
}
#endif
