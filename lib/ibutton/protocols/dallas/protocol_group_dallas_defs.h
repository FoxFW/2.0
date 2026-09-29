#pragma once

#include "protocol_dallas_base.h"

typedef enum {
    iButtonProtocolDS1990,
    iButtonProtocolDS1992,
    iButtonProtocolDS1996,
    iButtonProtocolDS1971,
    iButtonProtocolDS1420,

    iButtonProtocolDSGeneric,
    iButtonProtocolDSMax,
} iButtonProtocolDallas;

extern const iButtonProtocolDallasBase* const ibutton_protocols_dallas[];
