#pragma once

#include <stdbool.h>

typedef struct GDRApp GDRApp;

bool gdr_psa_bf_plugin_ensure_loaded(GDRApp* app);
void gdr_psa_bf_plugin_unload_if_idle(GDRApp* app);
void gdr_psa_bf_context_release(GDRApp* app);

void gdr_receiver_info_rebuild_normal_widget(void* app);

#ifdef ENABLE_SUB_DECODE_SCENE
void gdr_subdecode_psa_bf_complete_refresh(void* app);
#endif
