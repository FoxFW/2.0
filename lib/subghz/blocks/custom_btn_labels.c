/* custom_btn_labels.c — External button label table for SubGHz transmitter view.
 *
 * Place in: lib/subghz/blocks/
 *
 * Add ONE declaration to lib/subghz/blocks/custom_btn.h:
 *
 *   const char* subghz_custom_btn_get_label_for_proto(
 *       const char* proto_name, uint8_t btn_dir);
 *
 * That is the ONLY file outside this one that needs changing.
 * No protocol files are modified — new ARF protocol updates can be
 * dropped in without any merging.
 *
 * To add a new protocol: add one line to label_table[] below.
 *
 * btn_dir values:  SUBGHZ_CUSTOM_BTN_UP    = 1
 *                  SUBGHZ_CUSTOM_BTN_DOWN  = 2
 *                  SUBGHZ_CUSTOM_BTN_LEFT  = 3
 *                  SUBGHZ_CUSTOM_BTN_RIGHT = 4
 */

#include "custom_btn_i.h"
#include <string.h>

/* NULL = direction not used by this protocol (button hidden in view) */
typedef struct {
    const char* proto;   /* exact string from .sub file "Protocol:" field */
    const char* up;
    const char* down;
    const char* left;
    const char* right;
} SubGhzBtnLabelEntry;

static const SubGhzBtnLabelEntry label_table[] = {
    /* ── Kia / Hyundai ──────────────────────────────────────────────── */
    { "KIA/HYU V0",   "Lock",  "Unlock", "Trunk",  "Panic"  },
    { "KIA/HYU V1",   "Lock",  "Unlock", "Trunk",  "Panic"  },
    { "KIA/HYU V2",   "Lock",  "Unlock", "Trunk",  "Panic"  },
    { "KIA/HYU V3/V4","Lock",  "Unlock", "Trunk",  "Panic"  },
    { "KIA/HYU V5",   "Lock",  "Unlock", "Trunk",  "Horn"   }, /* btn_map verified */
    { "KIA/HYU V6",   "Lock",  "Unlock", "Trunk",  "Panic"  },
    { "Kia V7",       "Lock",  "Unlock", "Trunk",  "Panic"  },

    /* ── VAG Group (VW/Audi/Seat/Skoda/Porsche) ─────────────────────── */
    { "VAG GROUP",    "Lock",  "Unlock", "Trunk",  "Panic"  },
    { "Porsche AG",   "Lock",  "Unlock", "Trunk",  "Panic"  },

    /* ── PSA Group (Peugeot/Citroën/DS) ─────────────────────────────── */
    { "PSA GROUP",    "Lock",  "Unlock", "Trunk",  "Trunk"  }, /* LEFT=RIGHT=Trunk */
    { "PSA OLD",      "Lock",  "Unlock", "Trunk",  "Trunk"  },

    /* ── Ford ────────────────────────────────────────────────────────── */
    { "FORD V0",      "Lock",  "Unlock", "Trunk",  NULL     }, /* max=3 */

    /* ── Fiat ────────────────────────────────────────────────────────── */
    { "MARELLI",      "Lock",  "Unlock", "Trunk",  NULL     }, /* Fiat Marelli */
    { "FIAT SPA",     "Lock",  "Unlock", "Trunk",  "Panic"  },

    /* ── Chrysler/Dodge/Jeep ─────────────────────────────────────────── */
    { "Chrysler",     "Lock",  "Unlock", NULL,     NULL     }, /* max=2 */

    /* ── Japanese ────────────────────────────────────────────────────── */
    { "SUBARU",       "Lock",  "Unlock", "Trunk",  "Panic"  },
    { "SUZUKI",       "Panic", "Trunk",  "Lock",   "Unlock" }, /* inverted! */
    { "Mazda V0",     "Lock",  "Unlock", "Trunk",  "Panic"  },
    { "MazdaSiemens", "Lock",  "Unlock", "Trunk",  "Panic"  },
    { "Mitsubishi V0","Lock",  "Unlock", NULL,     NULL     },

    /* ── Russian alarm systems ───────────────────────────────────────── */
    { "Scher-Khan",   "Lock",  "Unlock", "Trunk",  "Start"  },
    { "Star Line",    "Lock",  "Unlock", "Trunk",  "Start"  },
    { "Sheriff CFM",  "Lock",  "Unlock", "Trunk",  "Panic"  },

    /* ── Gate / roller-shutter protocols (channel-based, no lock labels) */
    /* Add entries here as needed once button meanings are confirmed      */

    /* Sentinel */
    { NULL, NULL, NULL, NULL, NULL }
};

const char* subghz_custom_btn_get_label_for_proto(
    const char* proto_name, uint8_t btn_dir) {

    if(!proto_name || btn_dir == 0 || btn_dir > 4) return NULL;

    for(const SubGhzBtnLabelEntry* e = label_table; e->proto; e++) {
        if(strcmp(e->proto, proto_name) == 0) {
            switch(btn_dir) {
            case 1: return e->up;
            case 2: return e->down;
            case 3: return e->left;
            case 4: return e->right;
            }
        }
    }
    return NULL; /* protocol not in table → caller shows arrows */
}
