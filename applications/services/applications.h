#pragma once

#include <furi.h>
#include <gui/icon.h>

// v2.0 Stub includes for Archive and Dolphin
#include "desktop/archive_stub.h"


typedef enum {
    FlipperInternalApplicationFlagDefault = 0,
    FlipperInternalApplicationFlagInsomniaSafe = (1 << 0),
} FlipperInternalApplicationFlag;

typedef struct {
    const FuriThreadCallback app;
    const char* name;
    const char* appid;
    const size_t stack_size;
    const Icon* icon;
    const FlipperInternalApplicationFlag flags;
} FlipperInternalApplication;

typedef struct {
    const char* name;
    const Icon* icon;
    const char* path;
} FlipperExternalApplication;

typedef void (*FlipperInternalOnStartHook)(void);

extern const char* FLIPPER_AUTORUN_APP_NAME;

/* Services list
 * Spawned on startup
 */
extern const FlipperInternalApplication FLIPPER_SERVICES[];
extern const size_t FLIPPER_SERVICES_COUNT;

/* Apps list
 * Spawned by loader
 */
extern const FlipperInternalApplication FLIPPER_APPS[];
extern const size_t FLIPPER_APPS_COUNT;

/* On system start hooks
 * Called by loader, after OS initialization complete
 */
extern const FlipperInternalOnStartHook FLIPPER_ON_SYSTEM_START[];
extern const size_t FLIPPER_ON_SYSTEM_START_COUNT;

/* System apps
 * Can only be spawned by loader by name
 */
extern const FlipperInternalApplication FLIPPER_SYSTEM_APPS[];
extern const size_t FLIPPER_SYSTEM_APPS_COUNT;

/* Debug apps 
 * Can only be spawned by loader by name
 */
extern const FlipperInternalApplication FLIPPER_DEBUG_APPS[];
extern const size_t FLIPPER_DEBUG_APPS_COUNT;

/* v2.0: FLIPPER_ARCHIVE is now a stub
 * Archive runs as external .fap file instead of internal app
 * The stub is declared in applications/services/desktop/archive_stub.h
 * and defined in applications/services/desktop/archive_stub.c
 * (declared via #include "desktop/archive_stub.h" above)
 */

/* Settings list
 * Spawned by loader
 */
extern const FlipperInternalApplication FLIPPER_SETTINGS_APPS[];
extern const size_t FLIPPER_SETTINGS_APPS_COUNT;

/* External Settings list
 * Spawned by loader
 */
extern const FlipperExternalApplication FLIPPER_EXTSETTINGS_APPS[];
extern const size_t FLIPPER_EXTSETTINGS_APPS_COUNT;

/* External Menu Apps list
 * Spawned by loader
 */
extern const FlipperExternalApplication FLIPPER_EXTERNAL_APPS[];
extern const size_t FLIPPER_EXTERNAL_APPS_COUNT;
