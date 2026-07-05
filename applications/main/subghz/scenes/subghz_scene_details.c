/* subghz_scene_details.c
 *
 * Shows the full decoded technical key string (Key, Yek, Sn, Cnt, Btn,
 * Seed, CRC, etc.) for a saved decoded signal in a scrollable Widget view.
 *
 * The protocol name is shown in FontPrimary at the top.
 * The remaining fields fill a text-scroll area below.
 * Pressing Back returns to the Saved Menu with "Details" still selected.
 *
 * Registration: add this line to subghz_scene_config.h:
 *   ADD_SCENE(subghz, details, Details)
 */

#include "../subghz_i.h"
#include <string.h>

void subghz_scene_details_on_enter(void* context) {
    SubGhz* subghz = context;
    widget_reset(subghz->widget);

    const char* full = furi_string_get_cstr(subghz->error_str);

    /* ── Split first line (protocol name) from the rest ── */
    char proto[48] = "Details";
    const char* body = full;
    const char* nl = strchr(full, '\n');
    if(nl) {
        size_t len = (size_t)(nl - full);
        if(len >= sizeof(proto)) len = sizeof(proto) - 1;
        memcpy(proto, full, len);
        proto[len] = '\0';
        body = nl + 1;
    }

    /* ── Protocol name header ── */
    widget_add_string_element(
        subghz->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, proto);

    /* ── Scrollable body (everything after the first line) ── */
    widget_add_text_scroll_element(
        subghz->widget, 0, 14, 128, 50, body);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdWidget);
}

bool subghz_scene_details_on_event(void* context, SceneManagerEvent event) {
    /* Let the scene manager handle Back automatically — it will pop this
     * scene and return to SavedMenu with the previous selection intact. */
    UNUSED(context);
    UNUSED(event);
    return false;
}

void subghz_scene_details_on_exit(void* context) {
    SubGhz* subghz = context;
    widget_reset(subghz->widget);
}
