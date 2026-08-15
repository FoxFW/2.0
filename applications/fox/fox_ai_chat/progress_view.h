#pragma once

#include "app.h"

View* progress_view_alloc(App* app);
void progress_view_free(View* view);

void progress_view_show(App* app, ProgressStage stage);
void progress_view_set_stage(App* app, ProgressStage stage);
