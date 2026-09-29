#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t frequency;
    const char* preset_name;
} TPMSVehicleRfCandidate;

typedef struct {
    const char* heading;
    const char* body;
} TPMSVehicleStep;

typedef struct {
    const char* make_name;
    const char* subtitle;
    const char* protocol_name;
    const TPMSVehicleRfCandidate* candidates;
    uint8_t candidate_count;
    const TPMSVehicleStep* steps;
    uint8_t step_count;
} TPMSVehicleGroup;

#define TPMS_VEHICLE_GROUP_COUNT 5

extern const TPMSVehicleGroup tpms_vehicle_groups[TPMS_VEHICLE_GROUP_COUNT];

#ifdef __cplusplus
}
#endif
