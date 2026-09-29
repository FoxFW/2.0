#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Popup Popup;

typedef void (*PopupCallback)(void* context);

Popup* popup_alloc(void);

void popup_free(Popup* popup);

View* popup_get_view(Popup* popup);

void popup_set_callback(Popup* popup, PopupCallback callback);

void popup_set_context(Popup* popup, void* context);

void popup_set_header(
    Popup* popup,
    const char* text,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical);

void popup_set_text(
    Popup* popup,
    const char* text,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical);

void popup_set_icon(Popup* popup, uint8_t x, uint8_t y, const Icon* icon);

void popup_set_timeout(Popup* popup, uint32_t timeout_in_ms);

void popup_enable_timeout(Popup* popup);

void popup_disable_timeout(Popup* popup);

void popup_reset(Popup* popup);

#ifdef __cplusplus
}
#endif
