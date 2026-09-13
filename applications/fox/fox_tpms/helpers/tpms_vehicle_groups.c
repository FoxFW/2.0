#include "tpms_vehicle_groups.h"
#include <furi.h>

/* Real-world grounding for the frequency/modulation choices below: each of
 * these 5 groups maps 1:1 onto one of this app's 5 registered decoders
 * (protocols/protocol_items.c), and every number here traces back to that
 * decoder's own header comment (ported directly from rtl_433's device
 * driver, itself written against real captures) - not guesswork:
 *
 *   - schrader_gg4.c:   "Frequency: 433.92 MHz +-38 kHz, Modulation: ASK /
 *     OOK (AM650)... Schrader 3013/3015 MRX-GG4 (Kia Sportage,
 *     Mercedes A0009054100, ...)"
 *   - tpms_ford.c:      "FSK, Manchester encoded... Seen on Ford Fiesta/
 *     Focus/Kuga/Transit (Continental S180084730Z). 315 MHz (US) /
 *     433.92 MHz (EU)."
 *   - tpms_renault.c:   "FSK 433 MHz... Seen on Renault Clio/Captur/Zoe and
 *     Dacia Sandero."
 *   - tpms_citroen.c:   "FSK 433 MHz... Also Peugeot and likely Fiat,
 *     Mitsubishi, VDO-types."
 *   - tpms_pmv107j.c:   "FSK 315 MHz... Pacific PMV-107J sensors used by
 *     Toyota."
 *
 * None of those comments give an exact FSK deviation, so FM238/FM476 (the
 * app's existing 2.38kHz/4.76kHz-deviation presets - see tpms_set_preset())
 * are an experimental best guess for the 4 FSK groups, tried in that order
 * by the hopper (tpms_hopper_update(), tpms_app_i.c) before giving up on a
 * frequency. Ford is the only group with two *frequencies* on record (US
 * vs EU markets); the others only ever get a modulation-only second
 * candidate at their one confirmed frequency. Schrader/Kia/Mercedes tries
 * AM270 (narrower CC1101 RX bandwidth, same OOK modulation) as its second
 * candidate rather than a second frequency, since only 433.92 MHz is
 * documented for it. See COMMIT_LOG.md for the fuller writeup - this is a
 * starting point for real-world testing, not a verified-accurate table
 * (the Mode Picker row that leads here dropped its own "(Experimental)"
 * label per the user's own call, but the caveat still applies here). */

static const TPMSVehicleRfCandidate k_ford_candidates[] = {
    {315000000, "FM238"},
    {433920000, "FM238"},
};

static const TPMSVehicleRfCandidate k_renault_candidates[] = {
    {433920000, "FM238"},
    {433920000, "FM476"},
};

static const TPMSVehicleRfCandidate k_citroen_candidates[] = {
    {433920000, "FM238"},
    {433920000, "FM476"},
};

static const TPMSVehicleRfCandidate k_toyota_candidates[] = {
    {315000000, "FM238"},
    {315000000, "FM476"},
};

static const TPMSVehicleRfCandidate k_schrader_candidates[] = {
    {433920000, "AM650"},
    {433920000, "AM270"},
};

/* Every group uses this same single step: LF activation is a near-field
 * magnetic trigger at the wheel, not a sequence tied to which wheel comes
 * first - our research turned up no evidence any of these 5 vehicle groups
 * needs the wheels visited in a particular order, so there's nothing to
 * number. tpms_scene_vehicle_steps.c still walks a `steps[]` array of any
 * length, so a future group that genuinely does need an ordered multi-step
 * procedure (or a different LF gesture) only needs a new steps array here,
 * not a scene rewrite. */
static const TPMSVehicleStep k_generic_steps[] = {
    // \ec at the start of each line is widget_add_text_scroll_element()'s
    // own per-line center-alignment control code (widget_element_text_
    // scroll.c: '\e' + 'c' -> AlignCenter for that line) - it must be
    // repeated on every explicit \n-separated line, since the element
    // resets alignment back to left on each new line unless told
    // otherwise; it's NOT needed on Relearn's body (tpms_scene_relearn.c),
    // which is one auto-wrapping paragraph with no \n's of its own, so a
    // single leading \ec there covers every wrapped line too.
    {"Approach Vehicle",
     "\ecGo to ANY wheel and hold\n"
     "\ecthe Flippers back flat\n"
     "\ecagainst base of valve stem\n"
     "\ecFlipper will pulse a 125kHz\n"
     "\ecsignal to TPMS to activate\n"
     "\ecand begin scanning data."},
};

const TPMSVehicleGroup tpms_vehicle_groups[TPMS_VEHICLE_GROUP_COUNT] = {
    {
        .make_name = "Ford",
        .subtitle = "Fiesta, Focus, Kuga, Transit",
        .protocol_name = "Ford TPMS",
        .candidates = k_ford_candidates,
        .candidate_count = COUNT_OF(k_ford_candidates),
        .steps = k_generic_steps,
        .step_count = COUNT_OF(k_generic_steps),
    },
    {
        .make_name = "Renault",
        .subtitle = "Clio, Captur, Zoe, Dacia Sandero",
        .protocol_name = "Renault TPMS",
        .candidates = k_renault_candidates,
        .candidate_count = COUNT_OF(k_renault_candidates),
        .steps = k_generic_steps,
        .step_count = COUNT_OF(k_generic_steps),
    },
    {
        .make_name = "Citroen / Peugeot",
        .subtitle = "Shared VDO-type sensor",
        .protocol_name = "Citroen TPMS",
        .candidates = k_citroen_candidates,
        .candidate_count = COUNT_OF(k_citroen_candidates),
        .steps = k_generic_steps,
        .step_count = COUNT_OF(k_generic_steps),
    },
    {
        .make_name = "Toyota / Lexus",
        .subtitle = "PMV-107J sensor",
        .protocol_name = "Toyota PMV-107J",
        .candidates = k_toyota_candidates,
        .candidate_count = COUNT_OF(k_toyota_candidates),
        .steps = k_generic_steps,
        .step_count = COUNT_OF(k_generic_steps),
    },
    {
        .make_name = "Kia / Mercedes / Other",
        .subtitle = "Generic Schrader GG4 sensor",
        .protocol_name = "Schrader GG4",
        .candidates = k_schrader_candidates,
        .candidate_count = COUNT_OF(k_schrader_candidates),
        .steps = k_generic_steps,
        .step_count = COUNT_OF(k_generic_steps),
    },
};
