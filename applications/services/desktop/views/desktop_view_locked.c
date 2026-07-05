#include <furi.h>

#include <gui/elements.h>
#include <gui/view.h>

#include "../desktop_i.h"
#include "desktop_view_locked.h"

#define LOCK_DISPLAY_TIMEOUT_MS  (1500)
#define LOCKED_HINT_TIMEOUT_MS   (1000)
#define UNLOCKED_HINT_TIMEOUT_MS (2000)

#define UNLOCK_CNT         (3)
#define UNLOCK_RST_TIMEOUT (600)

struct DesktopViewLocked {
    View* view;
    DesktopViewLockedCallback callback;
    void* context;

    FuriTimer* timer;
    uint8_t lock_count;
    uint32_t lock_lastpress;
};

typedef enum {
    DesktopViewLockedStateUnlocked,
    DesktopViewLockedStateLocked,
    DesktopViewLockedStateDoorsClosing,
    DesktopViewLockedStateLockedHintShown,
    DesktopViewLockedStateUnlockedHintShown
} DesktopViewLockedState;

typedef struct {
    bool pin_locked;
    int8_t door_offset;
    DesktopViewLockedState view_state;
    uint8_t back_presses;
} DesktopViewLockedModel;

void desktop_view_locked_set_callback(
    DesktopViewLocked* locked_view,
    DesktopViewLockedCallback callback,
    void* context) {
    furi_assert(locked_view);
    furi_assert(callback);
    locked_view->callback = callback;
    locked_view->context = context;
}

static void locked_view_timer_callback(void* context) {
    DesktopViewLocked* locked_view = context;
    locked_view->callback(DesktopLockedEventUpdate, locked_view->context);
}

static void desktop_view_locked_update_hint_icon_timeout(DesktopViewLocked* locked_view) {
    DesktopViewLockedModel* model = view_get_model(locked_view->view);
    const bool change_state = (model->view_state == DesktopViewLockedStateLocked) &&
                              !model->pin_locked;
    if(change_state) {
        model->view_state = DesktopViewLockedStateLockedHintShown;
    }
    view_commit_model(locked_view->view, change_state);
    furi_timer_start(locked_view->timer, LOCKED_HINT_TIMEOUT_MS);
}

void desktop_view_locked_update(DesktopViewLocked* locked_view) {
    DesktopViewLockedModel* model = view_get_model(locked_view->view);
    DesktopViewLockedState view_state = model->view_state;

    if(view_state == DesktopViewLockedStateLockedHintShown) {
        model->view_state = DesktopViewLockedStateLocked;
    } else if(view_state == DesktopViewLockedStateUnlockedHintShown) {
        model->view_state = DesktopViewLockedStateUnlocked;
    }

    view_commit_model(locked_view->view, true);
    furi_timer_stop(locked_view->timer);
}

// Draw a closed padlock using primitives only (no icon dependency)
static void draw_padlock_closed(Canvas* canvas) {
    uint8_t cx = 64;
    uint8_t by = 31; // body top Y

    // Shackle: two legs + top bar (closed arch)
    canvas_draw_box(canvas, cx - 7, by - 10, 3, 11); // left leg
    canvas_draw_box(canvas, cx + 4, by - 10, 3, 11); // right leg
    canvas_draw_box(canvas, cx - 7, by - 10, 14, 3); // top bar

    // Body
    canvas_draw_rbox(canvas, cx - 10, by, 20, 16, 2);

    // Keyhole (white)
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_disc(canvas, cx, by + 6, 3);
    canvas_draw_box(canvas, cx - 1, by + 6, 3, 4);
    canvas_set_color(canvas, ColorBlack);
}

// Draw an open padlock (shackle lifted, visible on the right)
static void draw_padlock_open(Canvas* canvas) {
    uint8_t cx = 64;
    uint8_t by = 31;

    // Lifted shackle (only right leg + top going to the right)
    canvas_draw_box(canvas, cx + 4, by - 2,  3, 4);  // right leg stub in body hole
    canvas_draw_box(canvas, cx + 4, by - 14, 3, 12); // right leg above body
    canvas_draw_box(canvas, cx + 4, by - 14, 9, 3);  // top bar going right

    // Body
    canvas_draw_rbox(canvas, cx - 10, by, 20, 16, 2);

    // Keyhole (white)
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_disc(canvas, cx, by + 6, 3);
    canvas_draw_box(canvas, cx - 1, by + 6, 3, 4);
    canvas_set_color(canvas, ColorBlack);
}

static void desktop_view_locked_draw(Canvas* canvas, void* model) {
    DesktopViewLockedModel* m = model;
    DesktopViewLockedState view_state = m->view_state;

    // Unlocked state is transparent — let underlying views render through
    if(view_state == DesktopViewLockedStateUnlocked) return;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(view_state == DesktopViewLockedStateLocked ||
       view_state == DesktopViewLockedStateDoorsClosing) {
        draw_padlock_closed(canvas);

    } else if(view_state == DesktopViewLockedStateLockedHintShown) {
        draw_padlock_closed(canvas);
        if(!m->pin_locked) {
            uint8_t remaining = UNLOCK_CNT - m->back_presses;
            char hint[22];
            snprintf(hint, sizeof(hint), "Back x%u to unlock", (unsigned)remaining);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignBottom, hint);
        }

    } else if(view_state == DesktopViewLockedStateUnlockedHintShown) {
        draw_padlock_open(canvas);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignBottom, "Unlocked");
    }
}

View* desktop_view_locked_get_view(DesktopViewLocked* locked_view) {
    furi_assert(locked_view);
    return locked_view->view;
}

static bool desktop_view_locked_input(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    bool is_changed = false;
    const uint32_t press_time = furi_get_tick();
    DesktopViewLocked* locked_view = context;
    DesktopViewLockedModel* model = view_get_model(locked_view->view);
    if(model->view_state == DesktopViewLockedStateUnlockedHintShown &&
       event->type == InputTypePress) {
        model->view_state = DesktopViewLockedStateUnlocked;
        is_changed = true;
    }
    const DesktopViewLockedState view_state = model->view_state;
    const bool pin_locked = model->pin_locked;
    view_commit_model(locked_view->view, is_changed);

    if(view_state == DesktopViewLockedStateUnlocked) {
        return false;
    } else if(view_state == DesktopViewLockedStateLocked && pin_locked) {
        locked_view->callback(DesktopLockedEventShowPinInput, locked_view->context);
    } else if(
        view_state == DesktopViewLockedStateLocked ||
        view_state == DesktopViewLockedStateLockedHintShown) {
        if(press_time - locked_view->lock_lastpress > UNLOCK_RST_TIMEOUT) {
            locked_view->lock_lastpress = press_time;
            locked_view->lock_count = 0;
            DesktopViewLockedModel* mr = view_get_model(locked_view->view);
            mr->back_presses = 0;
            view_commit_model(locked_view->view, true);
        }

        desktop_view_locked_update_hint_icon_timeout(locked_view);

        if(event->key == InputKeyBack) {
            if(event->type == InputTypeShort) {
                locked_view->lock_lastpress = press_time;
                locked_view->lock_count++;
                DesktopViewLockedModel* mb = view_get_model(locked_view->view);
                mb->back_presses = locked_view->lock_count;
                view_commit_model(locked_view->view, true);
                if(locked_view->lock_count == UNLOCK_CNT) {
                    locked_view->callback(DesktopLockedEventUnlocked, locked_view->context);
                }
            }
        } else {
            locked_view->lock_count = 0;
            DesktopViewLockedModel* mo = view_get_model(locked_view->view);
            mo->back_presses = 0;
            view_commit_model(locked_view->view, true);
        }

        locked_view->lock_lastpress = press_time;
    }

    return true;
}

DesktopViewLocked* desktop_view_locked_alloc(void) {
    DesktopViewLocked* locked_view = malloc(sizeof(DesktopViewLocked));
    locked_view->view = view_alloc();
    locked_view->timer =
        furi_timer_alloc(locked_view_timer_callback, FuriTimerTypePeriodic, locked_view);

    view_allocate_model(locked_view->view, ViewModelTypeLocking, sizeof(DesktopViewLockedModel));
    view_set_context(locked_view->view, locked_view);
    view_set_draw_callback(locked_view->view, desktop_view_locked_draw);
    view_set_input_callback(locked_view->view, desktop_view_locked_input);

    return locked_view;
}

void desktop_view_locked_free(DesktopViewLocked* locked_view) {
    furi_assert(locked_view);
    furi_timer_free(locked_view->timer);
    view_free(locked_view->view);
    free(locked_view);
}

void desktop_view_locked_close_doors(DesktopViewLocked* locked_view) {
    DesktopViewLockedModel* model = view_get_model(locked_view->view);
    furi_assert(model->view_state == DesktopViewLockedStateLocked);
    view_commit_model(locked_view->view, false);
    furi_timer_start(locked_view->timer, LOCK_DISPLAY_TIMEOUT_MS);
}

void desktop_view_locked_lock(DesktopViewLocked* locked_view, bool pin_locked) {
    DesktopViewLockedModel* model = view_get_model(locked_view->view);
    furi_assert(model->view_state == DesktopViewLockedStateUnlocked);
    model->view_state = DesktopViewLockedStateLocked;
    model->pin_locked = pin_locked;
    model->back_presses = 0;
    view_commit_model(locked_view->view, true);
}

void desktop_view_locked_unlock(DesktopViewLocked* locked_view) {
    locked_view->lock_count = 0;
    DesktopViewLockedModel* model = view_get_model(locked_view->view);
    model->view_state = DesktopViewLockedStateUnlockedHintShown;
    model->pin_locked = false;
    model->back_presses = 0;
    view_commit_model(locked_view->view, true);
    furi_timer_start(locked_view->timer, UNLOCKED_HINT_TIMEOUT_MS);
}

bool desktop_view_locked_is_locked_hint_visible(DesktopViewLocked* locked_view) {
    DesktopViewLockedModel* model = view_get_model(locked_view->view);
    const DesktopViewLockedState view_state = model->view_state;
    view_commit_model(locked_view->view, false);
    return view_state == DesktopViewLockedStateLockedHintShown ||
           view_state == DesktopViewLockedStateLocked;
}
