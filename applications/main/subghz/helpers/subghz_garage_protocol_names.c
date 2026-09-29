#include "subghz_garage_protocol_names.h"
#include <string.h>
#include <furi/core/core_defines.h>

const char* const subghz_garage_protocol_group_names[SUBGHZ_GARAGE_PROTOCOL_GROUP_COUNT] = {
    "General",
    "General 2",
    "Chamberlain (USA)",
    "Linear (USA)",
    "Italian Brands 1",
    "Italian Brands 2",
    "Italian Brands 3",
    "Spain, Russia",
    "Germany",
    "China / Gate Only",
    "Blinds / Shutters",
};

const char* const subghz_garage_protocol_group_members[SUBGHZ_GARAGE_PROTOCOL_GROUP_COUNT] = {
    "SMC5326, Holtek, Holtek HT12X, Princeton",
    "X10, KeyFinder, KeeLoq",
    "Chamberlain Code, Security+ 1.0, Security+ 2.0",
    "Linear, LinearDelta3, MegaCode",
    "Beninca ARC, CAME, CAME Atomo, CAME TWEE",
    "KingGates Stylo4K, Nice FLO, Nice FloR-S, Phoenix_V2",
    "FAAC SLH, Roger, Doitrand, Telcoma/Cardin EDGE",
    "Alutech AT-4N, Ansonic, Clemsa, Mastercode",
    "Dickert MAHS, Hormann HSM, Marantec, Marantec24",
    "GangQi, GateTX, Hay21, Revers RB2",
    "Somfy Telis, Somfy Keytis, Jarolift, Dooya",
};

const char* const subghz_garage_protocol_names[] = {
    SUBGHZ_PROTOCOL_ALUTECH_AT_4N_NAME,
    SUBGHZ_PROTOCOL_SOMFY_TELIS_NAME,
    SUBGHZ_PROTOCOL_JAROLIFT_NAME,
    SUBGHZ_PROTOCOL_NICE_FLO_NAME,
    SUBGHZ_PROTOCOL_CAME_TWEE_NAME,
    SUBGHZ_PROTOCOL_SECPLUS_V1_NAME,
    SUBGHZ_PROTOCOL_SMC5326_NAME,
    SUBGHZ_PROTOCOL_DICKERT_MAHS_NAME,
    SUBGHZ_PROTOCOL_ROGER_NAME,
    SUBGHZ_PROTOCOL_KEYFINDER_NAME,
    SUBGHZ_PROTOCOL_SECPLUS_V2_NAME,
    SUBGHZ_PROTOCOL_GANGQI_NAME,
    SUBGHZ_PROTOCOL_HAY21_NAME,
    SUBGHZ_PROTOCOL_DOOYA_NAME,
    SUBGHZ_PROTOCOL_HOLTEK_NAME,
    SUBGHZ_PROTOCOL_CAME_NAME,
    SUBGHZ_PROTOCOL_NICE_FLOR_S_NAME,
    SUBGHZ_PROTOCOL_BENINCA_ARC_NAME,
    SUBGHZ_PROTOCOL_MARANTEC_NAME,
    SUBGHZ_PROTOCOL_HOLTEK_HT12X_NAME,
    SUBGHZ_PROTOCOL_REVERSRB2_NAME,
    SUBGHZ_PROTOCOL_MEGACODE_NAME,
    SUBGHZ_PROTOCOL_MARANTEC24_NAME,
    SUBGHZ_PROTOCOL_FAAC_SLH_NAME,
    SUBGHZ_PROTOCOL_CAME_ATOMO_NAME,
    SUBGHZ_PROTOCOL_CHAMB_CODE_NAME,
    SUBGHZ_PROTOCOL_MASTERCODE_NAME,
    SUBGHZ_PROTOCOL_LINEAR_NAME,
    SUBGHZ_PROTOCOL_LINEAR_DELTA3_NAME,
    SUBGHZ_PROTOCOL_GATE_TX_NAME,
    SUBGHZ_PROTOCOL_KINGGATES_STYLO_4K_NAME,
    SUBGHZ_PROTOCOL_SOMFY_KEYTIS_NAME,
    SUBGHZ_PROTOCOL_PHOENIX_V2_NAME,
    SUBGHZ_PROTOCOL_CLEMSA_NAME,
    SUBGHZ_PROTOCOL_ANSONIC_NAME,
    SUBGHZ_PROTOCOL_DOITRAND_NAME,
    SUBGHZ_PROTOCOL_HORMANN_HSM_NAME,
    SUBGHZ_PROTOCOL_X10_NAME,
    SUBGHZ_PROTOCOL_TELCOMA_EDGE_NAME,
};

const size_t subghz_garage_protocol_names_count = COUNT_OF(subghz_garage_protocol_names);

bool subghz_garage_protocol_name_is_garage(const char* protocol_name) {
    if(!protocol_name) return false;
    for(size_t i = 0; i < subghz_garage_protocol_names_count; i++) {
        if(strcmp(subghz_garage_protocol_names[i], protocol_name) == 0) {
            return true;
        }
    }
    return false;
}
