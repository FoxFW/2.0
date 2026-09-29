#pragma once
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TPMSBoxList TPMSBoxList;

typedef void (*TPMSBoxListCallback)(void* context, uint32_t index);

typedef struct {
    const char* title;
    const char* subtitle;
} TPMSBoxListOption;

TPMSBoxList* tpms_box_list_alloc(void);
void tpms_box_list_free(TPMSBoxList* instance);
View* tpms_box_list_get_view(TPMSBoxList* instance);
void tpms_box_list_set_callback(
    TPMSBoxList* instance,
    TPMSBoxListCallback callback,
    void* context);

void tpms_box_list_set_options(
    TPMSBoxList* instance,
    const TPMSBoxListOption* options,
    uint8_t count);

void tpms_box_list_set_selected(TPMSBoxList* instance, uint8_t index);

void tpms_box_list_resume_scroll(TPMSBoxList* instance);
void tpms_box_list_pause_scroll(TPMSBoxList* instance);

#ifdef __cplusplus
}
#endif
