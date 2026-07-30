#pragma once

#include "app.h"

void scripts_render_menu(App* app);
void scripts_menu_select(App* app, uint32_t index);

void scripts_actions_render_menu(App* app);
void scripts_actions_select(App* app, uint32_t index);

void scripts_name_submitted(App* app);
void scripts_source_submitted(App* app);
