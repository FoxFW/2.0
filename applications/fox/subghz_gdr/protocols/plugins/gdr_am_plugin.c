#include "../gdr_protocol_plugins.h"

#include "../alutech_at_4n.h"
#include "../beninca_arc.h"
#include "../came.h"
#include "../came_atomo.h"
#include "../came_twee.h"
#include "../chamberlain_code.h"
#include "../clemsa.h"
#include "../dooya.h"
#include "../faac_slh.h"
#include "../gate_tx.h"
#include "../hormann.h"
#include "../keeloq.h"
#include "../linear.h"
#include "../linear_delta3.h"
#include "../megacode.h"
#include "../nice_flo.h"
#include "../nice_flor_s.h"
#include "../princeton.h"
#include "../somfy_keytis.h"
#include "../somfy_telis.h"

#define GDR_AM_PROTOCOL(symbol) 
#include "gdr_am_protocols_list.inc"
#undef GDR_AM_PROTOCOL

static const SubGhzProtocol* const gdr_protocol_registry_am_items[] = {
#define GDR_AM_PROTOCOL(symbol) &symbol,
#include "gdr_am_protocols_list.inc"
#undef GDR_AM_PROTOCOL
};

static const SubGhzProtocolRegistry gdr_protocol_registry_am = {
    .items = gdr_protocol_registry_am_items,
    .size = sizeof(gdr_protocol_registry_am_items) /
            sizeof(gdr_protocol_registry_am_items[0]),
};

static const GDRProtocolPlugin gdr_am_plugin = {
    .plugin_name = "Garage Door Remote AM Registry",
    .filter = GDRProtocolRegistryFilterAM,
    .registry = &gdr_protocol_registry_am,
};

static const FlipperAppPluginDescriptor gdr_am_plugin_descriptor = {
    .appid = GDR_PROTOCOL_PLUGIN_APP_ID,
    .ep_api_version = GDR_PROTOCOL_PLUGIN_API_VERSION,
    .entry_point = &gdr_am_plugin,
};

const FlipperAppPluginDescriptor* gdr_am_plugin_ep(void) {
    return &gdr_am_plugin_descriptor;
}
