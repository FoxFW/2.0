#pragma once

#include "esp_at.h"
#include <gui/gui.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* FoxLAB Companion: the Flipper-side handler for the Fox Remote ("FLPR")
 * protocol - see the project's Fox Remote Protocol design doc for the full
 * wire format. This is what actually answers every "[FLPR/...]" line the
 * esp_at_router (esp_at_router.h) hands it: device/power/storage info,
 * file browsing, screen streaming, remote input, clock read/write, reboot,
 * and (task #54) notification/locale/menu-theme settings, all built
 * directly on this firmware's own Storage/Gui/Input/Notification/Locale
 * APIs - no RPC session, no Expansion protocol, just more lines on the
 * same AT-command UART [LAB/...] already uses.
 *
 * One instance lives for the whole app session (allocated right after a
 * successful ESP32 probe, alongside the esp_at_router - see main.c),
 * registered as the router's FLPR handler. Its dispatch function runs on
 * the router's background thread, one command at a time - see esp_at_
 * router.h's header comment. */

typedef struct FoxrCompanion FoxrCompanion;

#define FOXR_LOG_LINES     6
#define FOXR_LOG_LINE_MAX  40

/* `restart_pending_flag` (may be NULL): a flag this companion sets to true
 * whenever a Device Name change is saved that actually needs a restart to
 * become visible (see foxr_handle_settings_system_set() in foxr_companion.c
 * for why that's no longer done immediately - short version: it would also
 * power-cycle the attached ESP32 and drop the very FoxLAB connection being
 * used to make the change). Owned by the caller (App, in app.h) - lives
 * longer than any single FoxrCompanion, since foxr_companion_alloc() can be
 * called again on ESP32 reconnect (see main.c's FOX_LAB_EVENT_SERIAL_DO_
 * RETRY), and a pending restart from before a reconnect should still be
 * remembered after it. A raw volatile bool*, not an App*, since app.h
 * already includes this header - the reverse include would be circular. */
FoxrCompanion* foxr_companion_alloc(EspAt* esp_at, Gui* gui, volatile bool* restart_pending_flag);

/* Tears down any in-progress screen stream / open write file and releases
 * any held (pressed-but-not-released) input keys before freeing - same
 * spirit as the stock RPC layer's own session teardown. Safe to call at
 * any time, including with nothing active. */
void foxr_companion_free(FoxrCompanion* companion);

/* Matches EspAtRouterFlprHandler's signature (esp_at_router.h) - pass this
 * plus the FoxrCompanion* as context to esp_at_router_alloc(). */
void foxr_companion_dispatch(void* context, const EspAtMsg* msg);

/* Activity log for the FoxLAB Companion screen (flpr_view.h): a small ring
 * of the most recent FLPR commands handled, newest last. `log_version`
 * increments on every new entry - a caller (the view's redraw timer) can
 * poll it cheaply and only copy the snapshot out when it's actually
 * changed. Thread-safe (guarded internally) - safe to call from any
 * thread, not just the router's.
 *
 * Every entry that lands here is also appended, in full and with more
 * detail (a timestamp, the complete request/reply, and a heap snapshot),
 * to a persistent log file on the SD card (see FOXR_LOG_FILE in
 * foxr_companion.c) - so the complete history survives even though this
 * in-memory ring only ever keeps the last few and the on-screen view only
 * shows fewer still. There's no public accessor for the file itself; it's
 * meant to be pulled off the SD card directly for review. */
uint32_t foxr_companion_log_version(FoxrCompanion* companion);
void foxr_companion_log_snapshot(
    FoxrCompanion* companion,
    char out[FOXR_LOG_LINES][FOXR_LOG_LINE_MAX],
    size_t* out_count);
