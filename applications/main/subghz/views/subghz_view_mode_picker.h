#pragma once
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SubGhzModePicker SubGhzModePicker;

typedef void (*SubGhzModePickerCallback)(void* context, uint32_t index);

#define SUBGHZ_MODE_PICKER_AUTOMOTIVE     0
#define SUBGHZ_MODE_PICKER_GARAGE         1
#define SUBGHZ_MODE_PICKER_JAMMER         2
#define SUBGHZ_MODE_PICKER_BRUTEFORCER    3
#define SUBGHZ_MODE_PICKER_TPMS           4
#define SUBGHZ_MODE_PICKER_RADIO_SETTINGS 5

SubGhzModePicker* subghz_mode_picker_alloc(void);
void subghz_mode_picker_free(SubGhzModePicker* instance);
View* subghz_mode_picker_get_view(SubGhzModePicker* instance);
void subghz_mode_picker_set_callback(
    SubGhzModePicker* instance,
    SubGhzModePickerCallback callback,
    void* context);
void subghz_mode_picker_set_selected(SubGhzModePicker* instance, uint8_t index);

#ifdef __cplusplus
}
#endif
