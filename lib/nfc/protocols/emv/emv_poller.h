#pragma once

#include "emv.h"

#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EmvPoller EmvPoller;

typedef enum {
    EmvPollerEventTypeReadSuccess,
    EmvPollerEventTypeReadFailed,
} EmvPollerEventType;

typedef union {
    EmvError error;
} EmvPollerEventData;

typedef struct {
    EmvPollerEventType type;
    EmvPollerEventData* data;
} EmvPollerEvent;

EmvError emv_poller_select_ppse(EmvPoller* instance);

EmvError emv_poller_select_application(EmvPoller* instance);

EmvError emv_poller_get_processing_options(EmvPoller* instance);

EmvError emv_poller_read_sfi_record(EmvPoller* instance, uint8_t sfi, uint8_t record_num);

EmvError emv_poller_read_afl(EmvPoller* instance, bool bruteforce_sfi, uint16_t* readed_mask);

EmvError emv_poller_read_log_entry(EmvPoller* instance);

EmvError emv_poller_get_pin_try_counter(EmvPoller* instance);

EmvError emv_poller_get_last_online_atc(EmvPoller* instance);

#ifdef __cplusplus
}
#endif
