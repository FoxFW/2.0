/**
 * @file subghz_scene_protocol_list.c
 *
 * Protocol List with live ON/OFF toggles, organized into manufacturer/
 * family groups. Each group has its own master toggle (sets every member
 * at once) as well as individual per-protocol toggles nested underneath.
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
 * Layout:
 *  Row 0   "Protocols"     value: "62" or "48/62"  (count header, live update)
 *  Row 1   "Select"        value: "All" / "None"    (OK to apply)
 *  Row 2…  [Group name]    value: "ON" / "OFF" / "MIX"  (master toggle)
 *          "  [Protocol]"  value: "ON" / "OFF"          (indented, nested)
 *          "RAW" and "BinRAW" locked ON, non-interactive, ungrouped.
 */

#include "../subghz_i.h"
#include "../subghz_protocol_filter.h"
#include "../subghz_protocol_groups.h"
#include <lib/subghz/subghz_protocol_registry.h>
#include <string.h>

#define PROTO_LIST_MAX   256u
#define SELECT_ROW_INDEX 1u

static const char* const proto_toggle_labels[] = {"OFF", "ON"};
static const char* const group_toggle_labels[] = {"OFF", "ON"};
static const char* const select_labels[]       = {"All", "None"};


typedef struct {
    SubGhz* subghz;
    size_t  protocol_index;
    size_t  group;
    bool    enabled;   /* updated in callback; used by apply_items_to_filter */
    VariableItem* item;
    /* Row label ("  <protocol name>"), OWNED by this ctx. Required because
     * variable_item_list_add() stores the const char* it's given by
     * reference (see VariableItemList's own struct: `const char* label;`,
     * never copied) - it does NOT duplicate the string the way
     * variable_item_set_current_value_text() does internally via
     * furi_string_set(). A label built in a function-local stack buffer
     * (as this used to be, one `char label[32]` declared inside the
     * per-protocol loop below) becomes a dangling pointer the instant
     * protocol_list_populate() returns and its stack frame is torn down;
     * every row's `label` pointer then ends up reading whatever the same
     * reused stack slot last held - in practice, uniformly, the last
     * protocol name written into it. That's the confirmed root cause of a
     * real-hardware bug where every per-protocol row read the same single
     * protocol's name (whichever one happened to populate last) instead of
     * its own. g_proto_ctx (this struct's array) is heap-allocated and
     * lives for as long as the scene may still redraw, so a buffer stored
     * here - unlike the old stack buffer - stays valid for the pointer's
     * entire real lifetime. */
    char label[32];
} ProtoItemCtx;

static ProtoItemCtx*   g_proto_ctx       = NULL;
static size_t          g_proto_ctx_count = 0;
static uint8_t         g_select_choice   = 0;
static VariableItem*   g_count_item      = NULL;  /* safe: array pre-allocated */
static VariableItem*   g_group_items[SUBGHZ_PROTOCOL_GROUP_COUNT];
static bool             g_group_present[SUBGHZ_PROTOCOL_GROUP_COUNT];
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

/* Recompute a group header's ON/OFF/MIX display from its members' current
 * in-memory state (doesn't touch the underlying filter - that's handled by
 * apply_items_to_filter on a dirty flag, same as before). */
static void refresh_group_header(size_t group) {
    if(group >= SUBGHZ_PROTOCOL_GROUP_COUNT || !g_group_items[group]) return;

    size_t enabled_count = 0, member_count = 0;
    for(size_t i = 0; i < g_proto_ctx_count; i++) {
        if(g_proto_ctx[i].group != group) continue;
        member_count++;
        if(g_proto_ctx[i].enabled) enabled_count++;
    }

    if(member_count == 0) return;
    if(enabled_count == member_count) {
        variable_item_set_current_value_index(g_group_items[group], 1);
        variable_item_set_current_value_text(g_group_items[group], "ON");
    } else if(enabled_count == 0) {
        variable_item_set_current_value_index(g_group_items[group], 0);
        variable_item_set_current_value_text(g_group_items[group], "OFF");
    } else {
        variable_item_set_current_value_index(g_group_items[group], 1);
        variable_item_set_current_value_text(g_group_items[group], "MIX");
    }
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

    refresh_group_header(g_proto_ctx[ci].group);
}


/* Encoded as (group_index + 1) | 0x8000 to distinguish from per-protocol
 * ctx (which uses (ci + 1) with the top bit clear - proto count never
 * gets remotely close to 0x8000). */
#define GROUP_CTX_FLAG 0x8000u

static void group_toggle_cb(VariableItem* item) {
    uintptr_t ctx_raw = (uintptr_t)variable_item_get_context(item);
    if(!(ctx_raw & GROUP_CTX_FLAG)) return;
    size_t group = (ctx_raw & ~(uintptr_t)GROUP_CTX_FLAG) - 1;
    if(group >= SUBGHZ_PROTOCOL_GROUP_COUNT) return;

    uint8_t val = variable_item_get_current_value_index(item);
    bool enable = (val != 0);

    for(size_t i = 0; i < g_proto_ctx_count; i++) {
        if(g_proto_ctx[i].group != group) continue;
        g_proto_ctx[i].enabled = enable;
        if(g_proto_ctx[i].item) {
            variable_item_set_current_value_index(g_proto_ctx[i].item, enable ? 1 : 0);
            variable_item_set_current_value_text(g_proto_ctx[i].item, proto_toggle_labels[enable ? 1 : 0]);
        }
    }
    variable_item_set_current_value_text(item, group_toggle_labels[enable ? 1 : 0]);

    g_filter_dirty = true;
    g_count_dirty  = true;
}


static void select_value_change_cb(VariableItem* item) {
    g_select_choice = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, select_labels[g_select_choice]);
}

/* RAW and BinRAW are both generic capture modes, not named protocols - they
 * get their own always-on ungrouped rows instead of being sorted into a
 * brand group. Without this, BinRAW was the sole member of the "General"
 * catch-all group, which just showed up as an odd single-protocol group at
 * the bottom of the list. */
static bool is_raw_family(const char* name) {
    return strcmp(name, "RAW") == 0 || strcmp(name, "BinRAW") == 0;
}


static void protocol_list_populate(SubGhz* subghz) {
    VariableItemList* list = subghz->variable_item_list;
    variable_item_list_reset(list);

    if(g_proto_ctx) { free(g_proto_ctx); g_proto_ctx = NULL; }
    g_proto_ctx_count = 0;
    memset(g_group_items, 0, sizeof(g_group_items));
    memset(g_group_present, 0, sizeof(g_group_present));

    size_t total = subghz_protocol_registry_count(&subghz_protocol_registry);
    size_t alloc = (total < PROTO_LIST_MAX ? total : PROTO_LIST_MAX);
    g_proto_ctx = malloc(sizeof(ProtoItemCtx) * alloc);
    furi_assert(g_proto_ctx);

    /* Which group each registry protocol belongs to, and whether any
     * non-RAW member exists in that group (so we skip empty groups). */
    size_t group_of[PROTO_LIST_MAX];
    for(size_t i = 0; i < total && i < PROTO_LIST_MAX; i++) {
        const SubGhzProtocol* proto = subghz_protocol_registry_get_by_index(&subghz_protocol_registry, i);
        if(!proto) continue;
        if(is_raw_family(proto->name)) continue; /* RAW/BinRAW aren't grouped */
        group_of[i] = subghz_protocol_group_for_name(proto->name);
        g_group_present[group_of[i]] = true;
    }

    size_t group_row_count = 0;
    for(size_t g = 0; g < SUBGHZ_PROTOCOL_GROUP_COUNT; g++) {
        if(g_group_present[g]) group_row_count++;
    }

    /* Pre-allocate: 1 count + 1 select + 1 RAW row (if present) + group
     * header rows + protocol rows. Prevents ANY realloc - all _add()
     * pointers stay valid for the lifetime of this populate. */
    variable_item_list_reserve(list, total + group_row_count + 2);

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

    /* RAW/BinRAW rows, if present - forced ON, non-interactive, ungrouped.
     * "RAW" is listed first, "BinRAW" second, matching registry order. */
    for(size_t i = 0; i < total && i < PROTO_LIST_MAX; i++) {
        const SubGhzProtocol* proto = subghz_protocol_registry_get_by_index(&subghz_protocol_registry, i);
        if(proto && is_raw_family(proto->name)) {
            subghz_protocol_filter_set_enabled(subghz->protocol_filter, i, true);
            VariableItem* raw = variable_item_list_add(list, proto->name, 1, NULL, NULL);
            variable_item_set_current_value_index(raw, 0);
            variable_item_set_current_value_text(raw, "ON");
        }
    }

    /* One block per group, in display order: header row + its members. */
    for(size_t g = 0; g < SUBGHZ_PROTOCOL_GROUP_COUNT; g++) {
        if(!g_group_present[g]) continue;

        VariableItem* header = variable_item_list_add(
            list, subghz_protocol_group_names[g], 2, group_toggle_cb,
            (void*)(uintptr_t)((g + 1) | GROUP_CTX_FLAG));
        g_group_items[g] = header;
        variable_item_set_current_value_index(header, 1);

        for(size_t i = 0; i < total && i < PROTO_LIST_MAX; i++) {
            const SubGhzProtocol* proto = subghz_protocol_registry_get_by_index(&subghz_protocol_registry, i);
            if(!proto || is_raw_family(proto->name)) continue;
            if(group_of[i] != g) continue;

            bool enabled = subghz_protocol_filter_is_enabled(subghz->protocol_filter, i);
            size_t ci    = g_proto_ctx_count++;
            g_proto_ctx[ci].subghz         = subghz;
            g_proto_ctx[ci].protocol_index = i;
            g_proto_ctx[ci].group          = g;
            g_proto_ctx[ci].enabled        = enabled;

            /* Written directly into the ctx's own persistent buffer, not a
             * local stack variable - see the ProtoItemCtx::label comment
             * above for why (variable_item_list_add() keeps this pointer
             * by reference, never copies it). */
            snprintf(
                g_proto_ctx[ci].label, sizeof(g_proto_ctx[ci].label), "  %s", proto->name);
            VariableItem* it = variable_item_list_add(
                list, g_proto_ctx[ci].label, 2, proto_toggle_cb, (void*)(uintptr_t)(ci + 1));
            variable_item_set_current_value_index(it, enabled ? 1 : 0);
            variable_item_set_current_value_text(it, proto_toggle_labels[enabled ? 1 : 0]);
            g_proto_ctx[ci].item = it;
        }

        refresh_group_header(g);
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
    memset(g_group_items, 0, sizeof(g_group_items));
    g_filter_dirty    = false;
    g_count_dirty     = false;
    g_proto_subghz    = NULL;
}
