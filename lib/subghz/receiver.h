#pragma once

#include "types.h"
#include "protocols/base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SubGhzReceiver SubGhzReceiver;

typedef void (*SubGhzReceiverCallback)(
    SubGhzReceiver* decoder,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context);

typedef bool (*SubGhzReceiverProtocolEnabledCallback)(
    void* context,
    size_t registry_index,
    const char* protocol_name);

SubGhzReceiver* subghz_receiver_alloc_init(SubGhzEnvironment* environment);

void subghz_receiver_free(SubGhzReceiver* instance);

void subghz_receiver_decode(SubGhzReceiver* instance, bool level, uint32_t duration);

void subghz_receiver_reset(SubGhzReceiver* instance);

void subghz_receiver_set_rx_callback(
    SubGhzReceiver* instance,
    SubGhzReceiverCallback callback,
    void* context);

void subghz_receiver_set_filter(SubGhzReceiver* instance, SubGhzProtocolFlag filter);

void subghz_receiver_set_modulation_filter(
    SubGhzReceiver* instance,
    SubGhzProtocolFlag modulation_filter);

void subghz_receiver_set_protocol_enabled_callback(
    SubGhzReceiver* instance,
    SubGhzReceiverProtocolEnabledCallback callback,
    void* context);

SubGhzProtocolDecoderBase*
    subghz_receiver_search_decoder_base_by_name(SubGhzReceiver* instance, const char* decoder_name);

#ifdef __cplusplus
}
#endif
