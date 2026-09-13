#pragma once
#include <lib/subghz/registry.h>

/* SubBrute's own private copy of exactly the 8 SubGhz protocols its
 * bruteforce attacks need (CAME, Nice FLO, Chamberlain, Linear, Ansonic,
 * SMC5326, Holtek HT12X, Princeton).
 *
 * Why this exists (task #76): SubBrute is an external .fap
 * (apptype=FlipperAppType.MENUEXTERNAL - see application.fam) loaded at
 * runtime by the app loader, not linked into core firmware. An external
 * app can only call into core through symbols targets/f7/api_symbols.csv
 * explicitly exports, and none of the individual protocol structs below
 * are exported there (only subghz_protocol_raw by itself and the whole
 * core subghz_protocol_registry struct are). Meanwhile core's own registry
 * (lib/subghz/protocols/protocol_items.c) deliberately omits CAME, Nice
 * FLO, Chamberlain, Linear, Ansonic, and Holtek entirely now that
 * FoxFW2.0's external Garage/Gate/Other app (applications/fox/subghz_garage)
 * owns their auto-detect duty, to avoid double-compiling them. SubBrute
 * used to point straight at that shrunk core registry, so 7 of its 9 brand
 * groups (everything except PT2260/PT2262) silently couldn't resolve a
 * transmitter/decoder by name any more - see subbrute_device.c and
 * helpers/subbrute_worker.c for the two places this used to crash or fail.
 *
 * The fix mirrors the exact pattern applications/fox/subghz_garage already
 * uses for the same problem: vendor a private copy of just the protocol
 * logic this app needs (this protocols/ folder), copied from Garage's own
 * already-proven copies - not from lib/subghz/protocols/'s originals, whose
 * byte sizes differ, confirming Garage's copies are adapted, not verbatim -
 * and give this app its own SubGhzProtocolRegistry pointing at that private
 * copy instead of core's. Princeton (PT2260/PT2262) is included too even
 * though it still resolves via core's registry today, so replacing the
 * environment's registry pointer entirely doesn't regress it.
 *
 * Maintenance tradeoff (explicitly accepted, not an oversight): this is a
 * third, independently-maintained copy of these 8 protocols' encode/decode
 * logic, alongside both core's copy (lib/subghz/protocols/) and Garage's
 * own copy (applications/fox/subghz_garage/protocols/). Any future upstream
 * fix to one of these 8 protocols has to be applied here too if SubBrute's
 * bruteforce attacks should pick it up. */

#include "came.h"
#include "nice_flo.h"
#include "chamberlain_code.h"
#include "linear.h"
#include "ansonic.h"
#include "smc5326.h"
#include "holtek_ht12x.h"
#include "princeton.h"

extern const SubGhzProtocolRegistry subbrute_subghz_protocol_registry;
