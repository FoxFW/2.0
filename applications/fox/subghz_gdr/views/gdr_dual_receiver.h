// views/gdr_dual_receiver.h
#pragma once

#include "../helpers/gdr_types.h"

#ifdef ENABLE_DUAL_RX_SCENE

#include <gui/view.h>

typedef struct GDRDualReceiver GDRDualReceiver;
typedef struct GDRHistory GDRHistory;

typedef void (*GDRDualReceiverCallback)(GDRCustomEvent event, void* context);

GDRDualReceiver* gdr_view_dual_receiver_alloc(void);
void gdr_view_dual_receiver_free(GDRDualReceiver* receiver);
View* gdr_view_dual_receiver_get_view(GDRDualReceiver* receiver);

void gdr_view_dual_receiver_set_callback(
    GDRDualReceiver* receiver,
    GDRDualReceiverCallback callback,
    void* context);

void gdr_view_dual_receiver_set_history(
    GDRDualReceiver* receiver,
    GDRHistory* history);

void gdr_view_dual_receiver_set_history_mutex(
    GDRDualReceiver* receiver,
    FuriMutex* mutex);

void gdr_view_dual_receiver_set_chain_status(
    GDRDualReceiver* receiver,
    uint8_t slot,
    const char* tag,
    const char* frequency_str,
    const char* modulation_str,
    bool external);

void gdr_view_dual_receiver_set_rssi(
    GDRDualReceiver* receiver,
    uint8_t slot,
    float rssi);

void gdr_view_dual_receiver_set_history_stat(
    GDRDualReceiver* receiver,
    const char* history_stat_str);

void gdr_view_dual_receiver_sync_menu_from_history(
    GDRDualReceiver* receiver,
    GDRHistory* history);

void gdr_view_dual_receiver_reset_menu(GDRDualReceiver* receiver);

uint16_t gdr_view_dual_receiver_get_idx_menu(GDRDualReceiver* receiver);
void gdr_view_dual_receiver_set_idx_menu(GDRDualReceiver* receiver, uint16_t idx);
void gdr_view_dual_receiver_delete_item(GDRDualReceiver* receiver, uint16_t idx);

#endif // ENABLE_DUAL_RX_SCENE
