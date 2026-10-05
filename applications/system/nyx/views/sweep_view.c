#include "sweep_view.h"
#include <furi.h>
#include <gui/gui.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* The sweep screen is a locating instrument, not a dashboard. Everything on it
 * answers one of two questions you ask while walking a room: "is there IR here"
 * and "am I getting warmer".
 *
 * The hero is an eye — Nyx watching back. Its ring fills clockwise with the
 * live level, a tick marks your best reading so far, and the pupil dilates as
 * you close on a source; when it locks on, glare spikes rotate around the iris.
 * A trend arrow on the right says whether you are getting warmer. You hunt by
 * watching the ring fill and the pupil widen, not by reading the number.
 *
 * Every measurement is named in exactly one place. Proximity (FAINT..STRONG)
 * lives in the alarm strip and nowhere else; while an emitter is locked on, the
 * readout's middle row switches to the raw telemetry for the live mode instead
 * of repeating the word.
 *
 * Layout of the 128x64:
 *   y 0..11   header: mark, sensitivity, active mode, live dot
 *   y 12..51  eye gauge (left) + readout: kind, state/telemetry, peak/hits
 *   y 53..63  status strip, inverted into an alarm when locked on
 */

/* The eye is a little smaller and a little lower than it looks like it could
 * be, because the alarm border runs along the screen edge and everything has to
 * keep clear of it. See the margin note by the border itself. */
#define EYE_CX   21
#define EYE_CY   27
#define EYE_ROUT 13
#define EYE_IRIS 8

#define STRIP_TOP 53 // first row of the status strip; nothing above may cross it

struct SweepView {
    View* view;
    SweepViewCallback cb;
    void* cb_ctx;
};

typedef struct {
    bool armed;
    IrSenseError error;
    IrSenseMode active_mode;
    bool present;
    uint8_t level;
    uint8_t peak;
    int8_t trend;
    IrSourceKind kind;
    uint32_t hits;
    uint16_t baseline_mv;
    uint16_t raw_mv;
    uint16_t ripple_mv;
    uint32_t edges_per_sec;
    uint8_t sensitivity;
    uint8_t anim;
} SweepModel;

static const char* proximity_word(uint8_t level) {
    if(level >= 70) return "STRONG";
    if(level >= 45) return "CLOSE";
    if(level >= 20) return "NEAR";
    return "FAINT";
}

/* Short enough for the header, and the same three words the Settings list uses
 * so the two screens cannot be read as different scales. */
static const char* sens_word(uint8_t index) {
    switch(index) {
    case 0:
        return "HIGH";
    case 2:
        return "LOW";
    default:
        return "MED";
    }
}

static void draw_error(Canvas* canvas, const SweepModel* m) {
    canvas_set_font(canvas, FontPrimary);

    if(m->error == IrSenseErrorIrBusy) {
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "IR receiver busy");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Close any other IR app,");
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "then re-open the sweep.");
    } else if(m->error == IrSenseErrorNoProbe) {
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "No probe on the pin");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Check Probe Setup wiring");
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "or set Mode to Auto.");
    } else {
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "ADC unavailable");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Another app holds it.");
    }
}

static void draw_nulling(Canvas* canvas, const SweepModel* m) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "Nulling ambient");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignBottom, "Hold still, aim where");
    canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignBottom, "you will sweep");
    /* three dots filling in, so the wait reads as progress not a hang */
    for(uint8_t i = 0; i < 3; i++) {
        int x = 56 + i * 8;
        if((m->anim / 3u) % 3u >= i) {
            canvas_draw_disc(canvas, x, 57, 2);
        } else {
            canvas_draw_circle(canvas, x, 57, 2);
        }
    }
}

/* "getting warmer" arrow, centered on (cx,cy). Up = rising, down = falling,
 * a flat bar = holding steady. */
static void draw_trend_at(Canvas* canvas, int cx, int cy, int8_t trend) {
    if(trend > 0) {
        canvas_draw_triangle(canvas, cx, cy + 4, 12, 9, CanvasDirectionBottomToTop);
    } else if(trend < 0) {
        canvas_draw_triangle(canvas, cx, cy - 4, 12, 9, CanvasDirectionTopToBottom);
    } else {
        canvas_draw_box(canvas, cx - 6, cy - 1, 12, 3);
    }
}

/* point on a circle of the given radius, angle in degrees clockwise from 12 o'clock */
static void ring_point(int radius, int deg, int* x, int* y) {
    float a = (float)(deg - 90) * (float)M_PI / 180.0f;
    *x = EYE_CX + (int)(cosf(a) * radius);
    *y = EYE_CY + (int)(sinf(a) * radius);
}

static void draw_eye(Canvas* canvas, const SweepModel* m) {
    /* outer ring */
    canvas_draw_circle(canvas, EYE_CX, EYE_CY, EYE_ROUT);

    /* proximity arc: fills clockwise from 12 o'clock, length proportional to level */
    int span = (m->level * 360) / 100;
    for(int deg = 0; deg < span; deg += 5) {
        int x, y;
        ring_point(EYE_ROUT - 1, deg, &x, &y);
        canvas_draw_dot(canvas, x, y);
        ring_point(EYE_ROUT - 2, deg, &x, &y);
        canvas_draw_dot(canvas, x, y);
    }

    /* peak tick: the reading to beat while you hunt for the hot spot */
    if(m->peak > 0) {
        int xi, yi, xo, yo;
        ring_point(EYE_ROUT - 4, (m->peak * 360) / 100, &xi, &yi);
        ring_point(EYE_ROUT + 1, (m->peak * 360) / 100, &xo, &yo);
        canvas_draw_line(canvas, xi, yi, xo, yo);
    }

    /* iris + pupil that dilates with the live level */
    canvas_draw_circle(canvas, EYE_CX, EYE_CY, EYE_IRIS);
    int pupil = 1 + (m->level * 7) / 100;
    canvas_draw_disc(canvas, EYE_CX, EYE_CY, pupil);

    /* lock-on glare: short spikes rotating just outside the iris */
    if(m->present) {
        for(int i = 0; i < 6; i++) {
            int deg = i * 60 + m->anim * 6;
            int x1, y1, x2, y2;
            ring_point(EYE_IRIS + 2, deg, &x1, &y1);
            ring_point(EYE_IRIS + 4, deg, &x2, &y2);
            canvas_draw_line(canvas, x1, y1, x2, y2);
        }
    }
}

/* Idle hint line. In onboard mode this is where Nyx keeps admitting what it
 * cannot see, because a clean-looking zero on this screen is exactly the
 * reading a DC illuminator produces. */
static void draw_hint(Canvas* canvas, const SweepModel* m) {
    char buf[32];
    const char* text;
    /* ~3 s per phase at a 100 ms tick. Onboard gets a fourth phase because it
     * is the mode that can be silently wrong, and the answer to that silence
     * (wire a probe) deserves to be on the screen rather than only in About. */
    uint8_t phase = (uint8_t)((m->anim / 30u) % 4u);

    if(!m->armed) {
        text = "Idle";
    } else if(m->active_mode == IrSenseModeOnboard) {
        if(phase == 0) {
            text = "Onboard: pulsed IR only";
        } else if(phase == 1) {
            /* %lu is budgeted at 10 digits by -Wformat-truncation, so the
             * buffer is sized for the worst case rather than the real range. */
            snprintf(buf, sizeof(buf), "%lu edges/s", (unsigned long)m->edges_per_sec);
            text = buf;
        } else if(phase == 2) {
            text = "Steady IR? Use Probe";
        } else {
            text = "OK zero    < > sens";
        }
    } else {
        if(phase == 0 || phase == 2) {
            snprintf(
                buf,
                sizeof(buf),
                "amb %umV  rip %umV",
                (unsigned)m->baseline_mv,
                (unsigned)m->ripple_mv);
            text = buf;
        } else if(phase == 1) {
            text = "OK zero   Hold OK null";
        } else {
            text = "< > sensitivity";
        }
    }
    canvas_draw_str(canvas, 2, 62, text);
}

static void sweep_view_draw(Canvas* canvas, void* model) {
    SweepModel* m = model;
    char buf[24];

    /* ---------- header ---------- */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 10, "NYX");
    canvas_draw_str(canvas, 26, 10, sens_word(m->sensitivity));

    /* Until a worker has resolved Auto there is no honest answer, so say so
     * rather than naming a mode that may not be the one that comes up. */
    const char* mode_word = !m->armed                              ? "--" :
                            (m->active_mode == IrSenseModeProbe)   ? "PROBE" :
                                                                     "ONBOARD";
    canvas_draw_str_aligned(canvas, 114, 10, AlignRight, AlignBottom, mode_word);
    if(m->present) {
        canvas_draw_disc(canvas, 120, 6, 2);
    } else {
        canvas_draw_circle(canvas, 120, 6, 2);
    }
    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(m->error != IrSenseErrorNone) {
        draw_error(canvas, m);
        return;
    }

    /* Probe mode holds the meter back until the ambient null is captured —
     * a reading before then would be measuring the room, not the emitter. */
    if(m->armed && m->active_mode == IrSenseModeProbe && m->baseline_mv == 0) {
        draw_nulling(canvas, m);
        return;
    }

    /* ---------- eye gauge (left) ---------- */
    draw_eye(canvas, m);
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)m->level);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, EYE_CX, 49, AlignCenter, AlignBottom, buf);

    /* ---------- readout (right) ---------- */
    canvas_draw_line(canvas, 41, 14, 41, 49);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 45, 23, ir_sense_source_kind_str(m->kind));

    canvas_set_font(canvas, FontSecondary);
    if(m->present) {
        /* Locked on: the strip is already shouting the proximity word, so this
         * row shows the number behind it instead of saying the same thing. */
        if(m->active_mode == IrSenseModeProbe) {
            uint16_t excess = (m->raw_mv > m->baseline_mv) ?
                                  (uint16_t)(m->raw_mv - m->baseline_mv) :
                                  0u;
            snprintf(buf, sizeof(buf), "+%umV", (unsigned)excess);
        } else {
            snprintf(buf, sizeof(buf), "%lu/s", (unsigned long)m->edges_per_sec);
        }
        canvas_draw_str(canvas, 45, 36, buf);
    } else {
        canvas_draw_str(canvas, 45, 36, m->armed ? "SCANNING" : "IDLE");
    }

    snprintf(buf, sizeof(buf), "PK%u  HIT%lu", (unsigned)m->peak, (unsigned long)m->hits);
    canvas_draw_str(canvas, 45, 49, buf);

    draw_trend_at(canvas, 114, 30, m->trend);

    /* ---------- status strip ---------- */
    canvas_draw_line(canvas, 0, 52, 127, 52);
    if(m->present) {
        canvas_draw_box(canvas, 0, STRIP_TOP, 128, 64 - STRIP_TOP);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, 4, 58, 1);
        canvas_draw_str(canvas, 9, 62, "IR EMITTER");
        canvas_draw_str_aligned(
            canvas, 125, 62, AlignRight, AlignBottom, proximity_word(m->level));
        canvas_set_color(canvas, ColorBlack);

        /* Alarm border, with a real margin inside it.
         *
         * The frame has to sit on the screen edge, so the clearance has to come
         * from the content instead. Everything inside is laid out to leave at
         * least two blank pixels against it: the header baseline is 10, so its
         * glyphs occupy rows 3-10 and rows 1-2 stay empty; "NYX" starts at x=4;
         * the live dot ends at x=122; the trend arrow ends at x=120; and the
         * eye is drawn at radius 13 rather than 15 so its left edge lands at
         * x=8. The frame also stops above the strip on purpose — a frame's
         * bottom edge is real ink, and one row lower it would land exactly on
         * the baseline of the strip's own text and shave it. */
        canvas_draw_frame(canvas, 0, 0, 128, STRIP_TOP);
    } else {
        draw_hint(canvas, m);
    }
}

static void sweep_view_emit(SweepView* v, SweepViewEvent e) {
    if(v->cb) v->cb(v->cb_ctx, e);
}

static bool sweep_view_input(InputEvent* event, void* context) {
    SweepView* v = context;

    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyOk:
            sweep_view_emit(v, SweepViewEventResetPeak);
            return true;
        case InputKeyRight:
            sweep_view_emit(v, SweepViewEventSensUp);
            return true;
        case InputKeyLeft:
            sweep_view_emit(v, SweepViewEventSensDown);
            return true;
        default:
            return false; // Back belongs to the dispatcher
        }
    }

    /* Held Left/Right repeat, so you can run the gain down without 3 presses. */
    if(event->type == InputTypeRepeat) {
        if(event->key == InputKeyRight) {
            sweep_view_emit(v, SweepViewEventSensUp);
            return true;
        }
        if(event->key == InputKeyLeft) {
            sweep_view_emit(v, SweepViewEventSensDown);
            return true;
        }
        return false;
    }

    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        sweep_view_emit(v, SweepViewEventRenull);
        return true;
    }

    return false;
}

SweepView* sweep_view_alloc(void) {
    SweepView* v = malloc(sizeof(SweepView));
    memset(v, 0, sizeof(SweepView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, sweep_view_draw);
    view_set_input_callback(v->view, sweep_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(SweepModel));
    return v;
}

void sweep_view_free(SweepView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* sweep_view_get_view(SweepView* v) {
    furi_assert(v);
    return v->view;
}

void sweep_view_set_callback(SweepView* v, SweepViewCallback cb, void* context) {
    furi_assert(v);
    v->cb = cb;
    v->cb_ctx = context;
}

void sweep_view_update(SweepView* v, const IrStats* stats, uint8_t sensitivity) {
    furi_assert(v);
    with_view_model(
        v->view,
        SweepModel * m,
        {
            m->armed = stats->armed;
            m->error = stats->error;
            m->active_mode = stats->active_mode;
            m->present = stats->present;
            m->level = stats->level;
            m->peak = stats->peak;
            m->trend = stats->trend;
            m->kind = stats->kind;
            m->hits = stats->hits;
            m->baseline_mv = stats->baseline_mv;
            m->raw_mv = stats->raw_mv;
            m->ripple_mv = stats->ripple_mv;
            m->edges_per_sec = stats->edges_per_sec;
            m->sensitivity = sensitivity;
        },
        true);
}

void sweep_view_tick(SweepView* v) {
    furi_assert(v);
    with_view_model(v->view, SweepModel * m, { m->anim++; }, true);
}

void sweep_view_reset(SweepView* v, uint8_t sensitivity) {
    furi_assert(v);
    with_view_model(
        v->view,
        SweepModel * m,
        {
            memset(m, 0, sizeof(SweepModel));
            /* Index 0 is HIGH, so a plain memset would mislabel the header for
             * the first frame of every sweep. */
            m->sensitivity = sensitivity;
        },
        true);
}
