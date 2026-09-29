#pragma once

#include "../infrared_i.h"

void* infrared_decoder_rc6_alloc(void);
void infrared_decoder_rc6_reset(void* decoder);
void infrared_decoder_rc6_free(void* decoder);
InfraredMessage* infrared_decoder_rc6_check_ready(void* ctx);
InfraredMessage* infrared_decoder_rc6_decode(void* decoder, bool level, uint32_t duration);

void* infrared_encoder_rc6_alloc(void);
void infrared_encoder_rc6_reset(void* encoder_ptr, const InfraredMessage* message);
void infrared_encoder_rc6_free(void* decoder);
InfraredStatus infrared_encoder_rc6_encode(void* encoder_ptr, uint32_t* duration, bool* polarity);

const InfraredProtocolVariant* infrared_protocol_rc6_get_variant(InfraredProtocol protocol);
