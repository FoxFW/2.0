#pragma once

#include <stdint.h>
#include <string.h>
#include <ble/core/ble_std.h>
#include <ble/core/ble_defs.h>
#include "osal.h"
#include "compiler.h"

#ifndef BASIC_FEATURES
#define BASIC_FEATURES 0
#endif
#ifndef SLAVE_ONLY
#define SLAVE_ONLY 0
#endif
#ifndef LL_ONLY
#define LL_ONLY 0
#endif
#ifndef LL_ONLY_BASIC
#define LL_ONLY_BASIC 0
#endif
#ifndef BEACON_ONLY
#define BEACON_ONLY 0
#endif

#define BLE_CMD_MAX_PARAM_LEN HCI_COMMAND_MAX_PARAM_LEN

#define BLE_EVT_MAX_PARAM_LEN HCI_EVENT_MAX_PARAM_LEN

struct hci_request {
    uint16_t ogf;
    uint16_t ocf;
    int event;
    void* cparam;
    int clen;
    void* rparam;
    int rlen;
};
extern int hci_send_req(struct hci_request* req, uint8_t async);

#ifndef FALSE
#define FALSE 0
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
