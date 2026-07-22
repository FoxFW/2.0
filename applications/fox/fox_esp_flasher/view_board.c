#include "fox_esp_flasher.h"

static FlasherApp* s_app = NULL;

typedef struct {
    uint8_t row;       /* 0 = board selector, 1 = action button */
} BoardModel;

#define BOX_X 4
#define BOX_W 120
#define BOX_H 22
#define BOX_R 4

static const uint8_t k_box_y[2] = {15, 40};

static void board_draw(Canvas* canvas, void* model_ptr) {
    BoardModel* bm = model_ptr;
    FlasherApp* app = s_app;
    if(!app) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Select Board");

    for(uint8_t row = 0; row < 2; row++) {
        bool sel = (bm->row == row);
        uint8_t y = k_box_y[row];

        canvas_set_color(canvas, ColorBlack);
        if(sel) {
            canvas_draw_rbox(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_draw_rframe(canvas, BOX_X + 2, y + 2, BOX_W - 4, BOX_H - 4, BOX_R - 2);
        }

        uint8_t ty = y + BOX_H / 2;
        canvas_set_font(canvas, FontSecondary);

        if(row == 0) {
            canvas_draw_str_aligned(canvas, 64, ty, AlignCenter, AlignCenter,
                                    k_flasher_boards[app->board_index].label);
            canvas_draw_str_aligned(canvas, BOX_X + 6,         ty, AlignLeft,  AlignCenter, "<");
            canvas_draw_str_aligned(canvas, BOX_X + BOX_W - 6, ty, AlignRight, AlignCenter, ">");
        } else {
            const char* label = app->board_custom ? "Select Files" : "Install";
            canvas_draw_str_aligned(canvas, 64, ty, AlignCenter, AlignCenter, label);
        }
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool board_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        with_view_model(app->board_view, BoardModel* m, {
            m->row = (m->row == 0) ? 1 : 0;
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->board_view, BoardModel* m, {
            m->row = (m->row == 0) ? 1 : 0;
        }, true);
        return true;
    case InputKeyLeft:
    case InputKeyRight: {
        uint8_t row = 0;
        with_view_model(app->board_view, BoardModel* m, { row = m->row; }, false);
        if(row == 0) {
            if(event->key == InputKeyRight) {
                app->board_index = (app->board_index + 1) % FLASHER_BOARD_COUNT;
            } else {
                app->board_index = (app->board_index == 0)
                                       ? FLASHER_BOARD_COUNT - 1
                                       : app->board_index - 1;
            }
            with_view_model(app->board_view, BoardModel* m, { UNUSED(m); }, true);
        }
        return true;
    }
    case InputKeyOk: {
        uint8_t row = 0;
        with_view_model(app->board_view, BoardModel* m, { row = m->row; }, false);
        if(row == 1) {
            view_dispatcher_send_custom_event(app->view_dispatcher, FlasherEventBoardGo);
        }
        return true;
    }
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* view_board_alloc(FlasherApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, board_draw);
    view_set_input_callback(v, board_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(BoardModel));
    with_view_model(v, BoardModel* m, { m->row = 0; }, false);
    return v;
}

void view_board_free(View* v) {
    s_app = NULL;
    view_free(v);
}

void view_board_refresh(View* v) {
    with_view_model(v, BoardModel* m, { UNUSED(m); }, true);
}
