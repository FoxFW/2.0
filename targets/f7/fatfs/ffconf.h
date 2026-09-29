#ifndef _FFCONF
#define _FFCONF 68300

#ifdef FURI_RAM_EXEC
#define _FS_READONLY 1
#else
#define _FS_READONLY 0
#endif

#define _FS_MINIMIZE 0

#define _USE_STRFUNC 0

#define _USE_FIND 0

#ifdef FURI_RAM_EXEC
#define _USE_MKFS 0
#else
#define _USE_MKFS 1
#endif

#define _USE_FASTSEEK 1

#define _USE_EXPAND 0

#define _USE_CHMOD 0

#define _USE_LABEL 1

#define _USE_FORWARD 0

#define _CODE_PAGE 850

#define _USE_LFN 2
#define _MAX_LFN 255

#define _LFN_UNICODE 0

#define _STRF_ENCODE 0

#define _FS_RPATH 0

#define _VOLUMES 1

#define _STR_VOLUME_ID 0
#define _VOLUME_STRS   "SD"

#define _MULTI_PARTITION 0

#define _MIN_SS          512
#define _MAX_SS          512

#define _USE_TRIM 0

#define _FS_NOFSINFO 0

#define _FS_TINY 0

#define _FS_EXFAT 1

#define _FS_NORTC 0

#define _FS_LOCK 0

#define _FS_REENTRANT 0
#define _FS_TIMEOUT   1000
#define _SYNC_t       FuriMutex*

#if !defined(ff_malloc) && !defined(ff_free)
#include <stdlib.h>
#define ff_malloc malloc
#define ff_free   free
#endif

#endif
