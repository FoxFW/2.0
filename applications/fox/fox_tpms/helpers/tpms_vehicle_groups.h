#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One {frequency, modulation} combination worth trying for a vehicle group.
 * `preset_name` matches the short keys tpms_set_preset() (tpms_app_i.c)
 * already maps FuriHalSubGhzPreset* onto - "AM650"/"AM270"/"FM238"/"FM476"/
 * "FM12K" - so these can be handed straight to tpms_preset_init() without
 * another translation layer. */
typedef struct {
    uint32_t frequency; /* Hz */
    const char* preset_name;
} TPMSVehicleRfCandidate;

/* One instruction screen in a vehicle group's guided activation sequence.
 * `heading` is the big top line ("Step 1"), `body` the instruction text -
 * every group currently uses exactly one step (LF activation doesn't
 * actually depend on wheel order - see the candidate/step data in
 * tpms_vehicle_groups.c for why), but the scene that walks through these
 * (tpms_scene_vehicle_steps.c) supports any step_count > 0 so a future
 * group with a genuine multi-step procedure doesn't need a scene rewrite. */
typedef struct {
    const char* heading;
    const char* body;
} TPMSVehicleStep;

typedef struct {
    const char* make_name; /* box title, e.g. "Ford" */
    const char* subtitle; /* box subtitle, e.g. "Fiesta, Focus, Kuga, Transit" */
    const char* protocol_name; /* TPMS_PROTOCOL_*_NAME this group decodes with - informational */
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
