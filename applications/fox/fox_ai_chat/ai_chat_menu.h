#pragma once

#include "app.h"

void ai_chat_render_menu(App* app);
void ai_chat_menu_select(App* app, uint32_t index);
void ai_chat_show_terminal(App* app);

void chat_message_submitted(App* app);
