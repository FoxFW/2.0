#include "dolphin.h"
#include <furi.h>

#define TAG "DolphinService"

struct Dolphin {
    FuriPubSub* pubsub;
};

void dolphin_deed(DolphinDeed deed) {
    UNUSED(deed);
    // Stubbed out: Deeds are ignored in FoxFW to maximize performance and save space
}

void dolphin_get_settings(Dolphin* dolphin, DolphinSettings* settings) {
    UNUSED(dolphin);
    if(settings) {
        settings->happy_mode = false;
    }
}

void dolphin_set_settings(Dolphin* dolphin, DolphinSettings* settings) {
    UNUSED(dolphin);
    UNUSED(settings);
}

DolphinStats dolphin_stats(Dolphin* dolphin) {
    UNUSED(dolphin);
    DolphinStats stats = {
        .icounter = 0,
        .butthurt = 0,
        .timestamp = 0,
        .level = 1,
        .level_up_is_pending = false
    };
    return stats;
}

void dolphin_flush(Dolphin* dolphin) {
    UNUSED(dolphin);
}

void dolphin_upgrade_level(Dolphin* dolphin) {
    UNUSED(dolphin);
}

FuriPubSub* dolphin_get_pubsub(Dolphin* dolphin) {
    furi_check(dolphin);
    return dolphin->pubsub;
}

// Background service loop entry point required by the application manager
int32_t dolphin_srv(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Initializing Minimal Subsystem");

    Dolphin* dolphin = malloc(sizeof(Dolphin));
    furi_check(dolphin);

    dolphin->pubsub = furi_pubsub_alloc();

    furi_record_create(RECORD_DOLPHIN, dolphin);

    FURI_LOG_I(TAG, "Subsystem Online");

    // Keep the service alive to fulfill pubsub queries from other core services
    while(true) {
        furi_delay_ms(1000);
    }

    return 0;
}