#pragma once

#include "predator_models.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Get the count of hardcoded car models (90 models from car_models_128.csv)
size_t predator_models_get_hardcoded_count(void);

// Get a hardcoded car model by index (0 to 89)
const PredatorCarModel* predator_models_get_hardcoded(size_t index);

// Check security type for attack logic
bool predator_models_uses_rolling_code(size_t index);
bool predator_models_uses_smart_key(size_t index);
bool predator_models_uses_fixed_code(size_t index);

// Continent detection
CarContinent predator_models_get_continent(size_t index);
const char* predator_models_get_continent_name(CarContinent continent);
bool predator_models_is_continent(size_t index, CarContinent continent);

#ifdef __cplusplus
}
#endif
