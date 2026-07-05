#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <input/input.h>
#include <storage/storage.h>
#include <assets_icons.h>

/* Check whether a PIN is stored in Fox.data WITHOUT calling
 * desktop_pin_code_is_set() — that function lives in the firmware and would
 * need an api_symbols.csv export to be callable from a FAP.
 * Instead we read the raw Fox.data file and decode the pin_length byte.
 *
 * FoxSettingsData layout (all uint8_t after the header, __packed__):
 *   offset  0 : uint32_t magic          (4 bytes)
 *   offset  4 : uint16_t version        (2 bytes)
 *   offset  6 : uint8_t  override_flag  (1 byte)
 *   offset  7 : uint8_t  _pad           (1 byte)
 *   offset  8 : uint8_t  device_name[16](16 bytes)
 *   offset 24 : uint8_t  pin_hash[21]   (21 bytes)
 *   offset 45 : uint8_t  pin_length     (1 byte)  ← this is the one we need
 */
#define FOX_PIN_LENGTH_OFFSET  45
#define FOX_SETTINGS_XOR_KEY   0xAD
#define FOX_SETTINGS_INT_PATH  "/int/Fox.data"

static bool fox_setup_pin_is_set(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    bool has_pin = false;
    if(storage_file_open(f, FOX_SETTINGS_INT_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buf[FOX_PIN_LENGTH_OFFSET + 1];
        if(storage_file_read(f, buf, sizeof(buf)) == sizeof(buf)) {
            uint8_t pin_len = buf[FOX_PIN_LENGTH_OFFSET] ^ FOX_SETTINGS_XOR_KEY;
            has_pin = (pin_len > 0);
        }
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return has_pin;
}

#define PIN_MIN_LEN 1
#define PIN_MAX_LEN 10

#define FOX_SETUP_FLAG_DIR          "/ext/apps_data/fox_setup"
#define FOX_SETUP_FLAG_PATH         "/ext/apps_data/fox_setup/completed.flag"
#define FOX_SETUP_DONE_INT_PATH     "/int/fox_setup.done"  /* desktop checks this path */
#define FOX_SETUP_PENDING_NAME_PATH "/ext/apps_data/fox_setup/name.pending"
#define FOX_SETUP_PENDING_PIN_PATH  "/ext/apps_data/fox_setup/fox_pend.tmp"
#define FOX_SETUP_AUTO_ARG          "auto"
#define FOX_SETUP_WIPER_ARG         "wiper"

typedef enum {
    Page1_Welcome = 1,
    Page2_RenamePrompt,
    Page3_RenameAction,
    Page4_PinPrompt,
    Page5_PinAction,
    Page6_Complete,
} FoxSetupPage;

// If a PIN is already set we skip Page4 + Page5 entirely.
// Called wherever we would navigate to Page4_PinPrompt.
static FoxSetupPage fox_post_rename_page(void) {
    return fox_setup_pin_is_set() ? Page6_Complete : Page4_PinPrompt;
}

typedef struct {
    char    first[PIN_MAX_LEN + 1];
    uint8_t first_len;
    char    confirm[PIN_MAX_LEN + 1];
    uint8_t confirm_len;
    uint8_t selected_row;
    uint8_t selected_col;
    bool    confirming;
    bool    mismatch;
} PinState;

typedef struct {
    FoxSetupPage current_page;
    bool         completed;
    bool         name_changed;   /* true only when name.pending was written */
    bool         pin_changed;    /* true only when fox_pend.tmp was written */
    bool         wiper_launch;
    char         device_name[FURI_HAL_VERSION_ARRAY_NAME_LENGTH];
    PinState     pin;
    uint32_t     ignore_input_until;  // Debounces stale/buffered input events
                                       // that can be queued during the loader's
                                       // app-load transition when auto-launched
                                       // at boot. Not needed when launched from
                                       // the Apps menu (input queue is already
                                       // clean by then).
} FoxSetupModel;

typedef enum {
    ViewIdMain      = 0,
    ViewIdTextInput = 1,
} FoxSetupViewId;

typedef enum {
    EventRenameOk   = 0,
    EventRenameBack = 1,
} FoxSetupEvent;

typedef struct {
    ViewDispatcher* view_dispatcher;
    View*           main_view;
    TextInput*      text_input;
    FoxSetupModel*  model;
    FuriMutex*      mutex;
} FoxSetupApp;

// ===========================================================
// FLAG FILE  (simple existence check — no version hash needed)
// ===========================================================

static bool fox_setup_already_ran(void) {
    /* Check both flags so a single write failure can't cause the wizard to
     * reappear.  fox_setup writes /ext/.../completed.flag; the desktop writes
     * /int/fox_setup.done.  Either one existing means setup is done.        */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ext_done = storage_file_exists(storage, FOX_SETUP_FLAG_PATH);
    bool int_done = storage_file_exists(storage, FOX_SETUP_DONE_INT_PATH);
    furi_record_close(RECORD_STORAGE);
    return ext_done || int_done;
}

static void fox_setup_write_flag(void) {
    /* Write both the EXT completed.flag (checked by fox_setup_already_ran) and
     * the INT fox_setup.done (checked by the desktop launch thread).  Writing
     * two independent files means a single storage write failure can't cause
     * the wizard to reappear on the next boot.                              */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, FOX_SETUP_FLAG_DIR);

    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, FOX_SETUP_FLAG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, "1", 1);
        storage_file_close(f);
    }
    storage_file_free(f);

    f = storage_file_alloc(storage);
    if(storage_file_open(f, FOX_SETUP_DONE_INT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, "1", 1);
        storage_file_close(f);
    }
    storage_file_free(f);

    furi_record_close(RECORD_STORAGE);
}

// PIN is applied by desktop.c on next boot — avoids any extern dependency.
// Encodes digits using the same 2-key scheme as desktop_view_pin_input.c
// and writes fox_pend.tmp.  The desktop's 2-second tick picks this file up
// and applies the PIN within 2 seconds of fox_setup exiting — no reboot and
// no FAP-level desktop API access required.
static void fox_setup_write_pending_pin(const char* pin_data, uint8_t pin_len) {
    static const uint8_t k1[10] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2};
    static const uint8_t k2[10] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1};

    char encoded[PIN_MAX_LEN * 2];
    uint8_t enc_len = 0;
    for(uint8_t i = 0; i < pin_len && enc_len + 2 <= (uint8_t)sizeof(encoded); i++) {
        uint8_t d = (uint8_t)(pin_data[i] - '0') % 10;
        encoded[enc_len++] = (char)k1[d];
        encoded[enc_len++] = (char)k2[d];
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, FOX_SETUP_FLAG_DIR);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, FOX_SETUP_PENDING_PIN_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, &enc_len, sizeof(uint8_t));
        storage_file_write(f, encoded, enc_len);
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

// ===========================================================
// PIN DRAWING HELPERS — mirrors desktop_view_pin_input.c's grid layout so
// the wizard's PIN page matches the lock screen / PIN settings page exactly.
// ===========================================================

#define PIN_GRID_ROWS 3
#define PIN_GRID_COLS 4
#define PIN_KEY_WIDTH 25
#define PIN_KEY_HEIGHT 11
#define PIN_GRID_START_X 5
#define PIN_GRID_START_Y 24
#define PIN_X_GAP 2
#define PIN_Y_GAP 1
#define PIN_SIDEBAR_GAP_X 12

static const char* const pin_digit_text[9] = {
    "1", "2", "3",
    "4", "5", "6",
    "7", "8", "9"
};

static void fox_setup_draw_pin(Canvas* canvas, FoxSetupModel* model) {
    PinState* p = &model->pin;

    canvas_set_font(canvas, FontPrimary);
    if(p->mismatch) {
        canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "PIN Mismatch!");
    } else if(!p->confirming) {
        canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Set PIN:");
    } else {
        canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Confirm PIN:");
    }

    uint8_t cur_len = p->confirming ? p->confirm_len : p->first_len;

    if(cur_len > 0) {
        uint8_t dot_gap = (cur_len > 6) ? 5 : 7;
        int16_t dot_width = (int16_t)((cur_len * dot_gap) - 1);
        int16_t dot_start = 64 - (dot_width / 2);
        for(uint8_t i = 0; i < cur_len; i++) {
            canvas_draw_disc(canvas, dot_start + (i * dot_gap), 16, 2);
        }
    }

    for(uint8_t row = 0; row < PIN_GRID_ROWS; row++) {
        for(uint8_t col = 0; col < PIN_GRID_COLS; col++) {
            int16_t box_x = PIN_GRID_START_X + (col * (PIN_KEY_WIDTH + PIN_X_GAP));
            if(col == 3) box_x += PIN_SIDEBAR_GAP_X;
            int16_t box_y = PIN_GRID_START_Y + (row * (PIN_KEY_HEIGHT + PIN_Y_GAP));

            bool is_selected = (row == p->selected_row && col == p->selected_col);
            if(is_selected) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rbox(canvas, box_x, box_y, PIN_KEY_WIDTH, PIN_KEY_HEIGHT, 2);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rframe(canvas, box_x, box_y, PIN_KEY_WIDTH, PIN_KEY_HEIGHT, 2);
            }

            if(col < 3) {
                uint8_t idx = (row * 3) + col;
                canvas_set_font(canvas, FontPrimary);
                canvas_draw_str_aligned(
                    canvas,
                    box_x + (PIN_KEY_WIDTH / 2),
                    box_y + (PIN_KEY_HEIGHT / 2) + 1,
                    AlignCenter, AlignCenter, pin_digit_text[idx]);
            } else {
                int16_t text_x = box_x + (PIN_KEY_WIDTH / 2);
                int16_t text_y = box_y + (PIN_KEY_HEIGHT / 2) + 1;

                if(row == 0) {
                    // Back/delete glyph
                    int16_t x = box_x + 6;
                    int16_t y = box_y + 1;
                    canvas_draw_line(canvas, x, y + 4, x + 4, y);
                    canvas_draw_line(canvas, x, y + 4, x + 4, y + 8);
                    canvas_draw_line(canvas, x + 4, y, x + 12, y);
                    canvas_draw_line(canvas, x + 4, y + 8, x + 12, y + 8);
                    canvas_draw_line(canvas, x + 12, y, x + 12, y + 8);
                    canvas_draw_line(canvas, x + 6, y + 2, x + 10, y + 6);
                    canvas_draw_line(canvas, x + 10, y + 2, x + 6, y + 6);
                } else if(row == 1) {
                    canvas_set_font(canvas, FontPrimary);
                    canvas_draw_str_aligned(canvas, text_x, text_y, AlignCenter, AlignCenter, "0");
                } else {
                    canvas_set_font(canvas, FontKeyboard);
                    canvas_draw_str_aligned(canvas, text_x, text_y, AlignCenter, AlignCenter, "OK");
                }
            }
            canvas_set_color(canvas, ColorBlack);
        }
    }
}

// ===========================================================
// MAIN VIEW DRAW CALLBACK
// ===========================================================

static void fox_setup_draw_main(Canvas* canvas, void* ctx) {
    // Flipper's view system always passes view->model to the draw callback,
    // NOT view->context. We store the app pointer in the model so we can
    // retrieve it here without a NULL dereference.
    FoxSetupApp* app = *(FoxSetupApp**)ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    FoxSetupModel* model = app->model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    switch(model->current_page) {
        case Page1_Welcome:
            canvas_draw_icon(canvas, 0, 0, &I_fox_64x64);
            canvas_set_font(canvas, FontPrimary);
            elements_multiline_text_aligned(
                canvas, 96, 28, AlignCenter, AlignCenter, "Welcome\nto\nFoxFW v2.0");
            elements_button_right(canvas, "Next");
            break;

        case Page2_RenamePrompt: {
            canvas_set_font(canvas, FontPrimary);
            char greet[48];
            snprintf(
                greet, sizeof(greet), "Hi, %s!",
                furi_hal_version_get_name_ptr() ? furi_hal_version_get_name_ptr() : "Flipper");
            canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, greet);
            canvas_set_font(canvas, FontSecondary);
            elements_multiline_text_aligned(
                canvas, 64, 36, AlignCenter, AlignCenter,
                "Want to rename\nyour Flipper?");
            elements_button_left(canvas, "Skip");
            elements_button_right(canvas, "Rename");
            break;
        }

        case Page3_RenameAction:
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Loading...");
            break;

        case Page4_PinPrompt:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignCenter, "PIN SECURITY");
            canvas_set_font(canvas, FontSecondary);
            elements_multiline_text_aligned(
                canvas, 64, 38, AlignCenter, AlignCenter,
                "Protect your flipper with\na PIN? (RECOMMENDED!)");
            elements_button_left(canvas, "Skip");
            elements_button_right(canvas, "Next");
            break;

        case Page5_PinAction:
            fox_setup_draw_pin(canvas, model);
            break;

        case Page6_Complete:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Setup Complete!");
            canvas_set_font(canvas, FontSecondary);
            if(model->name_changed) {
                /* Name change requires namechanger_srv on next boot — restart needed */
                canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter,
                                        "Flipper will restart");
                canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignCenter,
                                        "to apply your changes.");
            } else {
                /* PIN changes are applied at next startup automatically — no restart msg */
                canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignCenter,
                                        "Please join our Discord!");
            }
            elements_button_right(canvas, "Finish");
            break;
    }

    furi_mutex_release(app->mutex);
}

// ===========================================================
// TEXT INPUT CALLBACK
// ===========================================================

static void fox_setup_text_input_done(void* ctx) {
    FoxSetupApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, EventRenameOk);
}

// ===========================================================
// MAIN VIEW INPUT CALLBACK
// ===========================================================

static bool fox_setup_input_main(InputEvent* event, void* ctx) {
    FoxSetupApp* app = ctx;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    FoxSetupModel* model = app->model;

    // Debounce stale input. When auto-launched at boot, the loader's app-load
    // transition can leave buffered input events sitting in the queue (e.g.
    // residual presses from whatever was on screen the instant before, or
    // events generated during the transition itself). Those get delivered the
    // moment this callback registers, causing the spotty first-press behaviour
    // seen only on auto-launch (never when launched cleanly from the Apps menu).
    // Simply discard anything that arrives before the window closes.
    if(model->ignore_input_until && furi_get_tick() < model->ignore_input_until) {
        furi_mutex_release(app->mutex);
        return true;
    }
    model->ignore_input_until = 0;

    InputKey key = event->key;
    bool consumed = true;

    switch(model->current_page) {
        case Page1_Welcome:
            if(key == InputKeyBack) {
                furi_mutex_release(app->mutex);
                view_dispatcher_stop(app->view_dispatcher);
                return true;
            } else if(key == InputKeyOk || key == InputKeyRight) {
                model->current_page = Page2_RenamePrompt;
            }
            break;

        case Page2_RenamePrompt:
            if(key == InputKeyBack) {
                model->current_page = Page1_Welcome;
            } else if(key == InputKeyLeft) {
                model->current_page = fox_post_rename_page();
            } else if(key == InputKeyOk || key == InputKeyRight) {
                model->current_page = Page3_RenameAction;
                const char* cur = furi_hal_version_get_name_ptr();
                strlcpy(model->device_name, cur ? cur : "", sizeof(model->device_name));

                furi_mutex_release(app->mutex);

                text_input_reset(app->text_input);
                text_input_set_header_text(app->text_input, "Flipper Name:");
                text_input_set_result_callback(
                    app->text_input,
                    fox_setup_text_input_done,
                    app,
                    model->device_name,
                    FURI_HAL_VERSION_ARRAY_NAME_LENGTH,
                    true);
                view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdTextInput);
                return true;
            }
            break;

        case Page3_RenameAction:
            break;

        case Page4_PinPrompt:
            if(key == InputKeyBack) {
                /* Back goes to the rename page */
                model->current_page = Page2_RenamePrompt;
            } else if(key == InputKeyLeft) {
                /* Skip — bypass PIN setup entirely and go to Done */
                model->current_page = Page6_Complete;
            } else if(key == InputKeyOk || key == InputKeyRight) {
                PinState* p = &model->pin;
                memset(p, 0, sizeof(PinState));
                model->current_page = Page5_PinAction;
            }
            break;

        case Page5_PinAction: {
            PinState* p = &model->pin;
            uint8_t* cur_len = p->confirming ? &p->confirm_len : &p->first_len;
            char*    cur_buf = p->confirming ? p->confirm   : p->first;

            if(p->mismatch) {
                p->mismatch = false;
                p->confirming = false;
                p->first_len = 0;
                p->confirm_len = 0;
                p->selected_row = 0;
                p->selected_col = 0;
                memset(p->first, 0, sizeof(p->first));
                memset(p->confirm, 0, sizeof(p->confirm));
                break;
            }

            if(key == InputKeyUp) {
                p->selected_row = (p->selected_row + PIN_GRID_ROWS - 1) % PIN_GRID_ROWS;
            } else if(key == InputKeyDown) {
                p->selected_row = (p->selected_row + 1) % PIN_GRID_ROWS;
            } else if(key == InputKeyLeft) {
                p->selected_col = (p->selected_col + PIN_GRID_COLS - 1) % PIN_GRID_COLS;
            } else if(key == InputKeyRight) {
                p->selected_col = (p->selected_col + 1) % PIN_GRID_COLS;
            } else if(key == InputKeyOk) {
                if(p->selected_col < 3) {
                    // Digit cell (1-9)
                    uint8_t idx = (p->selected_row * 3) + p->selected_col;
                    if(*cur_len < PIN_MAX_LEN) {
                        cur_buf[*cur_len] = (char)('1' + idx);
                        (*cur_len)++;
                        cur_buf[*cur_len] = 0;
                    }
                } else if(p->selected_row == 0) {
                    // Back/delete cell
                    if(*cur_len > 0) {
                        (*cur_len)--;
                        cur_buf[*cur_len] = 0;
                    } else if(p->confirming) {
                        p->confirming = false;
                        p->confirm_len = 0;
                        memset(p->confirm, 0, sizeof(p->confirm));
                    } else {
                        model->current_page = Page4_PinPrompt;
                    }
                } else if(p->selected_row == 1) {
                    // "0" cell
                    if(*cur_len < PIN_MAX_LEN) {
                        cur_buf[*cur_len] = '0';
                        (*cur_len)++;
                        cur_buf[*cur_len] = 0;
                    }
                } else {
                    // Done cell
                    if(*cur_len >= PIN_MIN_LEN) {
                        if(!p->confirming) {
                            p->confirming = true;
                            p->selected_row = 0;
                            p->selected_col = 0;
                        } else {
                            if(p->first_len == p->confirm_len &&
                               memcmp(p->first, p->confirm, p->first_len) == 0) {
                                fox_setup_write_pending_pin(p->first, p->first_len);
                                model->pin_changed = true;
                                model->current_page = Page6_Complete;
                            } else {
                                p->mismatch = true;
                            }
                        }
                    }
                }
            } else if(key == InputKeyBack) {
                // Physical Back button mirrors the on-grid Back cell
                if(*cur_len > 0) {
                    (*cur_len)--;
                    cur_buf[*cur_len] = 0;
                } else if(p->confirming) {
                    p->confirming = false;
                    p->confirm_len = 0;
                    memset(p->confirm, 0, sizeof(p->confirm));
                } else {
                    model->current_page = Page4_PinPrompt;
                }
            }
            break;
        }

        case Page6_Complete:
            if(key == InputKeyBack || key == InputKeyOk || key == InputKeyRight) {
                model->completed = true;
                furi_mutex_release(app->mutex);
                view_dispatcher_stop(app->view_dispatcher);
                return true;
            }
            break;

        default:
            consumed = false;
            break;
    }

    furi_mutex_release(app->mutex);
    // Force a redraw. Our app uses a direct model pointer + mutex rather than
    // with_view_model(), which means model changes don't automatically schedule
    // a repaint. Re-switching to the same view is the lightest way to do it.
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMain);
    return consumed;
}

// ===========================================================
// CUSTOM EVENT / NAV CALLBACKS
// ===========================================================

static bool fox_setup_custom_event(void* ctx, uint32_t event) {
    FoxSetupApp* app = ctx;

    if(event == EventRenameOk) {
        // Copy name out under mutex before doing file I/O
        char name_copy[FURI_HAL_VERSION_ARRAY_NAME_LENGTH];
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        strlcpy(name_copy, app->model->device_name, sizeof(name_copy));
        furi_mutex_release(app->mutex);

        if(strlen(name_copy) > 0) {
            Storage* st = (Storage*)furi_record_open(RECORD_STORAGE);
            File* nf = storage_file_alloc(st);
            storage_simply_mkdir(st, FOX_SETUP_FLAG_DIR);
            if(storage_file_open(nf, FOX_SETUP_PENDING_NAME_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                storage_file_write(nf, name_copy, strlen(name_copy) + 1);
                storage_file_close(nf);
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->model->name_changed = true;
                furi_mutex_release(app->mutex);
            }
            storage_file_free(nf);
            furi_record_close(RECORD_STORAGE);
        }

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->model->current_page = fox_post_rename_page();
        furi_mutex_release(app->mutex);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMain);
    }
    return true;
}

static bool fox_setup_nav_event(void* ctx) {
    FoxSetupApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->model->current_page = Page2_RenamePrompt;
    furi_mutex_release(app->mutex);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMain);
    return true;
}

// ===========================================================
// MAIN ENTRY POINT
// ===========================================================

int32_t fox_setup_app(void* p) {
    bool auto_launch  = (p != NULL) && (strcmp((const char*)p, FOX_SETUP_AUTO_ARG)  == 0);
    bool wiper_launch = (p != NULL) && (strcmp((const char*)p, FOX_SETUP_WIPER_ARG) == 0);

    if(auto_launch && fox_setup_already_ran()) return 0;

    FoxSetupApp* app = malloc(sizeof(FoxSetupApp));
    app->model = malloc(sizeof(FoxSetupModel));
    memset(app->model, 0, sizeof(FoxSetupModel));
    app->model->current_page = Page1_Welcome;
    app->model->wiper_launch = wiper_launch;
    if(auto_launch) {
        // 600ms is enough to flush any input events queued during the loader's
        // transition without making manual interaction feel sluggish.
        app->model->ignore_input_until = furi_get_tick() + furi_ms_to_ticks(600);
    }
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->view_dispatcher = view_dispatcher_alloc();
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, fox_setup_custom_event);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, fox_setup_nav_event);

    app->main_view = view_alloc();
    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, fox_setup_draw_main);
    view_set_input_callback(app->main_view, fox_setup_input_main);

    // Allocate a minimal model that holds only the app pointer.
    // Flipper's view system passes view->model (not view->context) to the draw
    // callback, so without this the callback receives NULL and crashes.
    // ViewModelTypeLockFree is correct here — the pointer itself never changes
    // after init, and all mutable state is protected by app->mutex as before.
    view_allocate_model(app->main_view, ViewModelTypeLockFree, sizeof(FoxSetupApp*));
    FoxSetupApp** model_slot = (FoxSetupApp**)view_get_model(app->main_view);
    *model_slot = app;
    view_commit_model(app->main_view, false);

    view_dispatcher_add_view(app->view_dispatcher, ViewIdMain, app->main_view);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdTextInput, text_input_get_view(app->text_input));

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMain);
    view_dispatcher_run(app->view_dispatcher);

    bool write_flag    = app->model->completed && !wiper_launch;
    /* Only name changes need an immediate reboot (so namechanger_srv can
     * apply the new name on the next boot).  PIN changes are applied
     * automatically by desktop_srv at startup — no deliberate reboot needed.
     * For auto_launch the desktop's slideshow handles the reboot; for
     * manual launch fox_setup reboots itself.                             */
    bool should_reboot = app->model->completed &&
                         app->model->name_changed &&
                         !wiper_launch &&
                         !auto_launch;

    view_dispatcher_remove_view(app->view_dispatcher, ViewIdMain);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdTextInput);
    view_free(app->main_view);
    text_input_free(app->text_input);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_mutex_free(app->mutex);
    free(app->model);
    free(app);

    if(write_flag) fox_setup_write_flag();
    if(should_reboot) furi_hal_power_reset();

    return 0;
}
