#include "../subratt_i.h"
#include "subratt_scene.h"
#include <storage/storage.h>

#define TAG "SubRattSceneKeylogStopConfirm"

static void subratt_scene_keylog_stop_confirm_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    SubRattState* instance = context;
    if(type != InputTypeShort) {
        return;
    }
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, SubRattCustomEventTypeKeylogDiscard);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(
            instance->view_dispatcher, SubRattCustomEventTypeKeylogSave);
    }
}

void subratt_scene_keylog_stop_confirm_on_enter(void* context) {
    furi_assert(context);
    SubRattState* instance = context;
    Widget* widget = instance->widget;

    uint32_t keys_tested = subratt_worker_get_keylog_count(instance->worker);

    FuriString* text = furi_string_alloc();
    furi_string_printf(
        text,
        "%lu key%s tested.\nSave these keys?",
        (unsigned long)keys_tested,
        keys_tested == 1 ? "" : "s");
    widget_add_string_multiline_element(
        widget, 5, 6, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(text));
    furi_string_free(text);

    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        "Discard",
        subratt_scene_keylog_stop_confirm_widget_callback,
        instance);
    widget_add_button_element(
        widget,
        GuiButtonTypeRight,
        "Save",
        subratt_scene_keylog_stop_confirm_widget_callback,
        instance);

    instance->current_view = SubRattViewWidget;
    view_dispatcher_switch_to_view(instance->view_dispatcher, instance->current_view);
}

static void subratt_scene_keylog_stop_confirm_discard(SubRattState* instance) {
    UNUSED(instance);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_remove(storage, SUBRATT_KEYLOG_SESSION_PATH);
    furi_record_close(RECORD_STORAGE);
}

bool subratt_scene_keylog_stop_confirm_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    SubRattState* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubRattCustomEventTypeKeylogDiscard) {
            subratt_scene_keylog_stop_confirm_discard(instance);
            scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, SubRattSceneSetupAttack);
            consumed = true;
        } else if(event.event == SubRattCustomEventTypeKeylogSave) {
            scene_manager_next_scene(instance->scene_manager, SubRattSceneKeylogSaveName);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {

        subratt_scene_keylog_stop_confirm_discard(instance);
        scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, SubRattSceneSetupAttack);
        consumed = true;
    }

    return consumed;
}

void subratt_scene_keylog_stop_confirm_on_exit(void* context) {
    furi_assert(context);
    SubRattState* instance = context;
    widget_reset(instance->widget);
}
