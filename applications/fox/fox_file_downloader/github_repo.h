#pragma once

#include "app.h"

void github_repo_open(App* app);
void github_repo_submitted(App* app);
void github_repo_info_loaded(App* app);

void github_show_file_list(App* app);
void github_free_buffers(App* app);

View* github_file_list_view_alloc(App* app);
void github_file_list_view_free(View* v);
