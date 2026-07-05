#include "gdr_psa_bf_host.h"

bool gdr_psa_bf_plugin_ensure_loaded(GDRApp* app) {
    (void)app;
    return false;
}

void gdr_psa_bf_plugin_unload_if_idle(GDRApp* app) {
    (void)app;
}

void gdr_psa_bf_context_release(GDRApp* app) {
    (void)app;
}
