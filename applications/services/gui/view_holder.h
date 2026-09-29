#pragma once

#include <gui/view.h>
#include <gui/gui.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ViewHolder ViewHolder;

typedef void (*FreeCallback)(void* free_context);

typedef void (*BackCallback)(void* back_context);

ViewHolder* view_holder_alloc(void);

void view_holder_free(ViewHolder* view_holder);

void view_holder_set_view(ViewHolder* view_holder, View* view);

void view_holder_set_free_callback(
    ViewHolder* view_holder,
    FreeCallback free_callback,
    void* free_context);

void* view_holder_get_free_context(ViewHolder* view_holder);

void view_holder_set_back_callback(
    ViewHolder* view_holder,
    BackCallback back_callback,
    void* back_context);

void view_holder_attach_to_gui(ViewHolder* view_holder, Gui* gui);

void view_holder_update(View* view, void* context);

void view_holder_send_to_front(ViewHolder* view_holder);

void view_holder_send_to_back(ViewHolder* view_holder);

#ifdef __cplusplus
}
#endif
