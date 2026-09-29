#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NumberInput NumberInput;

typedef void (*NumberInputCallback)(void* context, int32_t number);

NumberInput* number_input_alloc(void);

void number_input_free(NumberInput* number_input);

View* number_input_get_view(NumberInput* number_input);

void number_input_set_result_callback(
    NumberInput* number_input,
    NumberInputCallback input_callback,
    void* callback_context,
    int32_t current_number,
    int32_t min_value,
    int32_t max_value);

void number_input_set_header_text(NumberInput* number_input, const char* text);

#ifdef __cplusplus
}
#endif
