#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EmptyScreen EmptyScreen;

EmptyScreen* empty_screen_alloc(void);

void empty_screen_free(EmptyScreen* empty_screen);

View* empty_screen_get_view(EmptyScreen* empty_screen);

#ifdef __cplusplus
}
#endif
