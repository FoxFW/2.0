// views/gdr_receiver.h
#pragma once

#include <gui/view.h>
#include "../helpers/gdr_types.h"

typedef struct GDRReceiver GDRReceiver;
typedef struct GDRHistory GDRHistory;

typedef void (*GDRReceiverCallback)(GDRCustomEvent event, void* context);

void gdr_view_receiver_set_callback(
    GDRReceiver* receiver,
    GDRReceiverCallback callback,
    void* context);

GDRReceiver* gdr_view_receiver_alloc(bool auto_save);
void gdr_view_receiver_free(GDRReceiver* receiver);
View* gdr_view_receiver_get_view(GDRReceiver* receiver);

void gdr_view_receiver_add_data_statusbar(
    GDRReceiver* receiver,
    const char* frequency_str,
    const char* preset_str,
    const char* history_stat_str,
    bool external_radio);

uint16_t gdr_view_receiver_get_idx_menu(GDRReceiver* receiver);
void gdr_view_receiver_set_idx_menu(GDRReceiver* receiver, uint16_t idx);
void gdr_view_receiver_set_rssi(GDRReceiver* receiver, float rssi);
void gdr_view_receiver_set_lock(GDRReceiver* receiver, GDRLock lock);
void gdr_view_receiver_set_autosave(GDRReceiver* receiver, bool auto_save);
void gdr_view_receiver_set_history_mutex(
    GDRReceiver* receiver,
    FuriMutex* history_mutex);
void gdr_view_receiver_set_sub_decode_mode(
    GDRReceiver* receiver,
    bool sub_decode_mode);
void gdr_view_receiver_reset_menu(GDRReceiver* receiver);

void gdr_view_receiver_sync_menu_from_history(
    GDRReceiver* receiver,
    GDRHistory* history);

void gdr_view_receiver_pop_first_menu_item(GDRReceiver* receiver);
void gdr_view_receiver_delete_item(GDRReceiver* receiver, uint16_t idx);

void gdr_view_receiver_append_menu_row_from_history(
    GDRReceiver* receiver,
    GDRHistory* history,
    uint16_t idx);
