#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/subghz/subghz_keystore.h>

typedef struct SubGhzKeeloqKeysManager SubGhzKeeloqKeysManager;

/** Allocate manager and load both system and user keystores */
SubGhzKeeloqKeysManager* subghz_keeloq_keys_alloc(void);

/** Free manager and keystores */
void subghz_keeloq_keys_free(SubGhzKeeloqKeysManager* m);

/** Total number of entries (user + system) */
size_t subghz_keeloq_keys_count(SubGhzKeeloqKeysManager* m);

/** Number of user-editable entries (indices 0..user_count-1).
 *  Indices user_count..count-1 are system read-only entries. */
size_t subghz_keeloq_keys_user_count(SubGhzKeeloqKeysManager* m);

/** Get pointer to entry at index i (valid until next add/delete) */
SubGhzKey* subghz_keeloq_keys_get(SubGhzKeeloqKeysManager* m, size_t i);

/** Append a new user entry */
void subghz_keeloq_keys_add(
    SubGhzKeeloqKeysManager* m,
    uint64_t key,
    uint16_t type,
    const char* name);

/** Overwrite user entry at index i (must be < user_count) */
void subghz_keeloq_keys_set(
    SubGhzKeeloqKeysManager* m,
    size_t i,
    uint64_t key,
    uint16_t type,
    const char* name);

/** Delete user entry at index i (must be < user_count) */
void subghz_keeloq_keys_delete(SubGhzKeeloqKeysManager* m, size_t i);

/** Write user entries back to the user keystore file (plaintext, Encryption:0) */
bool subghz_keeloq_keys_save(SubGhzKeeloqKeysManager* m);
