#pragma once

#include <stdint.h>
#include <stddef.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

// Continent classification for car brands
typedef enum {
    CarContinentEurope,
    CarContinentAsia,
    CarContinentAmerica,
    CarContinentCount
} CarContinent;

// Cryptographic protocols used by car remotes
typedef enum {
    CryptoProtocolNone,        // Fixed code replay (no crypto)
    CryptoProtocolKeeloq,      // 528-round rolling code (most common)
    CryptoProtocolHitag2,      // BMW/Audi/VW LFSR cipher
    CryptoProtocolAES128,      // Modern smart key
    CryptoProtocolTesla,       // Tesla-specific protocol
    CryptoProtocolCount
} CryptoProtocol;

typedef struct {
    char make[16];
    char model[40];
    uint32_t frequency; // Hz
    char remote_type[16];
    CarContinent continent; // Continent of manufacture
} PredatorCarModel;

// Returns number of models currently loaded from CSV (0 if none)
size_t predator_models_count(void);

// Returns pointer to model at index (NULL if out of range)
const PredatorCarModel* predator_models_get(size_t index);

// Clear any loaded models and free memory
void predator_models_clear(void);

// Load models from a CSV file at path. Returns number of models loaded (0 on failure)
// CSV format: make,model,frequency,remote_type
// frequency accepts integer Hz (e.g., 433920000) or with units (e.g., 433.92MHz, 315MHz, 868.35MHz)
size_t predator_models_load_csv(Storage* storage, const char* path);

// Load models from default path: /ext/apps_data/predator/car_models.csv
size_t predator_models_load_default(Storage* storage);

// ===== CRYPTO PROTOCOL DETECTION =====
// Detect correct cryptographic protocol based on model data (remote_type + frequency + make)
CryptoProtocol predator_models_get_protocol(size_t index);

// Get protocol name as string
const char* predator_models_get_protocol_name(CryptoProtocol protocol);

// Check if model uses specific protocol
bool predator_models_uses_protocol(size_t index, CryptoProtocol protocol);

#ifdef __cplusplus
}
#endif
