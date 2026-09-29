#pragma once

#include "../infrared_i.h"

void* infrared_decoder_samsung32_alloc(void);
void infrared_decoder_samsung32_reset(void* decoder);
void infrared_decoder_samsung32_free(void* decoder);
InfraredMessage* infrared_decoder_samsung32_check_ready(void* ctx);
InfraredMessage* infrared_decoder_samsung32_decode(void* decoder, bool level, uint32_t duration);

InfraredStatus
    infrared_encoder_samsung32_encode(void* encoder_ptr, uint32_t* duration, bool* level);
void infrared_encoder_samsung32_reset(void* encoder_ptr, const InfraredMessage* message);
void* infrared_encoder_samsung32_alloc(void);
void infrared_encoder_samsung32_free(void* encoder_ptr);

const InfraredProtocolVariant* infrared_protocol_samsung32_get_variant(InfraredProtocol protocol);
