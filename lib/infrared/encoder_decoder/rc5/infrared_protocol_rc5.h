#pragma once

#include "../infrared_i.h"

void* infrared_decoder_rc5_alloc(void);
void infrared_decoder_rc5_reset(void* decoder);
void infrared_decoder_rc5_free(void* decoder);
InfraredMessage* infrared_decoder_rc5_check_ready(void* ctx);
InfraredMessage* infrared_decoder_rc5_decode(void* decoder, bool level, uint32_t duration);

void* infrared_encoder_rc5_alloc(void);
void infrared_encoder_rc5_reset(void* encoder_ptr, const InfraredMessage* message);
void infrared_encoder_rc5_free(void* decoder);
InfraredStatus infrared_encoder_rc5_encode(void* encoder_ptr, uint32_t* duration, bool* polarity);

const InfraredProtocolVariant* infrared_protocol_rc5_get_variant(InfraredProtocol protocol);
