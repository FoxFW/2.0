#pragma once

#include "app.h"
#include <gui/view.h>

/* "FoxLAB Active" - shown once [LAB/START] succeeds (see launcher_view.c's
 * launcher_toggle()). The FLPR companion (foxr_companion.h) answers
 * commands regardless of which view is showing - this screen exists
 * purely so the user has a clear "this is what's keeping the Flipper tab
 * alive" signal, with a small live log of the most recent commands it's
 * handled. Back returns to the Launcher. */

View* flpr_view_alloc(App* app);
void flpr_view_free(View* view);
