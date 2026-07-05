#pragma once

#include <stdint.h>
#include <stdbool.h>

#define DESKTOP_PIN_CODE_MIN_LEN (1)
#define DESKTOP_PIN_CODE_MAX_LEN (10)
#define FOX_RECOVERY_FILE_PATH "/ext/System/Fox.data"
// 2 encoded bytes per digit + null terminator
#define DESKTOP_PIN_DATA_LEN     ((DESKTOP_PIN_CODE_MAX_LEN * 2) + 1)

// ── Fox.Settings unified file ──────────────────────────────────────────────
// Single file that holds ALL Fox security and settings data.
// Stored on BOTH internal flash AND SD card — firmware syncs them on boot.
// The SD copy is what the Python recovery tool reads and edits.
#define FOX_SETTINGS_EXT_PATH    "/ext/System/Fox.data"
#define FOX_SETTINGS_INT_PATH    "/int/Fox.data"
#define FOX_SETTINGS_MAGIC       0x464F5853u   // "FOXS"
#define FOX_SETTINGS_VERSION     1u
#define FOX_SETTINGS_XOR_KEY     0xAD
// Set by the Python tool to force this copy as authoritative regardless of age.
// Firmware clears it after importing so it is single-use.
#define FOX_SETTINGS_OVERRIDE    0xF0u

// Persistent lockout flag — kept separate so it can be written before the wipe reboot
#define FOX_LOCKOUT_FLAG_PATH    "/int/Fox.lock"
#define FOX_FORMAT_FLAG_PATH     "/int/Fox.fmt"  /* written at factory-reset; cleared after format */
#define FOX_PIN_FILE_MAGIC       (0xFE)

#define FOX_ESCROW_MAX_USED_TOKENS (8)
#define FOX_TOKEN_SIZE             (16)

// ── Unified Fox.Settings binary struct (packed, XOR encrypted) ──────────────
// Must stay in sync with fox_recovery_tool.py SETTINGS_FMT.
// Total size: 310 bytes.
typedef struct {
    uint32_t magic;             // FOX_SETTINGS_MAGIC
    uint16_t version;           // FOX_SETTINGS_VERSION
    uint8_t  override_flag;     // FOX_SETTINGS_OVERRIDE → import this copy on boot
    uint8_t  _pad;

    uint8_t  device_name[16];   // Flipper device name (null-padded)

    uint8_t  pin_hash[21];      // 2-byte-per-digit encoded PIN
    uint8_t  pin_length;        // byte count in pin_hash

    uint8_t  fail_count;        // current wrong attempts
    uint8_t  fail_limit;        // max attempts (0 = no limit)
    uint8_t  wipe_method;       // 0 = fake wipe, 1 = full wipe
    uint8_t  locked_out;        // 0xFF = locked out (WIPER_SENTINEL)

    uint32_t recovery_attempts_prime;   // fail_count × PRIME_MULT (Python tool)
    uint32_t recovery_verify_key;       // always 0xABCD1234
    uint8_t  recovery_token[16];        // single-use tech-support reset token

    uint8_t  used_tokens[8][16];        // consumed token history (replay prevention)

    uint8_t  build_hash[65];    // current firmware git hash (null-terminated)
    uint8_t  wizard_complete;   // 0 = not done, 1 = done

    uint32_t auto_lock_delay_ms;
    uint8_t  usb_inhibit_auto_lock;
    uint8_t  pin_exceed_action; // mirrors wipe_method for desktop settings

    uint8_t  reserved[32];

    uint32_t checksum;          // simple byte sum of all preceding bytes
} __attribute__((packed)) FoxSettingsData;

/* Fox.key and Fox.esc no longer exist as separate files.
 * PIN and escrow data are stored exclusively in Fox.data (FoxSettingsData).
 * FOX_PIN_PATH and FOX_ESCROW_PATH are kept below only for removal during
 * migration (storage_common_remove calls on first boot with new firmware). */
// ── Legacy per-file types (kept for migration) ──────────────────────────────

typedef struct {
    char    data[DESKTOP_PIN_DATA_LEN];
    uint8_t length;
} DesktopPinCode;

typedef struct {
    uint8_t active_fail_count;
    uint32_t secure_session_nonce;
    uint8_t hardware_uid_hash[16];
    uint8_t wipe_limit;
    uint8_t wipe_method;
    char used_tokens[FOX_ESCROW_MAX_USED_TOKENS][FOX_TOKEN_SIZE];
    uint8_t recorded_tokens_count;
} FoxEscrowData;

typedef struct {
    uint8_t device_name[16];
    uint32_t attempts_x_prime;
    uint32_t verification_key;
    char recovery_token[FOX_TOKEN_SIZE];
} FoxRecoveryData;

// --- LINKAGE PROTECTION LAYER START ---
#ifdef __cplusplus
extern "C" {
#endif

bool desktop_pin_code_is_set(void);
void desktop_pin_code_set(const DesktopPinCode* pin_code);
void desktop_pin_code_reset(void);
bool desktop_pin_code_check(const DesktopPinCode* pin_code);
bool desktop_pin_code_is_equal(const DesktopPinCode* pin_code1, const DesktopPinCode* pin_code2);
void desktop_pin_lock_error_notify(void);
uint32_t desktop_pin_lock_get_fail_timeout(void);
void desktop_pin_code_load_from_storage(void);

bool fox_escrow_load_and_verify(FoxEscrowData* escrow_out);
bool fox_escrow_save_state(const FoxEscrowData* escrow_in);
void fox_escrow_trigger_wiper_screen(void);
void fox_escrow_execute_wipe(void);

bool fox_recovery_check_and_reset(void);
bool fox_recovery_generate_file(uint8_t current_attempts);
bool fox_recovery_validate_and_register_token(const char* incoming_token);

// Fox.Settings unified file API
void fox_settings_write(void);          // write to both SD and internal
bool fox_settings_read(void);           // read from SD (or internal if SD copy missing)
bool fox_settings_import_override(void); // import SD copy if override_flag is set
void fox_settings_update_wizard(bool complete, const char* build_hash);
void fox_settings_update_desktop_settings(uint32_t auto_lock_ms, uint8_t usb_inhibit, uint8_t exceed_action);

// Fox.data SD↔INT sync — call from desktop.c on SD mount events
void fox_settings_sync_int_to_sd(void); // copy Fox.data int→SD when SD missing our copy
void fox_settings_sync_sd_to_int(void); // copy Fox.data SD→int when int missing our copy

#ifdef __cplusplus
}
#endif
// --- LINKAGE PROTECTION LAYER END ---