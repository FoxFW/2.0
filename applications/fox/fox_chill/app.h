#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "fox_splash.h"

#define FOX_CHILL_LINE_MAX 256

typedef enum {
    FoxChillViewSplash,
    FoxChillViewMenu,
    FoxChillViewContent,
    FoxChillViewScrollCounter,
    FoxChillViewBeMindful,
    FoxChillViewLongWait,
    FoxChillViewStats,
} FoxChillView;

typedef enum {
    ContentKindJoke,
    ContentKindRiddle,
    ContentKindFact,
    ContentKindStatistic,
    ContentKindYoMama,
} ContentKind;

typedef struct {
    uint32_t magic;
    uint32_t high_score;
    uint32_t jokes_read;
    uint32_t riddles_read;
    uint32_t facts_read;
    uint32_t statistics_read;
    uint32_t yo_mama_read;
    uint32_t mindful_sessions;
    uint32_t long_waits;
} FoxChillSaveData;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    FuriTimer* mindful_timer;
    FuriTimer* long_wait_timer;

    FoxSplash* splash;

    View* menu_view;
    View* content_view;
    View* scroll_counter_view;
    View* be_mindful_view;
    View* long_wait_view;
    View* stats_view;

    FoxChillView current_view;

    FoxChillSaveData save;

    ContentKind content_kind;
    char content_question[FOX_CHILL_LINE_MAX];
    char content_answer[FOX_CHILL_LINE_MAX];
    bool content_has_answer;
    bool content_answer_shown;
    uint8_t content_scroll;

    uint32_t scroll_counter_value;
    uint32_t scroll_counter_last_tick;
    bool scroll_counter_notified_new_best;

    uint32_t mindful_tick;

    uint32_t long_wait_seconds_left;
} App;

void fox_chill_goto_menu(App* app);

bool fox_chill_pick_random(App* app, ContentKind kind);

void fox_chill_save_load(App* app);
void fox_chill_save_write(App* app);
void fox_chill_save_note_read(App* app, ContentKind kind);
void fox_chill_save_note_score(App* app, uint32_t score);
void fox_chill_save_note_mindful(App* app);
void fox_chill_save_note_long_wait(App* app);

View* menu_view_alloc(App* app);
void menu_view_free(View* view);

View* content_view_alloc(App* app);
void content_view_free(View* view);
void content_view_show(App* app, ContentKind kind);

View* scroll_counter_view_alloc(App* app);
void scroll_counter_view_free(View* view);
void scroll_counter_view_show(App* app);

View* mindful_view_alloc(App* app);
void mindful_view_free(View* view);
void mindful_view_show(App* app);
bool mindful_view_tick(App* app);

View* long_wait_view_alloc(App* app);
void long_wait_view_free(View* view);
void long_wait_view_show(App* app);
bool long_wait_view_tick(App* app);

View* stats_view_alloc(App* app);
void stats_view_free(View* view);

void fox_chill_draw_double_border(Canvas* canvas, int32_t x, int32_t y, int32_t w, int32_t h);
void fox_chill_draw_next_button(Canvas* canvas, const char* label);
void fox_chill_draw_left_pill_button(
    Canvas* canvas,
    const Icon* icon,
    const char* label,
    int32_t x);
void fox_chill_format_commas(uint32_t value, char* out, size_t out_cap);
void fox_chill_draw_big_words(
    Canvas* canvas,
    const Icon* icon1,
    const Icon* icon2,
    int32_t area_y,
    int32_t area_h);
