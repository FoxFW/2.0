#include "esp_at_router.h"

#include <furi.h>
#include <string.h>
#include <stdlib.h>

#define ESP_AT_ROUTER_FWD_QUEUE_DEPTH 16
#define ESP_AT_ROUTER_POLL_MS         100

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

        if(!esp_at_receive(router->esp_at, &msg, ESP_AT_ROUTER_POLL_MS)) continue;

        if(strncmp(msg.line, "[FLPR/", 6) == 0) {
            if(router->flpr_handler) router->flpr_handler(router->flpr_context, &msg);
        } else {

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
    furi_kernel_lock();
    furi_thread_free(router->thread);
    furi_message_queue_free(router->fwd_queue);
    free(router);
    furi_kernel_unlock();
}

bool esp_at_router_wait_line(EspAtRouter* router, EspAtMsg* out, uint32_t timeout_ms) {
    return furi_message_queue_get(router->fwd_queue, out, timeout_ms) == FuriStatusOk;
}
