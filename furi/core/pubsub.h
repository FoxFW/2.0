#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*FuriPubSubCallback)(const void* message, void* context);

typedef struct FuriPubSub FuriPubSub;

typedef struct FuriPubSubSubscription FuriPubSubSubscription;

FuriPubSub* furi_pubsub_alloc(void);

void furi_pubsub_free(FuriPubSub* pubsub);

FuriPubSubSubscription*
    furi_pubsub_subscribe(FuriPubSub* pubsub, FuriPubSubCallback callback, void* callback_context);

void furi_pubsub_unsubscribe(FuriPubSub* pubsub, FuriPubSubSubscription* pubsub_subscription);

void furi_pubsub_publish(FuriPubSub* pubsub, void* message);

#ifdef __cplusplus
}
#endif
