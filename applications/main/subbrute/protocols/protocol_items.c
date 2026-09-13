#include "protocol_items.h" // IWYU pragma: keep

// Registry for SubBrute's own private SubGhz environment (see
// helpers/subbrute_worker.c and subbrute_device.c). Exactly the 8 protocols
// SubBrute's SubBruteBrand groups need, by the exact name strings
// subbrute_protocol_file_types[] in subbrute_protocols.c expects
// (SubGhzProtocolRegistry looks entries up by ->name, e.g. "CAME",
// "Cham_Code", "Holtek_HT12X" - not by this array's C identifiers).
static const SubGhzProtocol* const subbrute_subghz_protocol_registry_items[] = {
    &subghz_protocol_came,
    &subghz_protocol_nice_flo,
    &subghz_protocol_chamb_code,
    &subghz_protocol_linear,
    &subghz_protocol_ansonic,
    &subghz_protocol_smc5326,
    &subghz_protocol_holtek_th12x,
    &subghz_protocol_princeton,
};

const SubGhzProtocolRegistry subbrute_subghz_protocol_registry = {
    .items = subbrute_subghz_protocol_registry_items,
    .size = COUNT_OF(subbrute_subghz_protocol_registry_items)};
