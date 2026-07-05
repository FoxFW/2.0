/**
 * @file subghz_scene_modulation_list.c
 *
 * Modulation List — per-modulation ON/OFF toggles that persist to SD card.
 * The list is built DYNAMICALLY from the live SubGhzSetting preset list, so
 * any presets added to setting_user.file are automatically included.
 *
 * Header row shows "Modulations: x/y" (enabled/total).
 */

#include "../subghz_i.h"
#include <string.h>
#include "../subghz_modulation_filter.h"
#include <gui/modules/variable_item_list.h>
#include <lib/subghz/subghz_setting.h>

static const char* const mod_toggle_labels[] = {"OFF", "ON"};

/* File-scope SubGhz pointer — set in on_enter, cleared in on_exit.
 * Safe: all VariableItemList callbacks run in the SubGHz app thread. */
static SubGhz*       g_mod_subghz      = NULL;
static VariableItem* g_mod_count_item  = NULL;
static bool          g_mod_count_dirty = false;

/* ── Count header ──────────────────────────────────────────────────────── */

static void build_mod_count_text(char* buf, size_t buf_size) {
    SubGhzSetting* setting = subghz_txrx_get_setting(g_mod_subghz->txrx);
    size_t total   = subghz_setting_get_preset_count(setting);
    size_t enabled = 0;
    for(size_t i = 0; i < total; i++) {
        if(subghz_modulation_filter_is_enabled(g_mod_subghz->modulation_filter, i))
            enabled++;
    }
    snprintf(buf, buf_size, "%u/%u", (unsigned)enabled, (unsigned)total);
}

/* ── Per-modulation toggle callback (Left/Right) ───────────────────────── */

static void mod_toggle_cb(VariableItem* item) {
    /* Context is the modulation index stored as uintptr_t — no aliasing. */
    uintptr_t idx = (uintptr_t)variable_item_get_context(item);
    if(!g_mod_subghz) return;
    if(!g_mod_subghz->modulation_filter) return;

    SubGhzSetting* setting = subghz_txrx_get_setting(g_mod_subghz->txrx);
    if(idx >= subghz_setting_get_preset_count(setting)) return;

    uint8_t val = variable_item_get_current_value_index(item);
    /* Prevent disabling the last enabled modulation */
    if(val == 0 && g_mod_subghz && g_mod_subghz->modulation_filter) {
        SubGhzSetting* s2 = subghz_txrx_get_setting(g_mod_subghz->txrx);
        size_t tot2 = subghz_setting_get_preset_count(s2);
        size_t en_count = 0;
        for(size_t j = 0; j < tot2; j++)
            if(subghz_modulation_filter_is_enabled(g_mod_subghz->modulation_filter, j))
                en_count++;
        if(en_count <= 1 && subghz_modulation_filter_is_enabled(g_mod_subghz->modulation_filter, idx)) {
            variable_item_set_current_value_index(item, 1);
            variable_item_set_current_value_text(item, mod_toggle_labels[1]);
            return;
        }
    }
    variable_item_set_current_value_text(item, mod_toggle_labels[val]);
    subghz_modulation_filter_set_enabled(g_mod_subghz->modulation_filter, idx, (bool)val);
    g_mod_count_dirty = true;
}

/* ── Build / rebuild the full list ────────────────────────────────────── */

static const char* const mod_select_labels[] = {"All", "None"};
static uint8_t        g_mod_select_choice = 0;
#define MOD_SELECT_ROW 1u

static VariableItem* g_mod_preset_items[SUBGHZ_MOD_FILTER_COUNT];
static size_t        g_mod_preset_item_count = 0;

static void mod_select_value_cb(VariableItem* item) {
    g_mod_select_choice = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, mod_select_labels[g_mod_select_choice]);
}

/* Enter callback: OK on Select row → apply All/None in-place */
static void mod_list_enter_cb(void* context, uint32_t index) {
    SubGhz* subghz = context;
    if(index != MOD_SELECT_ROW || !subghz || !subghz->modulation_filter) return;

    bool enable_all = (g_mod_select_choice == 0);
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    size_t total = subghz_setting_get_preset_count(setting);
    for(size_t i = 0; i < total && i < g_mod_preset_item_count; i++) {
        subghz_modulation_filter_set_enabled(subghz->modulation_filter, i, enable_all);
        if(g_mod_preset_items[i]) {
            uint8_t v = enable_all ? 1u : 0u;
            variable_item_set_current_value_index(g_mod_preset_items[i], v);
            variable_item_set_current_value_text(g_mod_preset_items[i], mod_toggle_labels[v]);
        }
    }
    g_mod_count_dirty = true;
}

static void mod_list_populate(SubGhz* subghz) {
    VariableItemList* list    = subghz->variable_item_list;
    SubGhzSetting*   setting = subghz_txrx_get_setting(subghz->txrx);
    size_t           total   = subghz_setting_get_preset_count(setting);

    variable_item_list_reset(list);
    g_mod_preset_item_count = 0;
    memset(g_mod_preset_items, 0, sizeof(g_mod_preset_items));

    /* Row 0: "Modulations x/y" header */
    char count_buf[16];
    build_mod_count_text(count_buf, sizeof(count_buf));
    g_mod_count_item = variable_item_list_add(list, "Modulations", 1, NULL, NULL);
    variable_item_set_current_value_index(g_mod_count_item, 0);
    variable_item_set_current_value_text(g_mod_count_item, count_buf);
    g_mod_count_dirty = false;

    /* Row 1: Select All / None */
    VariableItem* sel = variable_item_list_add(list, "Select", 2, mod_select_value_cb, NULL);
    variable_item_set_current_value_index(sel, g_mod_select_choice);
    variable_item_set_current_value_text(sel, mod_select_labels[g_mod_select_choice]);

    /* One row per preset — dynamically sourced from SubGhzSetting */
    for(size_t i = 0; i < total && i < SUBGHZ_MOD_FILTER_COUNT; i++) {
        const char* name    = subghz_setting_get_preset_name(setting, i);
        bool        enabled = subghz_modulation_filter_is_enabled(subghz->modulation_filter, i);

        VariableItem* item = variable_item_list_add(
            list,
            name ? name : "?",
            2,
            mod_toggle_cb,
            (void*)(uintptr_t)i);
        variable_item_set_current_value_index(item, enabled ? 1 : 0);
        variable_item_set_current_value_text(item, mod_toggle_labels[enabled ? 1 : 0]);
        if(g_mod_preset_item_count < SUBGHZ_MOD_FILTER_COUNT)
            g_mod_preset_items[g_mod_preset_item_count++] = item;
    }
}

/* ── Scene lifecycle ───────────────────────────────────────────────────── */

void subghz_scene_modulation_list_on_enter(void* context) {
    SubGhz* subghz = context;
    g_mod_subghz = subghz;
    g_mod_select_choice = 0;
    mod_list_populate(subghz);
    variable_item_list_set_enter_callback(subghz->variable_item_list, mod_list_enter_cb, subghz);
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdVariableItemList);
}

bool subghz_scene_modulation_list_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            subghz->scene_manager, SubGhzSceneModulationList, event.event);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        subghz_save_all(subghz);
        consumed = false;
    } else if(event.type == SceneManagerEventTypeTick) {
        if(g_mod_count_dirty && g_mod_count_item) {
            char count_buf[16];
            build_mod_count_text(count_buf, sizeof(count_buf));
            variable_item_set_current_value_text(g_mod_count_item, count_buf);
            g_mod_count_dirty = false;
        }
        consumed = true;
    }
    return consumed;
}

void subghz_scene_modulation_list_on_exit(void* context) {
    SubGhz* subghz = context;
    subghz_save_all(subghz);
    variable_item_list_reset(subghz->variable_item_list);
    g_mod_count_item  = NULL;
    g_mod_count_dirty = false;
    g_mod_subghz      = NULL;
}
