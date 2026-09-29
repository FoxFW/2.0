#pragma once

#include "../infrared_i.h"

void* infrared_decoder_pioneer_alloc(void);
void infrared_decoder_pioneer_reset(void* decoder);
InfraredMessage* infrared_decoder_pioneer_check_ready(void* decoder);
void infrared_decoder_pioneer_free(void* decoder);
InfraredMessage* infrared_decoder_pioneer_decode(void* decoder, bool level, uint32_t duration);

void* infrared_encoder_pioneer_alloc(void);
void infrared_encoder_pioneer_reset(void* encoder_ptr, const InfraredMessage* message);
void infrared_encoder_pioneer_free(void* decoder);
InfraredStatus
    infrared_encoder_pioneer_encode(void* encoder_ptr, uint32_t* duration, bool* polarity);

const InfraredProtocolVariant* infrared_protocol_pioneer_get_variant(InfraredProtocol protocol);
