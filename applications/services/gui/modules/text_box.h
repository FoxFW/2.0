#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TextBox TextBox;

typedef enum {
    TextBoxFontText,
    TextBoxFontHex,
} TextBoxFont;

typedef enum {
    TextBoxFocusStart,
    TextBoxFocusEnd,
} TextBoxFocus;

TextBox* text_box_alloc(void);

void text_box_free(TextBox* text_box);

View* text_box_get_view(TextBox* text_box);

void text_box_reset(TextBox* text_box);

void text_box_set_text(TextBox* text_box, const char* text);

void text_box_set_font(TextBox* text_box, TextBoxFont font);

void text_box_set_focus(TextBox* text_box, TextBoxFocus focus);

#ifdef __cplusplus
}
#endif
