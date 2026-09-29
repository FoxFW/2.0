#pragma once

#include "app.h"
#include <gui/view.h>

View* flpr_view_alloc(App* app);
void flpr_view_free(View* view);
