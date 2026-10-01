#pragma once
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LoaderMenu LoaderMenu;

LoaderMenu* loader_menu_alloc(void (*closed_cb)(void*), void* context);

void loader_menu_free(LoaderMenu* loader_menu);

void loader_menu_show(LoaderMenu* loader_menu);

void loader_menu_show_settings(LoaderMenu* loader_menu, const char* settings_item);

const char* loader_menu_take_settings_return(void);

bool loader_menu_settings_return_pending(void);

#ifdef __cplusplus
}
#endif
