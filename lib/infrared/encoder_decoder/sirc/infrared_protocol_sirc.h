#pragma once

#include "../infrared_i.h"

void* infrared_decoder_sirc_alloc(void);
void infrared_decoder_sirc_reset(void* decoder);
InfraredMessage* infrared_decoder_sirc_check_ready(void* decoder);
void infrared_decoder_sirc_free(void* decoder);
InfraredMessage* infrared_decoder_sirc_decode(void* decoder, bool level, uint32_t duration);

void* infrared_encoder_sirc_alloc(void);
void infrared_encoder_sirc_reset(void* encoder_ptr, const InfraredMessage* message);
void infrared_encoder_sirc_free(void* decoder);
InfraredStatus infrared_encoder_sirc_encode(void* encoder_ptr, uint32_t* duration, bool* polarity);

const InfraredProtocolVariant* infrared_protocol_sirc_get_variant(InfraredProtocol protocol);
