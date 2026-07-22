/**
 * @file subghz_scene_protocol_list.c
 *
 * Protocol List with live ON/OFF toggles.
 *
 * Root-cause fix for all previous crashes:
 * ─────────────────────────────────────────
 * variable_item_list_add() returns a pointer INTO the list's internal
 * VariableItemArray (M*LIB ARRAY_DEF, stores structs by value).  When the
 * array runs out of capacity it reallocs — moving every item — so every
 * pointer previously returned by _add() becomes dangling.
 *
 * Fix: variable_item_list_reserve(list, total_items) is called BEFORE the
 * first _add().  This pre-allocates enough capacity for all items in one
 * shot, so no realloc ever occurs and all returned pointers stay valid for
 * the lifetime of the list.
 *
 * The function is implemented in variable_item_list.c (our modified copy).
 *
 * Layout:
 *  Row 0   "Protocols"  value: "62" or "48/62"  (count header, live update)
 *  Row 1   "Select"     value: "All" / "None"    (OK to apply)
 *  Row 2…  [Protocol]   value: "ON" / "OFF"
 *          "RAW" locked ON, non-interactive.
 */

#include "../subghz_i.h"
#include "../subghz_protocol_filter.h"
#include <lib/subghz/subghz_protocol_registry.h>
#include <string.h>

/* Declared in variable_item_list.c (our modified copy) */
void variable_item_list_reserve(VariableItemList* list, size_t count);

#define PROTO_LIST_MAX   256u
#define SELECT_ROW_INDEX 1u

static const char* const proto_toggle_labels[] = {"OFF", "ON"};
static const char* const select_labels[]       = {"All", "None"};


typedef struct {
    SubGhz* subghz;
    size_t  protocol_index;
    bool    enabled;   /* updated in callback; used by apply_items_to_filter */
} ProtoItemCtx;

static ProtoItemCtx* g_proto_ctx       = NULL;
static size_t        g_proto_ctx_count = 0;
static uint8_t       g_select_choice   = 0;
static VariableItem* g_count_item      = NULL;  /* safe: array pre-allocated */
static bool          g_filter_dirty    = false;
static bool          g_count_dirty     = false;
static SubGhz*       g_proto_subghz    = NULL;


static void build_count_text(SubGhz* subghz, size_t total, char* buf, size_t n) {
    size_t active = subghz_protocol_filter_enabled_count(subghz->protocol_filter, total);
    if(active == total)
        snprintf(buf, n, "%zu", total);
    else
        snprintf(buf, n, "%zu/%zu", active, total);
}

static void apply_items_to_filter(void) {
    for(size_t i = 0; i < g_proto_ctx_count; i++) {
        if(!g_proto_ctx[i].subghz || !g_proto_ctx[i].subghz->protocol_filter) continue;
        subghz_protocol_filter_set_enabled(
            g_proto_ctx[i].subghz->protocol_filter,
            g_proto_ctx[i].protocol_index,
            g_proto_ctx[i].enabled);
    }
    g_filter_dirty = false;
}


static void proto_toggle_cb(VariableItem* item) {
    uintptr_t ctx_raw = (uintptr_t)variable_item_get_context(item);
    if(!ctx_raw) return;
    size_t ci = ctx_raw - 1;
    if(!g_proto_ctx || ci >= g_proto_ctx_count) return;

    uint8_t val = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, proto_toggle_labels[val]);
    g_proto_ctx[ci].enabled = (val != 0);
    g_filter_dirty = true;
    g_count_dirty  = true;
}


static void select_value_change_cb(VariableItem* item) {
    g_select_choice = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, select_labels[g_select_choice]);
}


static void protocol_list_populate(SubGhz* subghz) {
    VariableItemList* list = subghz->variable_item_list;
    variable_item_list_reset(list);

    if(g_proto_ctx) { free(g_proto_ctx); g_proto_ctx = NULL; }
    g_proto_ctx_count = 0;

    size_t total = subghz_protocol_registry_count(&subghz_protocol_registry);
    size_t alloc = (total < PROTO_LIST_MAX ? total : PROTO_LIST_MAX);
    g_proto_ctx = malloc(sizeof(ProtoItemCtx) * alloc);
    furi_assert(g_proto_ctx);

    /* Pre-allocate: 1 count + 1 select + total protocol rows.
     * Prevents ANY realloc — all _add() pointers stay valid. */
    variable_item_list_reserve(list, total + 2);

    /* Row 0: count header — pointer valid because array is pre-allocated */
    char count_buf[16];
    build_count_text(subghz, total, count_buf, sizeof(count_buf));
    g_count_item = variable_item_list_add(list, "Protocols", 1, NULL, NULL);
    variable_item_set_current_value_index(g_count_item, 0);
    variable_item_set_current_value_text(g_count_item, count_buf);
    g_count_dirty  = false;
    g_filter_dirty = false;

    /* Row 1: Select All / None */
    VariableItem* sel = variable_item_list_add(list, "Select", 2, select_value_change_cb, NULL);
    variable_item_set_current_value_index(sel, g_select_choice);
    variable_item_set_current_value_text(sel, select_labels[g_select_choice]);

    /* Protocol rows */
    for(size_t i = 0; i < total && i < PROTO_LIST_MAX; i++) {
        const SubGhzProtocol* proto =
            subghz_protocol_registry_get_by_index(&subghz_protocol_registry, i);
        if(!proto) continue;

        if(strcmp(proto->name, "RAW") == 0) {
            subghz_protocol_filter_set_enabled(subghz->protocol_filter, i, true);
            VariableItem* raw = variable_item_list_add(list, proto->name, 1, NULL, NULL);
            variable_item_set_current_value_index(raw, 0);
            variable_item_set_current_value_text(raw, "ON");
            continue;
        }

        bool enabled = subghz_protocol_filter_is_enabled(subghz->protocol_filter, i);
        size_t ci    = g_proto_ctx_count++;
        g_proto_ctx[ci].subghz         = subghz;
        g_proto_ctx[ci].protocol_index = i;
        g_proto_ctx[ci].enabled        = enabled;

        VariableItem* it = variable_item_list_add(
            list, proto->name, 2, proto_toggle_cb, (void*)(uintptr_t)(ci + 1));
        variable_item_set_current_value_index(it, enabled ? 1 : 0);
        variable_item_set_current_value_text(it, proto_toggle_labels[enabled ? 1 : 0]);
    }
}


static void protocol_list_enter_cb(void* context, uint32_t index) {
    SubGhz* subghz = context;
    if(index != SELECT_ROW_INDEX) return;

    bool enable_all = (g_select_choice == 0);
    size_t total    = subghz_protocol_registry_count(&subghz_protocol_registry);
    for(size_t i = 0; i < total; i++)
        subghz_protocol_filter_set_enabled(subghz->protocol_filter, i, enable_all);

    protocol_list_populate(subghz);
    variable_item_list_set_selected_item(subghz->variable_item_list, SELECT_ROW_INDEX);
}


void subghz_scene_protocol_list_on_enter(void* context) {
    SubGhz* subghz = context;
    g_proto_subghz  = subghz;
    g_select_choice = 0;

    protocol_list_populate(subghz);
    variable_item_list_set_enter_callback(
        subghz->variable_item_list, protocol_list_enter_cb, subghz);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdVariableItemList);
}

bool subghz_scene_protocol_list_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    bool consumed   = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            subghz->scene_manager, SubGhzSceneProtocolList, event.event);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        if(g_filter_dirty) apply_items_to_filter();
        subghz_protocol_filter_save(subghz->protocol_filter);
        consumed = false;
    } else if(event.type == SceneManagerEventTypeTick) {
        if(g_filter_dirty) apply_items_to_filter();
        if(g_count_dirty && g_count_item && g_proto_subghz) {
            size_t total = subghz_protocol_registry_count(&subghz_protocol_registry);
            char count_buf[16];
            build_count_text(g_proto_subghz, total, count_buf, sizeof(count_buf));
            variable_item_set_current_value_text(g_count_item, count_buf);
            g_count_dirty = false;
        }
        consumed = true;
    }
    return consumed;
}

void subghz_scene_protocol_list_on_exit(void* context) {
    SubGhz* subghz = context;
    if(g_filter_dirty) apply_items_to_filter();
    subghz_protocol_filter_save(subghz->protocol_filter);
    variable_item_list_reset(subghz->variable_item_list);
    if(g_proto_ctx) { free(g_proto_ctx); g_proto_ctx = NULL; }
    g_proto_ctx_count = 0;
    g_count_item      = NULL;
    g_filter_dirty    = false;
    g_count_dirty     = false;
    g_proto_subghz    = NULL;
}