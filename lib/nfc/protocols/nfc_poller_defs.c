#include <furi/furi.h>
#include "nfc_poller_defs.h"
#include "../nfc_group_manager.h"

#include <nfc/protocols/iso14443_3a/iso14443_3a_poller_defs.h>
#include <nfc/protocols/iso14443_3b/iso14443_3b_poller_defs.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller_defs.h>
#include <nfc/protocols/iso14443_4b/iso14443_4b_poller_defs.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller_defs.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller_defs.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_defs.h>
#include <nfc/protocols/mf_plus/mf_plus_poller_defs.h>
#include <nfc/protocols/mf_desfire/mf_desfire_poller_defs.h>
#include <nfc/protocols/slix/slix_poller_defs.h>
#include <nfc/protocols/st25tb/st25tb_poller_defs.h>
#include <nfc/protocols/ntag4xx/ntag4xx_poller_defs.h>
#include <nfc/protocols/type_4_tag/type_4_tag_poller_defs.h>
#include <nfc/protocols/emv/emv_poller_defs.h>

const NfcPollerBase* const nfc_pollers_api[NfcProtocolNum] = {
    [NfcProtocolIso14443_3a] = &nfc_poller_iso14443_3a,
    [NfcProtocolIso14443_3b] = &nfc_poller_iso14443_3b,
    [NfcProtocolIso14443_4a] = &nfc_poller_iso14443_4a,
    [NfcProtocolIso14443_4b] = &nfc_poller_iso14443_4b,
    [NfcProtocolIso15693_3] = &nfc_poller_iso15693_3,
    [NfcProtocolFelica] = NULL, /* external: grp_nfc_felica.fal */
    [NfcProtocolMfUltralight] = &mf_ultralight_poller,
    [NfcProtocolMfClassic] = &mf_classic_poller,
    [NfcProtocolMfPlus] = NULL, /* external: grp_nfc_mf_plus.fal */
    [NfcProtocolMfDesfire] = NULL, /* external: grp_nfc_mf_desfire.fal */
    [NfcProtocolSlix] = NULL, /* external: grp_nfc_slix.fal */
    [NfcProtocolSt25tb] = NULL, /* external: grp_nfc_st25tb.fal */
    [NfcProtocolNtag4xx] = NULL, /* external: grp_nfc_ntag4xx.fal */
    [NfcProtocolType4Tag] = NULL, /* external: grp_nfc_type_4_tag.fal */
    [NfcProtocolEmv] = NULL, /* external: grp_nfc_emv.fal */

};

/* FoxFW NFC externalization: external protocols are served by an on-demand .fal
 * group (see nfc_group_manager); internal ones by the static table above.
 * nfc_group_is_external() is authoritative - a static entry can be a legitimate
 * NULL for an internal protocol, so NULL must NOT be read as "externalized". */
const NfcPollerBase* nfc_poller_get_base(NfcProtocol protocol) {
    furi_check(protocol < NfcProtocolNum);
    if(nfc_group_is_external(protocol)) {
        return nfc_group_get_poller(protocol);
    }
    return nfc_pollers_api[protocol];
}
