#pragma once

#include <gui/view.h>
#include <datetime.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DateTimeInput DateTimeInput;

typedef void (*DateTimeChangedCallback)(void* context);

typedef void (*DateTimeDoneCallback)(void* context);

DateTimeInput* date_time_input_alloc(void);

void date_time_input_free(DateTimeInput* date_time_input);

View* date_time_input_get_view(DateTimeInput* date_time_input);

void date_time_input_set_result_callback(
    DateTimeInput* date_time_input,
    DateTimeChangedCallback changed_callback,
    DateTimeDoneCallback done_callback,
    void* callback_context,
    DateTime* datetime);

void date_time_input_set_editable_fields(
    DateTimeInput* date_time_input,
    bool year,
    bool month,
    bool day,
    bool hour,
    bool minute,
    bool second);

#ifdef __cplusplus
}
#endif
