#pragma once

#include <gui/view.h>
#include "../helpers/tpms_types.h"
#include "../helpers/tpms_event.h"
#include <lib/flipper_format/flipper_format.h>

typedef struct TPMSReceiverInfo TPMSReceiverInfo;

typedef enum {
    TPMSReceiverInfoActionEdit,
    TPMSReceiverInfoActionToggleBattery,
} TPMSReceiverInfoAction;

typedef void (*TPMSReceiverInfoActionCallback)(TPMSReceiverInfoAction action, void* context);

void tpms_view_receiver_info_update(TPMSReceiverInfo* tpms_receiver_info, FlipperFormat* fff);

void tpms_view_receiver_info_set_callback(
    TPMSReceiverInfo* tpms_receiver_info,
    TPMSReceiverInfoActionCallback callback,
    void* context);

TPMSField tpms_view_receiver_info_get_selected_field(TPMSReceiverInfo* tpms_receiver_info);

TPMSReceiverInfo* tpms_view_receiver_info_alloc();

void tpms_view_receiver_info_free(TPMSReceiverInfo* tpms_receiver_info);

View* tpms_view_receiver_info_get_view(TPMSReceiverInfo* tpms_receiver_info);
