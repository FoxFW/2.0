#include "launcher_view.h"

#include <string.h>

/* The FoxLAB Launcher screen - shown once the ESP32 is detected. A single
 * button flips the ESP32's FoxLAB WiFi portal on/off (see Fox_ESP32_FW's
 * fox_lab.cpp, "[LAB/START]"/"[LAB/STOP]"). The portal keeps running on
 * the ESP32 after this app is closed - this screen is just the switch. */

static App* s_launcher_view_app = NULL;

#define LAUNCHER_BUTTON_H 14
#define LAUNCHER_BUTTON_PAD_X 4
#define LAUNCHER_BUTTON_MARGIN 2
#define LAUNCHER_BUTTON_R 3

/* Two separate soft-key buttons (Left/Right), not the old single centered
 * OK pill - gives this screen a second destination (the Terminal/"FoxLAB
 * Active" screen - flpr_view.c) it didn't have room for as a single
 * centered button. Left/Right only move which side is focused (filled vs
 * outlined, same as message_view.c's message_draw_two_buttons()), and OK
 * activates whichever side is currently focused - see launcher_input_cb()
 * below. This used to have Left/Right each fire a different action
 * directly with no focus step at all; fixed per the 2026-09-13 footer-
 * button audit (FOOTER_BUTTON_AUDIT.md project doc), which flagged this
 * exact screen as "Pattern A." Each button is its own sized-to-fit rounded
 * box (canvas_draw_rbox()/canvas_draw_rframe(), the same pill look TPMS's
 * box-list rows and restart_confirm_view.c's own buttons use) rather than
 * one continuous inverted bar with both labels drawn on top - that used to
 * read as a single joined control. */
static void launcher_draw_button(Canvas* canvas, bool align_left, bool focused, const char* text) {
    canvas_set_font(canvas, FontSecondary);
    uint16_t text_w = canvas_string_width(canvas, text);
    uint16_t box_w = text_w + LAUNCHER_BUTTON_PAD_X * 2;
    uint16_t box_h = LAUNCHER_BUTTON_H;
    int32_t box_y = 64 - LAUNCHER_BUTTON_MARGIN - box_h;
    int32_t box_x = align_left ? LAUNCHER_BUTTON_MARGIN :
                                  (128 - LAUNCHER_BUTTON_MARGIN - (int32_t)box_w);

    canvas_set_color(canvas, ColorBlack);
    if(focused) {
        canvas_draw_rbox(canvas, box_x, box_y, box_w, box_h, LAUNCHER_BUTTON_R);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, box_x + box_w / 2, box_y + box_h / 2, AlignCenter, AlignCenter, text);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, box_x, box_y, box_w, box_h, LAUNCHER_BUTTON_R);
        canvas_draw_str_aligned(
            canvas, box_x + box_w / 2, box_y + box_h / 2, AlignCenter, AlignCenter, text);
    }
}

static void launcher_draw_bottom_bar(Canvas* canvas, bool focus_left, const char* left_label) {
    launcher_draw_button(canvas, true, focus_left, left_label);
    launcher_draw_button(canvas, false, !focus_left, "Terminal >");
}

/* "then visit: " (regular weight) + the IP (bold) drawn as one centered
 * unit - canvas_draw_str_aligned() can't mix fonts within a single call,
 * so this measures each half with canvas_string_width() and centers the
 * pair manually instead of hardcoding an x that would drift if either
 * string ever changes. foxlab.local dropped per the user's request - the
 * IP alone is now this line's whole job. */
static void launcher_draw_ip_line(Canvas* canvas, uint8_t y) {
    const char* prefix = "then visit: ";
    const char* ip = "192.168.4.1";

    canvas_set_font(canvas, FontSecondary);
    uint16_t prefix_w = canvas_string_width(canvas, prefix);
    canvas_set_font(canvas, FontPrimary);
    uint16_t ip_w = canvas_string_width(canvas, ip);

    int32_t x = (128 - (int32_t)(prefix_w + ip_w)) / 2;

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, x, y, AlignLeft, AlignCenter, prefix);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, x + (int32_t)prefix_w, y, AlignLeft, AlignCenter, ip);
}

static void launcher_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_launcher_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "FoxLAB Launcher");

    // Pushed down from the original y=13/22/33 layout, which ran close
    // enough to the header's own ~y=2-13 span to visibly touch it on real
    // hardware - and dropping the old "or foxlab.local" line (see
    // launcher_draw_ip_line() above) freed up enough room to space these
    // three lines out properly instead of cramming four into the same band.
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 19, AlignCenter, AlignCenter, "Connect PC to WiFi:");
    canvas_draw_str_aligned(canvas, 64, 29, AlignCenter, AlignCenter, "FoxLAB  (Pass: 88888888)");

    launcher_draw_ip_line(canvas, 40);

    canvas_set_font(canvas, FontSecondary);
    const char* left_label = app->lab_busy ? "..." : (app->lab_active ? "< Stop" : "< Start");
    launcher_draw_bottom_bar(canvas, app->launcher_focus_left, left_label);
}

static void launcher_toggle(App* app) {
    if(app->esp_at == NULL || app->lab_busy) return;

    app->lab_busy = true;
    with_view_model(app->launcher_view, uint8_t * _m, { UNUSED(_m); }, true);

    bool starting = !app->lab_active;
    const char* cmd = starting ? "[LAB/START]" : "[LAB/STOP]";
    /* Match on the reply's TAG prefix first (shared by both the SUCCESS
     * and ERROR replies), not the full SUCCESS string - see
     * app_wait_for_reply_prefix()'s comment in main.c. Starting brings up
     * a softAP plus three servers, so it gets more headroom than
     * stopping does. */
    const char* reply_prefix = starting ? "[LAB/START/" : "[LAB/STOP/";
    const char* expect_success = starting ? "[LAB/START/SUCCESS]" : "[LAB/STOP/SUCCESS]";
    uint32_t timeout_ms = starting ? 6000 : 3000;

    esp_at_send(app->esp_at, cmd);

    EspAtMsg msg;
    bool success = app_wait_for_reply_prefix(app, reply_prefix, timeout_ms, &msg) &&
                   strcmp(msg.line, expect_success) == 0;
    if(success) {
        app->lab_active = starting;
    }

    app->lab_busy = false;
    with_view_model(app->launcher_view, uint8_t * _m, { UNUSED(_m); }, true);

    /* On a successful Start (not Stop), hand off to the "FoxLAB Active"
     * companion screen - see flpr_view.h. The FLPR companion itself
     * answers commands regardless of which view is showing; this just
     * gives the user the "keep this app open" signal they asked for. */
    if(success && starting) {
        app_show_flpr(app);
    }
}

static bool launcher_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyLeft:
        if(!app->launcher_focus_left) {
            app->launcher_focus_left = true;
            with_view_model(app->launcher_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyRight:
        if(app->launcher_focus_left) {
            app->launcher_focus_left = false;
            with_view_model(app->launcher_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyOk:
        /* Whichever side is focused - Start/Stop, or the Terminal screen
         * (still reachable via Back from there too, since Right is now
         * only a focus-move, not a direct jump - real-hardware gap this
         * button originally closed, reported 2026-09-11, is unaffected:
         * Back from the Terminal screen still returns here). */
        if(app->launcher_focus_left) {
            launcher_toggle(app);
        } else {
            app_show_flpr(app);
        }
        return true;
    case InputKeyBack:
    default:
        return false;
    }
}

View* launcher_view_alloc(App* app) {
    s_launcher_view_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, launcher_draw_cb);
    view_set_input_callback(view, launcher_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void launcher_view_free(View* view) {
    s_launcher_view_app = NULL;
    view_free(view);
}
