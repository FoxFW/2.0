#include "subghz_protocol_groups.h"
#include <string.h>

const char* const subghz_protocol_group_names[SUBGHZ_PROTOCOL_GROUP_COUNT] = {
    "VAG",
    "Porsche",
    "PSA",
    "Ford",
    "Fiat",
    "Renault",
    "Mazda",
    "Kia/Hyundai",
    "Subaru",
    "Suzuki",
    "Mitsubishi",
    "Honda",
    "Chrysler/Dodge/Jeep",
    "Starline",
    "Scher-Khan",
    "Sheriff",
    "Other",
};

#define GROUP_VAG      0
#define GROUP_PORSCHE  1
#define GROUP_PSA      2
#define GROUP_FORD     3
#define GROUP_FIAT     4
#define GROUP_RENAULT  5
#define GROUP_MAZDA    6
#define GROUP_KIA      7
#define GROUP_SUBARU   8
#define GROUP_SUZUKI   9
#define GROUP_MITSUBISHI 10
#define GROUP_HONDA    11
#define GROUP_CHRYSLER 12
#define GROUP_STARLINE 13
#define GROUP_SCHERKHAN 14
#define GROUP_SHERIFF  15
#define GROUP_GENERAL  16

typedef struct {
    const char* protocol_name;
    size_t group;
} SubGhzProtocolGroupMapEntry;

static const SubGhzProtocolGroupMapEntry subghz_protocol_group_map[] = {
    {"RAW", GROUP_GENERAL},
    {"BinRAW", GROUP_GENERAL},
    {"VAG GROUP", GROUP_VAG},
    {"Porsche AG", GROUP_PORSCHE},
    {"FORD V0", GROUP_FORD},
    {"PSA GROUP", GROUP_PSA},
    {"FIAT SPA", GROUP_FIAT},
    {"MARELLI", GROUP_FIAT},
    {"SUBARU", GROUP_SUBARU},
    {"MazdaSiemens", GROUP_MAZDA},
    {"KIA/HYU V0", GROUP_KIA},
    {"KIA/HYU V1", GROUP_KIA},
    {"KIA/HYU V2", GROUP_KIA},
    {"KIA/HYU V3/V4", GROUP_KIA},
    {"KIA/HYU V5", GROUP_KIA},
    {"KIA/HYU V6", GROUP_KIA},
    {"SUZUKI", GROUP_SUZUKI},
    {"Mitsubishi V0", GROUP_MITSUBISHI},
    {"Mitsubishi V0-a", GROUP_MITSUBISHI},
    {"Star Line", GROUP_STARLINE},
    {"Scher-Khan", GROUP_SCHERKHAN},
    {"Sheriff CFM", GROUP_SHERIFF},
    {"Chrysler", GROUP_CHRYSLER},
    {"Kia V7", GROUP_KIA},
    {"Mazda V0", GROUP_MAZDA},
    {"Honda Static", GROUP_HONDA},
    {"Honda V1", GROUP_HONDA},
    {"Honda V2", GROUP_HONDA},
    {"Ford V1", GROUP_FORD},
    {"Ford V2", GROUP_FORD},
    {"Ford V3", GROUP_FORD},
    {"Fiat V0", GROUP_FIAT},
    {"Fiat V1", GROUP_FIAT},
    {"Fiat V2", GROUP_FIAT},
    {"Renault V0", GROUP_RENAULT},
};

size_t subghz_protocol_group_for_name(const char* protocol_name) {
    for(size_t i = 0; i < sizeof(subghz_protocol_group_map) / sizeof(subghz_protocol_group_map[0]);
        i++) {
        if(strcmp(subghz_protocol_group_map[i].protocol_name, protocol_name) == 0) {
            return subghz_protocol_group_map[i].group;
        }
    }
    return GROUP_GENERAL;
}
