#pragma once

#include "app.h"

/* FoxChat - Post Message / Read Messages, talking to the firmware's
   discord.cpp module (DISCORDINIT/DISCORDPOST/DISCORDREAD) over the
   same line protocol as every other command in this app. Bot token and
   channel ID are auto-provisioned via foxchat_auto_provision() with
   hard-coded seeds; no manual entry needed. */

void foxchat_render_menu(App* app);
void foxchat_menu_select(App* app, uint32_t index);

/* Called from main.c's text_input_result_callback() when the user
   submits a Post Message entry. */
void chat_message_submitted(App* app);
