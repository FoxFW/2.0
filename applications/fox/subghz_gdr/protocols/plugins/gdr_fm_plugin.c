#include "../gdr_protocol_plugins.h"
#include "../ansonic.h"

static const SubGhzProtocol* const gdr_protocol_registry_fm_items[] = {
    &subghz_protocol_ansonic,
};

static const SubGhzProtocolRegistry gdr_protocol_registry_fm = {
    .items = gdr_protocol_registry_fm_items,
    .size = sizeof(gdr_protocol_registry_fm_items) /
            sizeof(gdr_protocol_registry_fm_items[0]),
};

static const GDRProtocolPlugin gdr_fm_plugin = {
    .plugin_name = "Garage Door Remote FM Registry",
    .filter = GDRProtocolRegistryFilterFM,
    .registry = &gdr_protocol_registry_fm,
};

static const FlipperAppPluginDescriptor gdr_fm_plugin_descriptor = {
    .appid = GDR_PROTOCOL_PLUGIN_APP_ID,
    .ep_api_version = GDR_PROTOCOL_PLUGIN_API_VERSION,
    .entry_point = &gdr_fm_plugin,
};

const FlipperAppPluginDescriptor* gdr_fm_plugin_ep(void) {
    return &gdr_fm_plugin_descriptor;
}
