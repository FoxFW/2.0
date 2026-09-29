#pragma once

#include <nfc/protocols/mf_plus/mf_plus.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MfPlusKeyCache MfPlusKeyCache;

MfPlusKeyCache* mf_plus_key_cache_alloc(void);

void mf_plus_key_cache_free(MfPlusKeyCache* instance);

bool mf_plus_key_cache_save(const MfPlusData* data);

bool mf_plus_key_cache_load(MfPlusKeyCache* instance, const uint8_t* uid, size_t uid_len);

bool mf_plus_key_cache_get_sector_key(
    const MfPlusKeyCache* instance,
    uint8_t sector,
    MfPlusKeyType key_type,
    MfPlusKey* out_key);

bool mf_plus_key_cache_get_admin_key(
    const MfPlusKeyCache* instance,
    MfPlusAdminKeyType admin_type,
    MfPlusKey* out_key);

void mf_plus_key_cache_reset(MfPlusKeyCache* instance);

#ifdef __cplusplus
}
#endif
