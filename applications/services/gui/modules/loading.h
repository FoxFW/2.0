#pragma once
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Loading Loading;

Loading* loading_alloc(void);

void loading_free(Loading* instance);

View* loading_get_view(Loading* instance);

#ifdef __cplusplus
}
#endif
