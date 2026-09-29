#include "tpms_vehicle_groups.h"
#include <furi.h>

static const TPMSVehicleRfCandidate k_ford_candidates[] = {
    {315000000, "FM238"},
    {433920000, "FM238"},
};

static const TPMSVehicleRfCandidate k_renault_candidates[] = {
    {433920000, "FM238"},
    {433920000, "FM476"},
};

static const TPMSVehicleRfCandidate k_citroen_candidates[] = {
    {433920000, "FM238"},
    {433920000, "FM476"},
};

static const TPMSVehicleRfCandidate k_toyota_candidates[] = {
    {315000000, "FM238"},
    {315000000, "FM476"},
};

static const TPMSVehicleRfCandidate k_schrader_candidates[] = {
    {433920000, "AM650"},
    {433920000, "AM270"},
};

static const TPMSVehicleStep k_generic_steps[] = {

    {"Approach Vehicle",
     "\ecGo to ANY wheel and hold\n"
     "\ecthe Flippers back flat\n"
     "\ecagainst base of valve stem\n"
     "\ecFlipper will pulse a 125kHz\n"
     "\ecsignal to TPMS to activate\n"
     "\ecand begin scanning data."},
};

const TPMSVehicleGroup tpms_vehicle_groups[TPMS_VEHICLE_GROUP_COUNT] = {
    {
        .make_name = "Ford",
        .subtitle = "Fiesta, Focus, Kuga, Transit",
        .protocol_name = "Ford TPMS",
        .candidates = k_ford_candidates,
        .candidate_count = COUNT_OF(k_ford_candidates),
        .steps = k_generic_steps,
        .step_count = COUNT_OF(k_generic_steps),
    },
    {
        .make_name = "Renault",
        .subtitle = "Clio, Captur, Zoe, Dacia Sandero",
        .protocol_name = "Renault TPMS",
        .candidates = k_renault_candidates,
        .candidate_count = COUNT_OF(k_renault_candidates),
        .steps = k_generic_steps,
        .step_count = COUNT_OF(k_generic_steps),
    },
    {
        .make_name = "Citroen / Peugeot",
        .subtitle = "Shared VDO-type sensor",
        .protocol_name = "Citroen TPMS",
        .candidates = k_citroen_candidates,
        .candidate_count = COUNT_OF(k_citroen_candidates),
        .steps = k_generic_steps,
        .step_count = COUNT_OF(k_generic_steps),
    },
    {
        .make_name = "Toyota / Lexus",
        .subtitle = "PMV-107J sensor",
        .protocol_name = "Toyota PMV-107J",
        .candidates = k_toyota_candidates,
        .candidate_count = COUNT_OF(k_toyota_candidates),
        .steps = k_generic_steps,
        .step_count = COUNT_OF(k_generic_steps),
    },
    {
        .make_name = "Kia / Mercedes / Other",
        .subtitle = "Generic Schrader GG4 sensor",
        .protocol_name = "Schrader GG4",
        .candidates = k_schrader_candidates,
        .candidate_count = COUNT_OF(k_schrader_candidates),
        .steps = k_generic_steps,
        .step_count = COUNT_OF(k_generic_steps),
    },
};
