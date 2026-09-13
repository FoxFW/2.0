#include "esp_at_router.h"

#include <furi.h>
#include <string.h>
#include <stdlib.h>

#define ESP_AT_ROUTER_FWD_QUEUE_DEPTH 16
#define ESP_AT_ROUTER_POLL_MS         100
/* The FLPR dispatch handler (foxr_companion.c) runs synchronously on this
 * thread - see the typedef's own header comment - so this has to cover
 * its worst-case call depth too, not just the router's own bookkeeping.
 *
 * 3072 (previously 2048) was NOT enough - confirmed on real hardware
 * 2026-09-10/11: the very first FLPR command anywhere in the dashboard's
 * call sequence that formats a 64-bit value (FLPR/STORAGE's `%llu,%llu`
 * in foxr_replyf(), via foxr_handle_storage() -> storage_common_fs_info())
 * reliably crashed the whole device with a "MPU fault, possibly stack
 * overflow" reboot, every single time - FLPR/INFO and FLPR/POWER (both
 * %s-only replies, shallower call depth, no 64-bit formatting) ran fine
 * immediately before it. The base of every dispatch call chain already
 * carries a 512-byte EspAtMsg (this router thread's own `EspAtMsg msg`
 * local, esp_at.h's ESP_AT_LINE_MAX) before dispatch/handler/reply-
 * formatting frames stack on top of that - 3072 total left far too little
 * margin once a handler's reply needs real vsnprintf/64-bit-division work,
 * and foxr_handle_files_list() (FLPR/FILES/LIST) is a substantially bigger
 * stack consumer still (a 256-byte name buffer + a 352-byte base64 buffer
 * on top of the same call depth) that hadn't even been exercised yet when
 * this was found.
 *
 * 8192 matches this app's own main thread stack_size (application.fam) -
 * a known-safe budget for FoxLAB's Storage/Gui/Input-heavy work, rather
 * than guessing another "probably enough" number the way 3072 was. Costs
 * an extra 5KB of static RAM over the previous 3072, trivial on F7's
 * 256KB SRAM (same reasoning already used to justify smaller bumps to
 * this exact define). */
#define ESP_AT_ROUTER_THREAD_STACK    8192

struct EspAtRouter {
    EspAt* esp_at;
    FuriMessageQueue* fwd_queue;
    FuriThread* thread;
    volatile bool running;
    EspAtRouterFlprHandler flpr_handler;
    void* flpr_context;
};

static int32_t esp_at_router_thread(void* context) {
    EspAtRouter* router = context;
    EspAtMsg msg;

    while(router->running) {
        /* Short poll timeout, not the caller's timeout - this loop's own
         * job is just "keep checking `running`", not honoring anyone
         * else's deadline. */
        if(!esp_at_receive(router->esp_at, &msg, ESP_AT_ROUTER_POLL_MS)) continue;

        if(strncmp(msg.line, "[FLPR/", 6) == 0) {
            if(router->flpr_handler) router->flpr_handler(router->flpr_context, &msg);
        } else {
            /* Non-blocking put; if the forwarding queue is genuinely full
             * (nobody's been waiting on it), drop the oldest entry rather
             * than block the router - and therefore FLPR dispatch - on a
             * stuck or absent consumer. */
            if(furi_message_queue_put(router->fwd_queue, &msg, 0) != FuriStatusOk) {
                EspAtMsg discard;
                furi_message_queue_get(router->fwd_queue, &discard, 0);
                furi_message_queue_put(router->fwd_queue, &msg, 0);
            }
        }
    }

    return 0;
}

EspAtRouter*
    esp_at_router_alloc(EspAt* esp_at, EspAtRouterFlprHandler flpr_handler, void* context) {
    EspAtRouter* router = malloc(sizeof(EspAtRouter));
    router->esp_at = esp_at;
    router->fwd_queue = furi_message_queue_alloc(ESP_AT_ROUTER_FWD_QUEUE_DEPTH, sizeof(EspAtMsg));
    router->running = true;
    router->flpr_handler = flpr_handler;
    router->flpr_context = context;

    router->thread =
        furi_thread_alloc_ex("EspAtRouter", ESP_AT_ROUTER_THREAD_STACK, esp_at_router_thread, router);
    furi_thread_start(router->thread);

    return router;
}

void esp_at_router_free(EspAtRouter* router) {
    router->running = false;
    furi_thread_join(router->thread);
    furi_thread_free(router->thread);
    furi_message_queue_free(router->fwd_queue);
    free(router);
}

bool esp_at_router_wait_line(EspAtRouter* router, EspAtMsg* out, uint32_t timeout_ms) {
    return furi_message_queue_get(router->fwd_queue, out, timeout_ms) == FuriStatusOk;
}
