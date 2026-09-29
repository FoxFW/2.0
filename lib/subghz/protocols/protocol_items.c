#include "protocol_items.h"

const SubGhzProtocol* const subghz_protocol_registry_items[] = {

    &subghz_protocol_princeton,

    &subghz_protocol_raw,

    &subghz_protocol_bin_raw,

    &subghz_protocol_vag,
    &subghz_protocol_porsche_cayenne,
    &subghz_protocol_ford_v0,
    &subghz_protocol_psa,
    &subghz_protocol_fiat_spa,
    &subghz_protocol_fiat_marelli,

    &subghz_protocol_subaru,
    &subghz_protocol_mazda_siemens,
    &subghz_protocol_kia_v0,
    &subghz_protocol_kia_v1,
    &subghz_protocol_kia_v2,
    &subghz_protocol_kia_v3_v4,
    &subghz_protocol_kia_v5,
    &subghz_protocol_kia_v6,
    &subghz_protocol_suzuki,
    &subghz_protocol_mitsubishi_v0,
    &subghz_protocol_mitsubishi_v0a,
    &subghz_protocol_star_line,
    &subghz_protocol_scher_khan,
    &subghz_protocol_sheriff_cfm,
    &subghz_protocol_chrysler,
    &subghz_protocol_kia_v7,
    &subghz_protocol_mazda_v0,
    &honda_static_protocol,
    &honda_v1_protocol,
    &honda_v2_protocol,
    &ford_protocol_v1,
    &ford_protocol_v2,
    &ford_protocol_v3,
    &fiat_protocol_v0,
    &fiat_v1_protocol,
    &fiat_v2_protocol,
    &renault_v0_protocol,
    &renault_v1_protocol,
    &subghz_protocol_gm,
    &subghz_protocol_toyota,

};

const SubGhzProtocolRegistry subghz_protocol_registry = {
    .items = subghz_protocol_registry_items,
    .size = COUNT_OF(subghz_protocol_registry_items)};
