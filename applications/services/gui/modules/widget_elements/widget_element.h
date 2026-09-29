#pragma once

#include <input/input.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GuiButtonTypeLeft,
    GuiButtonTypeCenter,
    GuiButtonTypeRight,
} GuiButtonType;

typedef void (*ButtonCallback)(GuiButtonType result, InputType type, void* context);

#ifdef __cplusplus
}
#endif
