#include "../subghz_garage_protocol_plugin.h"
#include "../protocol_groups.h"

// Group 13 (new 2026-10-03): Ditec GOL4 (42-bit LCG rolling code) split into its
// own group - it is the largest of the new Unleashed gate decoders, kept apart so
// no single group's .fal exceeds the ~20 KB size envelope, with headroom for future
// additions. RX-only in garage (encoder compiled out under SUBGHZ_GARAGE_WITH_ENCODER).
static const SubGhzProtocol* const subghz_garage_protocol_registry_g13_items[] = {
    &subghz_protocol_raw,
    &subghz_protocol_bin_raw,
    &subghz_protocol_ditec_gol4,
};

const SubGhzProtocolRegistry subghz_garage_protocol_registry_g13 = {
    .items = subghz_garage_protocol_registry_g13_items,
    .size = COUNT_OF(subghz_garage_protocol_registry_g13_items)};

static const SubGhzGarageProtocolPlugin subghz_garage_protocol_plugin_g13 = {
    .registry = &subghz_garage_protocol_registry_g13,
};

static const FlipperAppPluginDescriptor subghz_garage_protocol_plugin_g13_descriptor = {
    .appid = SUBGHZ_GARAGE_PROTOCOL_PLUGIN_APP_ID,
    .ep_api_version = SUBGHZ_GARAGE_PROTOCOL_PLUGIN_API_VERSION,
    .entry_point = &subghz_garage_protocol_plugin_g13,
};

const FlipperAppPluginDescriptor* subghz_garage_protocol_plugin_g13_ep(void) {
    return &subghz_garage_protocol_plugin_g13_descriptor;
}
