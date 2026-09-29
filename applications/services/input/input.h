#pragma once

#include <furi_hal_resources.h>
#include "input_settings.h"
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_INPUT_EVENTS            "input_events"
#define RECORD_INPUT_SETTINGS          "input_settings"
#define INPUT_SEQUENCE_SOURCE_HARDWARE (0u)
#define INPUT_SEQUENCE_SOURCE_SOFTWARE (1u)

typedef enum {
    InputTypePress,
    InputTypeRelease,
    InputTypeShort,
    InputTypeLong,
    InputTypeRepeat,
    InputTypeMAX,
} InputType;

typedef struct {
    union {
        uint32_t sequence;
        struct {
            uint8_t sequence_source   : 2;
            uint32_t sequence_counter : 30;
        };
    };
    InputKey key;
    InputType type;
} InputEvent;

const char* input_get_key_name(InputKey key);

const char* input_get_type_name(InputType type);

#ifdef __cplusplus
}
#endif
