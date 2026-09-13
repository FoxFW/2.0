#include "restart_confirm_view.h"

#include <furi_hal_power.h>

static App* s_restart_confirm_app = NULL;

/* Shown when the user closes the FoxLAB app with a Device Name change
 * saved but not yet applied (App::device_name_restart_pending - see app.h
 * and foxr_companion.c's foxr_handle_settings_system_set()). Changing the
 * name no longer reboots the instant it's saved, since that would also
 * power-cycle the attached ESP32 and drop the FoxLAB WiFi connection the
 * user is actively using - so this screen is the deferred, opt-in version
 * of that reboot, offered right when it stops being disruptive (the app is
 * closing anyway).
 *
 * "< Later" just exits like Back normally would - the name is already
 * durably saved, it just takes effect whenever the Flipper next reboots
 * for any other reason. "Restart >" (or OK, when focused) applies it right
 * now via a full reboot, same as the Reboot card's own "Reboot" button.
 * Two-soft-key bottom bar, matching launcher_view.c's established "Fox
 * terminal" look for this app - Left/Right move which side is focused,
 * OK activates whichever side that is, same focus-then-confirm model
 * message_view.c's two-button screens use. This used to have Left and
 * Right/OK each fire directly with no focus step; fixed per the
 * 2026-09-13 footer-button audit (FOOTER_BUTTON_AUDIT.md project doc),
 * which flagged this exact screen as "Pattern A." */

#define RESTART_CONFIRM_BUTTON_H 14
#define RESTART_CONFIRM_BUTTON_PAD_X 4
#define RESTART_CONFIRM_BUTTON_MARGIN 2
#define RESTART_CONFIRM_BUTTON_R 3

/* Was one full-width inverted bar with both labels drawn on top of it - on
 * real hardware that reads as a single joined control rather than two
 * buttons. Now each label gets its own sized-to-fit rounded box (same
 * canvas_draw_rbox()/canvas_draw_rframe() pill look already used for
 * TPMS's box-list rows), with a real gap of background between them at the
 * bottom corners - visually distinct, and only the focused side is filled. */
static void restart_confirm_draw_button(
    Canvas* canvas, bool align_left, bool focused, const char* text) {
    canvas_set_font(canvas, FontSecondary);
    uint16_t text_w = canvas_string_width(canvas, text);
    uint16_t box_w = text_w + RESTART_CONFIRM_BUTTON_PAD_X * 2;
    uint16_t box_h = RESTART_CONFIRM_BUTTON_H;
    int32_t box_y = 64 - RESTART_CONFIRM_BUTTON_MARGIN - box_h;
    int32_t box_x = align_left ? RESTART_CONFIRM_BUTTON_MARGIN :
                                  (128 - RESTART_CONFIRM_BUTTON_MARGIN - (int32_t)box_w);

    canvas_set_color(canvas, ColorBlack);
    if(focused) {
        canvas_draw_rbox(canvas, box_x, box_y, box_w, box_h, RESTART_CONFIRM_BUTTON_R);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, box_x + box_w / 2, box_y + box_h / 2, AlignCenter, AlignCenter, text);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, box_x, box_y, box_w, box_h, RESTART_CONFIRM_BUTTON_R);
        canvas_draw_str_aligned(
            canvas, box_x + box_w / 2, box_y + box_h / 2, AlignCenter, AlignCenter, text);
    }
}

static void restart_confirm_draw_bottom_bar(Canvas* canvas, bool focus_left) {
    restart_confirm_draw_button(canvas, true, focus_left, "< Later");
    restart_confirm_draw_button(canvas, false, !focus_left, "Restart >");
}

static void restart_confirm_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_restart_confirm_app;
    if(app == NULL) return;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Shifted up from the original 9/21/36/46 layout - the bottom line at
    // 46 sat inside the button row's vertical span (buttons occupy
    // 64-2-14=48 up to 62), overlapping it on real hardware. Buttons still
    // start at y=48; the last line now centers at 40, clearing that by 4px.
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignCenter, "Restart now to apply");
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Device Name Update?");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignCenter, "Later applies it next");
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "time you boot instead.");

    restart_confirm_draw_bottom_bar(canvas, app->restart_confirm_focus_left);
}

static bool restart_confirm_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyBack:
        /* "Later", same as an unfocused Back always means in this app -
         * the name is already saved, this just exits without forcing a
         * reboot right now. Independent of which side is focused, matching
         * every other Back handler in this app. */
        app->device_name_restart_pending = false;
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    case InputKeyLeft:
        if(!app->restart_confirm_focus_left) {
            app->restart_confirm_focus_left = true;
            with_view_model(app->restart_confirm_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyRight:
        if(app->restart_confirm_focus_left) {
            app->restart_confirm_focus_left = false;
            with_view_model(app->restart_confirm_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyOk:
        if(app->restart_confirm_focus_left) {
            /* "Later" - same action as Back above. */
            app->device_name_restart_pending = false;
            view_dispatcher_stop(app->view_dispatcher);
        } else {
            app->device_name_restart_pending = false;
            furi_hal_power_reset(); /* never returns */
        }
        return true;
    default:
        return false;
    }
}

View* restart_confirm_view_alloc(App* app) {
    s_restart_confirm_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, restart_confirm_draw_cb);
    view_set_input_callback(view, restart_confirm_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void restart_confirm_view_free(View* view) {
    s_restart_confirm_app = NULL;
    view_free(view);
}
