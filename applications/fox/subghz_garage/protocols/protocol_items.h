#pragma once
#include <lib/subghz/registry.h>

#include "princeton.h"
#include "keeloq.h"
#include "nice_flo.h"
#include "came.h"
#include "faac_slh.h"
#include "nice_flor_s.h"
#include "came_twee.h"
#include "came_atomo.h"
#include "hormann.h"
#include "somfy_telis.h"
#include "somfy_keytis.h"
#include "gate_tx.h"
#include <lib/subghz/protocols/raw.h> /* subghz_txrx.h already includes this absolute path; using the same path here (instead of a local duplicate) avoids -Werror=redundant-decls */
#include "linear.h"
#include "linear_delta3.h"
#include "secplus_v2.h"
#include "secplus_v1.h"
#include "megacode.h"
#include "holtek.h"
#include "chamberlain_code.h"
#include "marantec.h"
#include "doitrand.h"
#include "telcoma_edge.h"
#include "phoenix_v2.h"
#include "clemsa.h"
#include "ansonic.h"
#include "smc5326.h"
#include "holtek_ht12x.h"
#include "dooya.h"
#include "alutech_at_4n.h"
#include "kinggates_stylo_4k.h"
#include "bin_raw.h"
#include "mastercode.h"
#include "dickert_mahs.h"
#include "gangqi.h"
#include "marantec24.h"
#include "hay21.h"
#include "revers_rb2.h"
#include "roger.h"
#include "beninca_arc.h"
#include "keyfinder.h"
#include "x10.h"
#include "jarolift.h"

/* Dormant protocols: source present, not wired into any group's registry or
 * application.fam sources= list, so fbt never compiles them. Mirrors ARF's
 * own disabled-by-default UNLEASHED_PORT pattern - land the source now,
 * activate later once a group has spare capacity. */
#include "nord_ice.h"
#include "allstar_firefly.h"
#include "ditec_gol4.h"

/* The combined registry that used to live here has been split into 8
 * per-group registries (see protocol_groups.h) so the protocol plugin can
 * be loaded a group at a time instead of all 42 protocols at once. */
