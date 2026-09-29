#pragma once

#include <input/input.h>

#include "icon_animation.h"
#include "canvas.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VIEW_NONE 0xFFFFFFFF

#define VIEW_IGNORE 0xFFFFFFFE

typedef enum {
    ViewOrientationHorizontal,
    ViewOrientationHorizontalFlip,
    ViewOrientationVertical,
    ViewOrientationVerticalFlip,
} ViewOrientation;

typedef struct View View;

typedef void (*ViewDrawCallback)(Canvas* canvas, void* model);

typedef bool (*ViewInputCallback)(InputEvent* event, void* context);

typedef bool (*ViewCustomCallback)(uint32_t event, void* context);

typedef uint32_t (*ViewNavigationCallback)(void* context);

typedef void (*ViewCallback)(void* context);

typedef void (*ViewUpdateCallback)(View* view, void* context);

typedef enum {

    ViewModelTypeNone,

    ViewModelTypeLockFree,

    ViewModelTypeLocking,
} ViewModelType;

View* view_alloc(void);

void view_free(View* view);

void view_tie_icon_animation(View* view, IconAnimation* icon_animation);

void view_set_draw_callback(View* view, ViewDrawCallback callback);

void view_set_input_callback(View* view, ViewInputCallback callback);

void view_set_custom_callback(View* view, ViewCustomCallback callback);

void view_set_previous_callback(View* view, ViewNavigationCallback callback);

void view_set_enter_callback(View* view, ViewCallback callback);

void view_set_exit_callback(View* view, ViewCallback callback);

void view_set_update_callback(View* view, ViewUpdateCallback callback);

void view_set_update_callback_context(View* view, void* context);

void view_set_context(View* view, void* context);

void view_set_orientation(View* view, ViewOrientation orientation);

void view_allocate_model(View* view, ViewModelType type, size_t size);

void view_free_model(View* view);

void* view_get_model(View* view);

void view_commit_model(View* view, bool update);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#define with_view_model_cpp(view, type, var, code, update)  \
    {                                                       \
        type var = static_cast<type>(view_get_model(view)); \
        {code};                                             \
        view_commit_model(view, update);                    \
    }
#else

#define with_view_model(view, type, code, update) \
    {                                             \
        type = view_get_model(view);              \
        {code};                                   \
        view_commit_model(view, update);          \
    }
#endif
