#pragma once

#include "app.h"

/* Backs FoxCommanderViewChatDetail - the full-message screen reached by
   pressing OK/Right on a row in chat_list_view.c. Shows the complete
   text for app->chat_messages[selected] (word-wrapped, scrollable with
   Up/Down if it runs past the visible area), bolding the leading
   "<device_name>: " prefix if the text has one (see
   chat_list_view.c's chat_find_username_split()), plus a bottom bar
   with the UTC time the message was sent. */

View* chat_detail_view_alloc(App* app);
void chat_detail_view_free(View* view);
