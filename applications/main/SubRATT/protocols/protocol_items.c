#include "protocol_items.h"

static const SubGhzProtocol* const subratt_subghz_protocol_registry_items[] = {
    &subghz_protocol_came,
    &subghz_protocol_nice_flo,
    &subghz_protocol_chamb_code,
    &subghz_protocol_linear,
    &subghz_protocol_ansonic,
    &subghz_protocol_smc5326,
    &subghz_protocol_holtek_th12x,
    &subghz_protocol_princeton,
    &subghz_protocol_gate_tx,
    &subghz_protocol_marantec24,
    &subghz_protocol_legrand,
    &subghz_protocol_clemsa,
    &subghz_protocol_bett,
    &subghz_protocol_megacode,

    &subghz_protocol_doitrand,
    &subghz_protocol_feron,
    &subghz_protocol_gangqi,
    &subghz_protocol_hollarm,
    &subghz_protocol_honeywell,
    &subghz_protocol_intertechno_v3,
    &subghz_protocol_magellan,
    &subghz_protocol_mastercode,
    &subghz_protocol_x10,
};

const SubGhzProtocolRegistry subratt_subghz_protocol_registry = {
    .items = subratt_subghz_protocol_registry_items,
    .size = COUNT_OF(subratt_subghz_protocol_registry_items)};
