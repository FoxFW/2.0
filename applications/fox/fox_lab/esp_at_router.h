#pragma once

#include "esp_at.h"
#include <stdbool.h>

/* Sits between esp_at_receive() and every caller in this app that wants
 * lines off the shared UART. esp_at_receive() is a single-consumer queue -
 * only one reader can ever exist - but this app now has two independent
 * consumers once the FoxLAB companion service (foxr_companion.h) is live:
 * the existing blocking request/reply callers (the "info" probe, [LAB/
 * STATUS], the Launcher's Start/Stop toggle - see main.c/launcher_view.c)
 * and the new background dispatcher for unprompted [FLPR/...] companion
 * commands relayed over from the browser via the ESP32.
 *
 * This router becomes the ONE actual esp_at_receive() consumer for the
 * whole app session: a background thread reads every line, and either
 * dispatches it (synchronously, on that same thread) to the FLPR handler
 * if it's a "[FLPR/" line, or forwards it into its own queue otherwise.
 * Every other call site in this app switches from calling esp_at_receive()
 * directly to calling esp_at_router_wait_line() below - same blocking
 * semantics, same timeout behavior, just reading from the router's
 * forwarding queue instead of esp_at's own queue. Nothing about the
 * existing [LAB/...] request/reply matching logic (wait_for_line(),
 * wait_for_line_prefix()) needs to change beyond that one swap. */

typedef struct EspAtRouter EspAtRouter;

/* Called on the router's own background thread for every line that starts
 * with "[FLPR/" - context is whatever was passed to esp_at_router_alloc().
 * Handlers run synchronously here, one at a time (the router thread does
 * nothing else while a handler is running) - that's fine for this app's
 * FLPR command set (see foxr_companion.c), but a handler should not block
 * indefinitely. */
typedef void (*EspAtRouterFlprHandler)(void* context, const EspAtMsg* msg);

EspAtRouter*
    esp_at_router_alloc(EspAt* esp_at, EspAtRouterFlprHandler flpr_handler, void* context);
void esp_at_router_free(EspAtRouter* router);

/* Drop-in replacement for esp_at_receive() from every OTHER call site in
 * this app - same semantics (blocks up to timeout_ms, returns true and
 * fills *out on a line, false on timeout). */
bool esp_at_router_wait_line(EspAtRouter* router, EspAtMsg* out, uint32_t timeout_ms);
