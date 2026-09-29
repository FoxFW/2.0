#pragma once

#include "thread_list.h"

#ifdef __cplusplus
extern "C" {
#endif

void furi_thread_list_process(FuriThreadList* instance, uint32_t runtime, uint32_t tick);

#ifdef __cplusplus
}
#endif
