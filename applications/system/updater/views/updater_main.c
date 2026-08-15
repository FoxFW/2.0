#include <gui/gui_i.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <gui/canvas.h>
#include <assets_icons.h>
#include <furi.h>
#include <input/input.h>
#include <string.h>
#include <toolbox/version.h>

#include "../updater_i.h"
#include "updater_main.h"

struct UpdaterMainView {
    View* view;
    ViewDispatcher* view_dispatcher;
    FuriPubSubSubscription* subscription;
    void* context;
};

static const uint8_t PROGRESS_RENDER_STEP = 1; /* percent, to limit rendering rate */

typedef struct {
    FuriString* status;
    uint8_t progress, rendered_progress;
    bool failed;
} UpdaterProgressModel;

void updater_main_model_set_state(
    UpdaterMainView* main_view,
    const char* message,
    uint8_t progress,
    bool failed) {
    bool update = false;
    with_view_model(
        main_view->view,
        UpdaterProgressModel * model,
        {
            model->failed = failed;
            model->progress = progress;
            if(furi_string_cmp_str(model->status, message)) {
                furi_string_set(model->status, message);
                model->rendered_progress = progress;
                update = true;
            } else if(
                (model->rendered_progress > progress) ||
                ((progress - model->rendered_progress) > PROGRESS_RENDER_STEP)) {
                model->rendered_progress = progress;
                update = true;
            }
        },
        update);
}

View* updater_main_get_view(UpdaterMainView* main_view) {
    furi_assert(main_view);
    return main_view->view;
}

bool updater_main_input(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    UpdaterMainView* main_view = context;
    if(!main_view->view_dispatcher) {
        return true;
    }

    if((event->type == InputTypeShort) && (event->key == InputKeyOk)) {
        view_dispatcher_send_custom_event(
            main_view->view_dispatcher, UpdaterCustomEventRetryUpdate);
    } else if((event->type == InputTypeLong) && (event->key == InputKeyBack)) {
        view_dispatcher_send_custom_event(
            main_view->view_dispatcher, UpdaterCustomEventCancelUpdate);
    }

    return true;
}

// Splits the version line into "FoxFW " (bold) and "(v2.0.4)" (not bold) so
// the caller can render each in a different font - the full line doesn't
// fit on screen at all-bold width. Built from firmware.ver via
// toolbox/version.h, so this never needs manual edits on release -
// version_get_firmware_origin() is already correctly-cased ("FoxFW"),
// version_get_version() returns the dist-suffixed tag (e.g. "foxfw-v2.0.4");
// strip the known "foxfw-" prefix to isolate the bare version.
static void updater_build_version_line(
    char* origin_out,
    size_t origin_out_size,
    char* tag_out,
    size_t tag_out_size) {
    const Version* ver = version_get();
    const char* origin = version_get_firmware_origin(ver);
    const char* tag = version_get_version(ver);

    const char* prefix = "foxfw-";
    size_t prefix_len = strlen(prefix);
    if(strncmp(tag, prefix, prefix_len) == 0) {
        tag += prefix_len;
    }

    // The RAM-resident updater stage (FURI_RAM_EXEC, active while flash is
    // actually being written) appends " (RAM)" to the version string - strip
    // it here so this screen doesn't briefly overflow off-screen with
    // "(v2.0.4 (RAM))" before settling back to the normal "(v2.0.4)".
    char tag_buf[24];
    strlcpy(tag_buf, tag, sizeof(tag_buf));
    const char* ram_suffix = " (RAM)";
    size_t tag_len = strlen(tag_buf);
    size_t ram_suffix_len = strlen(ram_suffix);
    if(tag_len >= ram_suffix_len &&
       strcmp(tag_buf + tag_len - ram_suffix_len, ram_suffix) == 0) {
        tag_buf[tag_len - ram_suffix_len] = '\0';
    }

    snprintf(origin_out, origin_out_size, "%s ", origin);

    // strlcat instead of snprintf("(%s)", ...) - tag_buf is a stack buffer,
    // not a literal, so GCC can't statically prove the snprintf fits and
    // flags it under -Werror=format-truncation.
    tag_out[0] = '\0';
    strlcat(tag_out, "(", tag_out_size);
    strlcat(tag_out, tag_buf, tag_out_size);
    strlcat(tag_out, ")", tag_out_size);
}

static void updater_main_draw_callback(Canvas* canvas, void* _model) {
    UpdaterProgressModel* model = _model;

    canvas_set_font(canvas, FontPrimary);

    if(model->failed) {
        canvas_draw_icon(canvas, 2, 22, &I_Warning_30x23);
        canvas_draw_str_aligned(canvas, 40, 9, AlignLeft, AlignTop, "Update Failed!");
        canvas_set_font(canvas, FontSecondary);

        elements_multiline_text_aligned(
            canvas, 75, 26, AlignCenter, AlignTop, furi_string_get_cstr(model->status));

        canvas_draw_str_aligned(
            canvas, 18, 55, AlignLeft, AlignTop, "to retry, hold       to abort");
        canvas_draw_icon(canvas, 7, 54, &I_Ok_btn_9x9);
        canvas_draw_icon(canvas, 75, 55, &I_Pin_back_arrow_10x8);
    } else {
        canvas_draw_str_aligned(canvas, 55, 6, AlignLeft, AlignTop, "UPDATING");

        char origin_part[16] = {0};
        char tag_part[24] = {0};
        updater_build_version_line(origin_part, sizeof(origin_part), tag_part, sizeof(tag_part));
        // I_Updating_32x40 is drawn at x=4 with a width of 32, so its right
        // border is at x=36 - start the version line 9px past that (x=45)
        // to give it the extra room "FoxFW (v2.0.4)" needs at FontPrimary
        // widths.
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 45, 20, AlignLeft, AlignTop, origin_part);
        uint16_t origin_width = canvas_string_width(canvas, origin_part);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 45 + origin_width, 20, AlignLeft, AlignTop, tag_part);
        canvas_draw_str_aligned(
            canvas, 64, 51, AlignCenter, AlignTop, furi_string_get_cstr(model->status));
        canvas_draw_icon(canvas, 4, 5, &I_Updating_32x40);
        elements_progress_bar(canvas, 42, 36, 80, (float)model->progress / 100);
    }
}

UpdaterMainView* updater_main_alloc(void) {
    UpdaterMainView* main_view = malloc(sizeof(UpdaterMainView));

    main_view->view = view_alloc();
    view_allocate_model(main_view->view, ViewModelTypeLocking, sizeof(UpdaterProgressModel));

    with_view_model(
        main_view->view,
        UpdaterProgressModel * model,
        {
            model->status = furi_string_alloc_set("Waiting for SD card");
            model->progress = 0;
            model->rendered_progress = 0;
            model->failed = false;
        },
        true);

    view_set_context(main_view->view, main_view);
    view_set_input_callback(main_view->view, updater_main_input);
    view_set_draw_callback(main_view->view, updater_main_draw_callback);

    return main_view;
}

void updater_main_free(UpdaterMainView* main_view) {
    furi_assert(main_view);
    with_view_model(
        main_view->view, UpdaterProgressModel * model, { furi_string_free(model->status); }, false);
    view_free(main_view->view);
    free(main_view);
}

void updater_main_set_storage_pubsub(UpdaterMainView* main_view, FuriPubSubSubscription* sub) {
    main_view->subscription = sub;
}

FuriPubSubSubscription* updater_main_get_storage_pubsub(UpdaterMainView* main_view) {
    return main_view->subscription;
}

void updater_main_set_view_dispatcher(UpdaterMainView* main_view, ViewDispatcher* view_dispatcher) {
    main_view->view_dispatcher = view_dispatcher;
}
