#pragma once

#include <gui/view.h>
#include "validators.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TextInput TextInput;
typedef void (*TextInputCallback)(void* context);
typedef bool (*TextInputValidatorCallback)(const char* text, FuriString* error, void* context);

TextInput* text_input_alloc(void);

void text_input_free(TextInput* text_input);

void text_input_reset(TextInput* text_input);

View* text_input_get_view(TextInput* text_input);

void text_input_set_result_callback(
    TextInput* text_input,
    TextInputCallback callback,
    void* callback_context,
    char* text_buffer,
    size_t text_buffer_size,
    bool clear_default_text);

void text_input_set_minimum_length(TextInput* text_input, size_t minimum_length);

void text_input_set_validator(
    TextInput* text_input,
    TextInputValidatorCallback callback,
    void* callback_context);

TextInputValidatorCallback text_input_get_validator_callback(TextInput* text_input);

void* text_input_get_validator_callback_context(TextInput* text_input);

void text_input_set_header_text(TextInput* text_input, const char* text);

#ifdef __cplusplus
}
#endif
