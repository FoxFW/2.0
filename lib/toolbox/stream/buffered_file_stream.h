#pragma once
#include <stdlib.h>
#include <storage/storage.h>
#include "stream.h"

#ifdef __cplusplus
extern "C" {
#endif

Stream* buffered_file_stream_alloc(Storage* storage);

bool buffered_file_stream_open(
    Stream* stream,
    const char* path,
    FS_AccessMode access_mode,
    FS_OpenMode open_mode);

bool buffered_file_stream_close(Stream* stream);

bool buffered_file_stream_sync(Stream* stream);

FS_Error buffered_file_stream_get_error(Stream* stream);

#ifdef __cplusplus
}
#endif
