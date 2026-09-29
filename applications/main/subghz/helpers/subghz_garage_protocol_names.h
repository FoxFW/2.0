#pragma once

#include <lib/subghz/protocols/alutech_at_4n.h>
#include <lib/subghz/protocols/somfy_telis.h>
#include <lib/subghz/protocols/jarolift.h>
#include <lib/subghz/protocols/nice_flo.h>
#include <lib/subghz/protocols/came_twee.h>
#include <lib/subghz/protocols/secplus_v1.h>
#include <lib/subghz/protocols/smc5326.h>
#include <lib/subghz/protocols/dickert_mahs.h>
#include <lib/subghz/protocols/roger.h>
#include <lib/subghz/protocols/keyfinder.h>
#include <lib/subghz/protocols/secplus_v2.h>
#include <lib/subghz/protocols/gangqi.h>
#include <lib/subghz/protocols/hay21.h>
#include <lib/subghz/protocols/dooya.h>
#include <lib/subghz/protocols/holtek.h>
#include <lib/subghz/protocols/came.h>
#include <lib/subghz/protocols/nice_flor_s.h>
#include <lib/subghz/protocols/beninca_arc.h>
#include <lib/subghz/protocols/marantec.h>
#include <lib/subghz/protocols/holtek_ht12x.h>
#include <lib/subghz/protocols/revers_rb2.h>
#include <lib/subghz/protocols/megacode.h>
#include <lib/subghz/protocols/marantec24.h>
#include <lib/subghz/protocols/faac_slh.h>
#include <lib/subghz/protocols/came_atomo.h>
#include <lib/subghz/protocols/chamberlain_code.h>
#include <lib/subghz/protocols/mastercode.h>
#include <lib/subghz/protocols/linear.h>
#include <lib/subghz/protocols/linear_delta3.h>
#include <lib/subghz/protocols/gate_tx.h>
#include <lib/subghz/protocols/kinggates_stylo_4k.h>
#include <lib/subghz/protocols/somfy_keytis.h>
#include <lib/subghz/protocols/phoenix_v2.h>
#include <lib/subghz/protocols/clemsa.h>
#include <lib/subghz/protocols/ansonic.h>
#include <lib/subghz/protocols/doitrand.h>
#include <lib/subghz/protocols/hormann.h>
#include <lib/subghz/protocols/x10.h>
#include <lib/subghz/protocols/telcoma_edge.h>

#include <stddef.h>
#include <stdbool.h>

#define SUBGHZ_GARAGE_PROTOCOL_GROUP_COUNT 11

extern const char* const subghz_garage_protocol_group_names[SUBGHZ_GARAGE_PROTOCOL_GROUP_COUNT];
extern const char* const subghz_garage_protocol_group_members[SUBGHZ_GARAGE_PROTOCOL_GROUP_COUNT];

extern const char* const subghz_garage_protocol_names[];
extern const size_t subghz_garage_protocol_names_count;

bool subghz_garage_protocol_name_is_garage(const char* protocol_name);
