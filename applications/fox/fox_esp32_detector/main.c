#include <furi.h>
#include <furi_hal.h>
#include <expansion/expansion.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* Sized for AT response lines only - the longest expected is the AT+GMR
   version banner, well under 128 bytes. */
#define LINE_MAX 128

#define AT_TIMEOUT_MS       600
#define GMR_TIMEOUT_MS      1200
#define FINGERPRINT_TIMEOUT_MS 400
#define NMEA_LISTEN_MS      200
#define CAPTURE_MAX         160

/* scan_worker()'s local `banner` buffer specifically - LINE_MAX is too
   small for it: the Tier 2 "Match \"%.20s\": %.100s" format alone needs
   up to 131 bytes (10 literal + 20 + 100 + the null terminator), which
   a real build's -Wformat-truncation correctly flagged as a possible
   overflow of a 128-byte buffer. post_result()'s copy into
   App::result_banner (still LINE_MAX bytes) already truncates safely
   via strncpy, so only this local buffer needs to grow. */
#define BANNER_BUFFER_MAX 160

typedef enum {
    DetectorViewResult,
    DetectorViewProgress,
} DetectorView;

typedef enum {
    DetectorEventProgress,
} DetectorEvent;

typedef struct {
    FuriHalSerialId serial_id;
    const char* label;
} PinOption;

static const PinOption pin_options[] = {
    {FuriHalSerialIdUsart, "13/14 (USART)"},
    {FuriHalSerialIdLpuart, "15/16 (LPUART)"},
};
#define PIN_OPTION_COUNT (sizeof(pin_options) / sizeof(pin_options[0]))

static const uint32_t baud_options[] =
    {9600, 19200, 38400, 57600, 74880, 115200, 230400, 460800, 921600};
#define BAUD_OPTION_COUNT (sizeof(baud_options) / sizeof(baud_options[0]))

/* Raw substrings this app knows how to translate into a clean name,
   wherever they turn up - a tier-2 signature match, or buried in an
   otherwise-unidentified tier-3/GPS capture. This is meant to grow:
   every confirmed real-world response adds one line here rather than a
   special case elsewhere. Only Bruce is confirmed for real (see the
   comment on signatures[] below) - the commented-out entries show the
   intended shape for adding more once their actual output is known
   from real hardware, e.g. a Marauder banner containing a short build
   tag like "MRDR" would become {"MRDR", "Marauder"}. */
typedef struct {
    const char* raw_marker; /* case-insensitive substring to look for */
    const char* clean_name;
} KnownTag;

static const KnownTag known_tags[] = {
    {"bruce", "Bruce firmware"},
    {"fox esp32 firmware", "Fox ESP32 Firmware"},
    /* {"MRDR", "Marauder"}, */
    /* {"esp-idf", "ESP-IDF application (unidentified)"}, */
};
#define KNOWN_TAG_COUNT (sizeof(known_tags) / sizeof(known_tags[0]))

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Widget* widget;
    View* progress_view;

    FuriMutex* mutex;
    FuriThread* scan_thread;
    bool scanning;
    /* Set by navigation_callback() when Back is pressed mid-scan, read by
       scan_worker() (via Link::cancel) on its tightest blocking loop -
       without this, view_dispatcher_stop() still fires immediately, but
       app_free()'s furi_thread_join() then blocks until the in-flight
       scan finishes on its own (up to ~45s for the full pin/baud combo
       sweep), which is exactly what looked like a freeze on Back. */
    volatile bool cancel_requested;

    /* Written by scan_worker() under mutex, read by custom_event_callback()
       under mutex - the only two places that ever touch these. */
    size_t progress_index;
    size_t progress_total;
    char progress_status[64];

    /* Snapshot of the three fields above, copied out from under the mutex
       once per custom event by custom_event_callback() - see its comment
       for why progress_draw_cb() reads these instead of the raw fields
       directly. Touched only on the view dispatcher's own thread (the
       custom event callback and every draw callback both run there), so
       unlike progress_index/progress_total/progress_status above, these
       don't need the mutex at all. */
    size_t progress_display_index;
    size_t progress_display_total;
    char progress_display_status[64];

    bool scan_done;
    bool result_found;
    bool result_any_pin_claimed;
    char result_banner[LINE_MAX];
    size_t result_pin;
    uint32_t result_baud;
} App;

/* Bare-minimum receive path: an ISR pushes bytes into a stream buffer,
   and everything else runs synchronously on the scan worker thread as a
   sequence of blocking, timeout-bounded reads. */
typedef struct {
    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial;
    volatile bool* cancel;
} Link;

static void link_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    UNUSED(handle);
    Link* link = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(link->rx_stream, &byte, 1, 0);
    }
}

static bool link_read_line_until(Link* link, char* out, size_t out_capacity, uint32_t deadline_tick) {
    size_t len = 0;
    while(furi_get_tick() < deadline_tick) {
        if(link->cancel != NULL && *link->cancel) return false;
        uint8_t byte;
        size_t got = furi_stream_buffer_receive(link->rx_stream, &byte, 1, 50);
        if(got == 0) continue;

        if(byte == '\n') {
            if(len > 0 && out[len - 1] == '\r') len--;
            out[len] = '\0';
            if(len > 0) return true;
            len = 0;
            continue;
        }
        if(len < out_capacity - 1) out[len++] = (char)byte;
    }
    return false;
}

static bool link_expect_ok(Link* link, uint32_t timeout_ms) {
    char line[LINE_MAX];
    uint32_t deadline = furi_get_tick() + timeout_ms;
    while(link_read_line_until(link, line, sizeof(line), deadline)) {
        if(strcmp(line, "OK") == 0) return true;
        if(strcmp(line, "ERROR") == 0) return false;
    }
    return false;
}

static bool
    link_get_firmware_banner(Link* link, char* banner_out, size_t banner_capacity, uint32_t timeout_ms) {
    char line[LINE_MAX];
    uint32_t deadline = furi_get_tick() + timeout_ms;
    bool found = false;
    while(link_read_line_until(link, line, sizeof(line), deadline)) {
        if(strstr(line, "AT version") != NULL) {
            strncpy(banner_out, line, banner_capacity - 1);
            banner_out[banner_capacity - 1] = '\0';
            found = true;
        }
        if(strcmp(line, "OK") == 0 || strcmp(line, "ERROR") == 0) break;
    }
    return found;
}

static FuriHalBus link_bus_for_serial(FuriHalSerialId serial_id) {
    return serial_id == FuriHalSerialIdUsart ? FuriHalBusUSART1 : FuriHalBusLPUART1;
}

static bool contains_ci(const char* haystack, const char* needle) {
    size_t haystack_len = strlen(haystack);
    size_t needle_len = strlen(needle);
    if(needle_len == 0 || needle_len > haystack_len) return false;

    for(size_t i = 0; i + needle_len <= haystack_len; i++) {
        size_t j = 0;
        for(; j < needle_len; j++) {
            char a = haystack[i + j];
            char b = needle[j];
            if(a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if(b >= 'A' && b <= 'Z') b = (char)(b + 32);
            if(a != b) break;
        }
        if(j == needle_len) return true;
    }
    return false;
}

static const char* lookup_known_tag(const char* raw) {
    for(size_t i = 0; i < KNOWN_TAG_COUNT; i++) {
        if(contains_ci(raw, known_tags[i].raw_marker)) return known_tags[i].clean_name;
    }
    return NULL;
}

/* "info" is a real, documented Bruce serial command (confirmed against
   BruceDevices/firmware's own wiki), tried here purely to elicit some
   response - matching "bruce" in whatever comes back is a reasonable
   bet given how consistently these community firmwares self-identify,
   but the exact wording of that output was not independently confirmed
   the way the command's validity was. Marauder is deliberately not
   here: its documented commands are all WiFi-attack actions, none of
   which double as a safe "identify yourself". A Marauder module will
   still surface via the generic fallback tier and known_tags[] above,
   just not with a dedicated probe of its own yet.

   Fox ESP32 Firmware answers this same "info" probe with a plain
   "Fox ESP32 Firmware" line (see its own handleCommand()'s "info" case)
   specifically so it's identified here instead of falling through to
   the generic ECHO: fallback and showing up as an unhelpful
   "Unidentified: ECHO:?" - the exact real-world result this project's
   own firmware produced before that command existed. */
typedef struct {
    const char* probe;
    const char* match;
} FirmwareSignature;
static const FirmwareSignature signatures[] = {
    {"info", "bruce"},
    {"info", "fox esp32 firmware"},
};
#define SIGNATURE_COUNT (sizeof(signatures) / sizeof(signatures[0]))

/* Sends a probe command and accumulates whatever line(s) come back
   within timeout_ms into a single space-joined string. */
static bool
    link_capture_response(Link* link, const char* probe, char* out, size_t out_capacity, uint32_t timeout_ms) {
    char command[40];
    snprintf(command, sizeof(command), "%.35s\r\n", probe);

    uint8_t discard;
    while(furi_stream_buffer_receive(link->rx_stream, &discard, 1, 0) > 0) {
    }

    furi_hal_serial_tx(link->serial, (const uint8_t*)command, strlen(command));

    out[0] = '\0';
    size_t len = 0;
    char line[LINE_MAX];
    uint32_t deadline = furi_get_tick() + timeout_ms;
    while(link_read_line_until(link, line, sizeof(line), deadline)) {
        size_t line_len = strlen(line);
        size_t sep = (len > 0) ? 1 : 0;
        if(len + sep + line_len >= out_capacity) break;
        if(sep) out[len++] = ' ';
        memcpy(out + len, line, line_len);
        len += line_len;
        out[len] = '\0';
    }
    return len > 0;
}

/* Passive listen, no probe sent at all: a GPS module speaks NMEA
   sentences continuously and unprompted, always starting with '$', so
   the right way to detect one is to just listen briefly rather than
   send it a command it has no reason to understand. This runs before
   the AT/signature tiers precisely because it needs silence from us to
   work - sending "AT" first and only listening afterward could miss a
   sentence already in progress or catch a mid-sentence fragment. */
static bool link_listen_for_nmea(Link* link, char* out, size_t out_capacity, uint32_t timeout_ms) {
    char line[LINE_MAX];
    uint32_t deadline = furi_get_tick() + timeout_ms;
    if(link_read_line_until(link, line, sizeof(line), deadline) && line[0] == '$') {
        strncpy(out, line, out_capacity - 1);
        out[out_capacity - 1] = '\0';
        return true;
    }
    return false;
}

static void post_progress(App* app, const char* status, size_t index, size_t total) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strncpy(app->progress_status, status, sizeof(app->progress_status) - 1);
    app->progress_status[sizeof(app->progress_status) - 1] = '\0';
    app->progress_index = index;
    app->progress_total = total;
    furi_mutex_release(app->mutex);
    view_dispatcher_send_custom_event(app->view_dispatcher, DetectorEventProgress);
}

static void post_result(
    App* app,
    bool found,
    bool any_pin_claimed,
    const char* banner,
    size_t pin,
    uint32_t baud) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->result_found = found;
    app->result_any_pin_claimed = any_pin_claimed;
    strncpy(app->result_banner, banner, sizeof(app->result_banner) - 1);
    app->result_banner[sizeof(app->result_banner) - 1] = '\0';
    app->result_pin = pin;
    app->result_baud = baud;
    app->scan_done = true;
    furi_mutex_release(app->mutex);
    view_dispatcher_send_custom_event(app->view_dispatcher, DetectorEventProgress);
}

/* Runs entirely on its own thread (started by start_scan()) and never
   touches Widget/Gui directly - only shared App fields, guarded by
   app->mutex, followed by view_dispatcher_send_custom_event() to ask
   the main thread to redraw. An earlier version called widget_reset()
   and friends directly from inside the Scan button's own callback in a
   tight loop; the screen never visibly updated until the whole 18-combo
   scan finished, some 25-30 seconds later. Flipper's GUI does redraw
   asynchronously on its own thread, but Widget updates are only
   reliably picked up when driven through the ViewDispatcher's own
   event-processing cycle - i.e. from a context like this callback,
   which the dispatcher itself invokes, not from inside another
   button's callback still executing a blocking loop. */
static int32_t scan_worker(void* context) {
    App* app = context;
    size_t total_combos = PIN_OPTION_COUNT * BAUD_OPTION_COUNT;

    post_progress(app, "Starting...", 0, total_combos);

    char banner[BANNER_BUFFER_MAX];
    bool found = false;
    size_t found_pin = 0;
    uint32_t found_baud = 0;
    bool any_pin_claimed = false;

    for(size_t p = 0; p < PIN_OPTION_COUNT && !found && !app->cancel_requested; p++) {
        Expansion* expansion = furi_record_open(RECORD_EXPANSION);
        expansion_disable(expansion);

        FuriHalSerialHandle* handle = furi_hal_serial_control_acquire(pin_options[p].serial_id);
        if(handle == NULL) {
            expansion_enable(expansion);
            furi_record_close(RECORD_EXPANSION);
            char status[64];
            snprintf(status, sizeof(status), "%.20s: in use", pin_options[p].label);
            post_progress(app, status, p * BAUD_OPTION_COUNT, total_combos);
            continue;
        }
        any_pin_claimed = true;

        bool owned = !furi_hal_bus_is_enabled(link_bus_for_serial(pin_options[p].serial_id));
        if(owned) furi_hal_serial_init(handle, baud_options[0]);

        Link link;
        link.rx_stream = furi_stream_buffer_alloc(512, 1);
        link.serial = handle;
        link.cancel = &app->cancel_requested;
        furi_hal_serial_async_rx_start(handle, link_rx_callback, &link, false);

        for(size_t b = 0; b < BAUD_OPTION_COUNT && !found && !app->cancel_requested; b++) {
            size_t combo_index = p * BAUD_OPTION_COUNT + b;
            char status[64];
            snprintf(
                status,
                sizeof(status),
                "%.20s @ %lu",
                pin_options[p].label,
                (unsigned long)baud_options[b]);
            post_progress(app, status, combo_index, total_combos);

            furi_hal_serial_set_br(handle, baud_options[b]);

            uint8_t discard;
            while(furi_stream_buffer_receive(link.rx_stream, &discard, 1, 0) > 0) {
            }

            /* Tier 0: GPS/NMEA, passive - see link_listen_for_nmea(). */
            char capture[CAPTURE_MAX];
            if(link_listen_for_nmea(&link, capture, sizeof(capture), NMEA_LISTEN_MS)) {
                snprintf(banner, sizeof(banner), "GPS (NMEA): %.100s", capture);
                found = true;
                found_pin = p;
                found_baud = baud_options[b];
                break;
            }

            /* Tier 1: ESP-AT. */
            furi_hal_serial_tx(handle, (const uint8_t*)"AT\r\n", 4);
            if(link_expect_ok(&link, AT_TIMEOUT_MS)) {
                furi_hal_serial_tx(handle, (const uint8_t*)"AT+GMR\r\n", 8);
                if(!link_get_firmware_banner(&link, banner, sizeof(banner), GMR_TIMEOUT_MS)) {
                    snprintf(banner, sizeof(banner), "ESP-AT (no version banner)");
                } else {
                    char with_prefix[LINE_MAX];
                    snprintf(with_prefix, sizeof(with_prefix), "ESP-AT: %.100s", banner);
                    strncpy(banner, with_prefix, sizeof(banner) - 1);
                    banner[sizeof(banner) - 1] = '\0';
                }
                found = true;
                found_pin = p;
                found_baud = baud_options[b];
                break;
            }

            /* Tier 2: known non-AT firmware signatures. */
            bool tier2_found = false;
            for(size_t s = 0; s < SIGNATURE_COUNT; s++) {
                if(link_capture_response(
                       &link, signatures[s].probe, capture, sizeof(capture), FINGERPRINT_TIMEOUT_MS) &&
                   contains_ci(capture, signatures[s].match)) {
                    const char* clean = lookup_known_tag(capture);
                    if(clean != NULL) {
                        snprintf(banner, sizeof(banner), "%.100s", clean);
                    } else {
                        snprintf(banner, sizeof(banner), "Match \"%.20s\": %.100s", signatures[s].match, capture);
                    }
                    found = true;
                    tier2_found = true;
                    found_pin = p;
                    found_baud = baud_options[b];
                    break;
                }
            }
            if(tier2_found) break;

            /* Tier 3: nothing recognized by name. Requiring the same
               probe to repeat identically twice is what actually
               distinguishes a real command-response from a device
               that's continuously outputting something on its own (a
               boot log, a crash loop) getting caught mid-transmission,
               or a baud mismatch scrambling readable bytes into other
               readable-looking garbage - both of which produced a
               different result on every scan during this app's own
               bring-up before this check existed. */
            char capture_repeat[CAPTURE_MAX];
            if(link_capture_response(&link, "?", capture, sizeof(capture), FINGERPRINT_TIMEOUT_MS) &&
               link_capture_response(
                   &link, "?", capture_repeat, sizeof(capture_repeat), FINGERPRINT_TIMEOUT_MS) &&
               strcmp(capture, capture_repeat) == 0) {
                const char* clean = lookup_known_tag(capture);
                if(clean != NULL) {
                    snprintf(banner, sizeof(banner), "%.100s", clean);
                } else {
                    snprintf(banner, sizeof(banner), "Unidentified: %.100s", capture);
                }
                found = true;
                found_pin = p;
                found_baud = baud_options[b];
                break;
            }
        }

        /* Order matters: stop the receive interrupt before anything
           else, free the buffer it writes into last - see esp_at.c in
           the companion fox_chameleon project for the full story on
           why (a real Flipper firmware bug class, PR #4246). */
        furi_hal_serial_async_rx_stop(handle);
        if(owned) furi_hal_serial_deinit(handle);
        furi_hal_serial_control_release(handle);
        expansion_enable(expansion);
        furi_record_close(RECORD_EXPANSION);
        furi_stream_buffer_free(link.rx_stream);
    }

    if(app->cancel_requested) return 0;
    post_result(app, found, any_pin_claimed, found ? banner : "", found_pin, found_baud);
    return 0;
}

static void start_scan(App* app) {
    if(app->scanning) return;
    app->scanning = true;
    app->scan_done = false;
    app->cancel_requested = false;

    if(app->scan_thread != NULL) {
        furi_thread_join(app->scan_thread);
        furi_thread_free(app->scan_thread);
    }
    app->scan_thread = furi_thread_alloc_ex("FoxDetectorScan", 2048, scan_worker, app);
    furi_thread_start(app->scan_thread);
}

static void scan_button_callback(GuiButtonType result, InputType type, void* context) {
    App* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) start_scan(app);
}

/* --- Progress: custom View (not the Widget) for the scan-in-progress
   screen. ---
   Same "static App* pointer set once, read from the draw callback"
   pattern fox_chameleon's candidate/terminal/settings views use, for the
   same reason - the draw callback only gets the throwaway model
   allocated for it, not the context set via view_set_context() (only
   the input callback gets that).

   Replaces the old Widget-based render_progress(), which drew the bar
   as a literal "####------" string - functional, but exactly the "looks
   terrible" ASCII look the user asked to move away from. This instead
   hand-draws a rounded outer frame with a smaller rounded fill inset
   inside it, the same rounded-rect vocabulary Flipper's own firmware
   update/install progress screens use. */
static App* s_progress_view_app = NULL;

#define PROGRESS_TITLE_Y     2
#define PROGRESS_COUNTER_Y   16
#define PROGRESS_BAR_X       10
#define PROGRESS_BAR_Y       30
#define PROGRESS_BAR_W       108
#define PROGRESS_BAR_H       14
#define PROGRESS_BAR_RADIUS  4
#define PROGRESS_BAR_PAD     2
#define PROGRESS_STATUS_Y    52

static void progress_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_progress_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, PROGRESS_TITLE_Y, AlignCenter, AlignTop, "Scanning...");

    canvas_set_font(canvas, FontSecondary);
    char counter[24];
    snprintf(
        counter,
        sizeof(counter),
        "%u / %u",
        (unsigned)app->progress_display_index,
        (unsigned)app->progress_display_total);
    canvas_draw_str_aligned(canvas, 64, PROGRESS_COUNTER_Y, AlignCenter, AlignTop, counter);

    canvas_draw_rframe(
        canvas, PROGRESS_BAR_X, PROGRESS_BAR_Y, PROGRESS_BAR_W, PROGRESS_BAR_H, PROGRESS_BAR_RADIUS);

    size_t total = app->progress_display_total;
    size_t index = app->progress_display_index;
    if(index > total) index = total;

    int32_t inner_w = PROGRESS_BAR_W - PROGRESS_BAR_PAD * 2;
    int32_t inner_h = PROGRESS_BAR_H - PROGRESS_BAR_PAD * 2;
    int32_t fill_w = (total > 0) ? (int32_t)((size_t)inner_w * index / total) : 0;
    if(fill_w > inner_w) fill_w = inner_w;

    if(fill_w > 0) {
        /* Radius has to shrink with the fill - a full-height sliver at
           the start of a scan can't take the same radius as the full
           bar without canvas_draw_rbox() drawing something that looks
           broken rather than just small. */
        int32_t fill_radius = PROGRESS_BAR_RADIUS - PROGRESS_BAR_PAD;
        if(fill_radius < 0) fill_radius = 0;
        int32_t max_radius = (fill_w < inner_h ? fill_w : inner_h) / 2;
        if(fill_radius > max_radius) fill_radius = max_radius;
        canvas_draw_rbox(
            canvas,
            PROGRESS_BAR_X + PROGRESS_BAR_PAD,
            PROGRESS_BAR_Y + PROGRESS_BAR_PAD,
            fill_w,
            inner_h,
            fill_radius);
    }

    canvas_draw_str_aligned(
        canvas, 64, PROGRESS_STATUS_Y, AlignCenter, AlignTop, app->progress_display_status);
}

static bool progress_input_cb(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    /* No buttons on this screen, same as the old Widget-based version -
       Back falls through to navigation_callback and exits the app,
       the only Back behavior this single-screen app has ever had. */
    return false;
}

static void render_result(App* app, bool found, bool any_pin_claimed, const char* banner, size_t pin, uint32_t baud) {
    widget_reset(app->widget);

    char text[LINE_MAX + 64];
    if(found) {
        /* %.20s / %.100s, not bare %s: text is a fixed-size local, but
           pin_options[pin].label and banner are both plain pointer
           parameters here, so the compiler can't see any bound on them
           on its own - same -Wformat-truncation fix as the spots in
           scan_worker() an actual build already flagged once. */
        snprintf(
            text,
            sizeof(text),
            "Found a response:\nPins: %.20s\nBaud: %lu\n%.100s",
            pin_options[pin].label,
            (unsigned long)baud,
            banner);
    } else if(any_pin_claimed) {
        snprintf(
            text,
            sizeof(text),
            "Nothing responded on\nany pin/baud combo.\nCheck wiring and power -\nsee README for what\nthis can and can't\nrecognize.");
    } else {
        snprintf(
            text,
            sizeof(text),
            "Could not claim any\nUART pins at all.\nAnother app may be\nusing them.");
    }

    widget_add_text_box_element(app->widget, 0, 0, 128, 50, AlignLeft, AlignTop, text, false);
    widget_add_button_element(app->widget, GuiButtonTypeCenter, "Rescan", scan_button_callback, app);
}

static bool custom_event_callback(void* context, uint32_t event) {
    App* app = context;
    if(event != DetectorEventProgress) return false;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool done = app->scan_done;
    size_t index = app->progress_index;
    size_t total = app->progress_total;
    char status[64];
    strncpy(status, app->progress_status, sizeof(status) - 1);
    status[sizeof(status) - 1] = '\0';
    bool found = app->result_found;
    bool any_pin_claimed = app->result_any_pin_claimed;
    char banner[LINE_MAX];
    strncpy(banner, app->result_banner, sizeof(banner) - 1);
    banner[sizeof(banner) - 1] = '\0';
    size_t pin = app->result_pin;
    uint32_t baud = app->result_baud;
    furi_mutex_release(app->mutex);

    if(!done) {
        /* Snapshot into the display-only fields progress_draw_cb() reads -
           see their comment on App for why this is safe without the
           mutex (both this callback and every draw callback run on the
           view dispatcher's own thread). */
        app->progress_display_index = index;
        app->progress_display_total = total;
        strncpy(app->progress_display_status, status, sizeof(app->progress_display_status) - 1);
        app->progress_display_status[sizeof(app->progress_display_status) - 1] = '\0';
        view_dispatcher_switch_to_view(app->view_dispatcher, DetectorViewProgress);
        with_view_model(app->progress_view, uint8_t * _m, { UNUSED(_m); }, true);
    } else {
        render_result(app, found, any_pin_claimed, banner, pin, baud);
        view_dispatcher_switch_to_view(app->view_dispatcher, DetectorViewResult);
        app->scanning = false;
    }
    return true;
}

static bool navigation_callback(void* context) {
    App* app = context;
    if(app->scanning) app->cancel_requested = true;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);

    app->widget = widget_alloc();
    /* Two short lines, well clear of the Scan button's fixed bottom
       anchor - a longer, five-line version of this screen silently
       line-wrapped past its intended length and overlapped the button,
       which has no configurable position of its own to compensate
       with. */
    widget_add_string_multiline_element(
        app->widget,
        2,
        2,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "Fox ESP32 Detector\n\n"
        "Checks what's on\n"
        "GPIO. Press Scan.");
    widget_add_button_element(app->widget, GuiButtonTypeCenter, "Scan", scan_button_callback, app);

    /* Progress - see progress_draw_cb()/progress_input_cb() above for why
       this is a custom View, not the Widget the idle/result screens use. */
    app->progress_view = view_alloc();
    view_set_draw_callback(app->progress_view, progress_draw_cb);
    view_set_input_callback(app->progress_view, progress_input_cb);
    view_set_context(app->progress_view, app);
    view_allocate_model(app->progress_view, ViewModelTypeLocking, sizeof(uint8_t));
    s_progress_view_app = app;

    view_dispatcher_add_view(
        app->view_dispatcher, DetectorViewResult, widget_get_view(app->widget));
    view_dispatcher_add_view(app->view_dispatcher, DetectorViewProgress, app->progress_view);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, DetectorViewResult);

    return app;
}

static void app_free(App* app) {
    if(app->scan_thread != NULL) {
        furi_thread_join(app->scan_thread);
        furi_thread_free(app->scan_thread);
    }

    view_dispatcher_remove_view(app->view_dispatcher, DetectorViewResult);
    view_dispatcher_remove_view(app->view_dispatcher, DetectorViewProgress);
    widget_free(app->widget);
    view_free(app->progress_view);
    s_progress_view_app = NULL;
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    furi_mutex_free(app->mutex);
    free(app);
}

int32_t fox_esp32_detector_main(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
