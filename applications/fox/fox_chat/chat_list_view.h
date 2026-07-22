#pragma once

#include "app.h"

/* Backs FoxCommanderViewChatList - the Read Messages results screen.
   Styled after fox_file_browser's Favourites menu (big 2-line rounded
   -box rows, 2 visible at once, rounded-box selection, side scrollbar)
   rather than a plain Submenu, per the user's reference for what this
   should look like. Unlike Favourites, a row here does nothing when
   scrolled past - selecting one and pressing OK/Right opens
   chat_detail_view.c to show that message's full text and the time it
   was sent. */

/* Longest single wrapped line chat_wrap_lines() below will ever
   produce - generous relative to FOX_CHAT_MESSAGE_TEXT_MAX since a
   wrapped line is always a fragment of one message's text, never the
   whole thing. */
#define CHAT_WRAP_LINE_MAX 48

/* Word-wraps `text` into up to out_capacity lines no wider than max_w
   pixels (using canvas's current font), breaking at spaces where
   possible. If more text remains once out_capacity lines are filled,
   the last line is cut short and "..." appended so truncation is
   always visible rather than silently dropping text. Returns the
   number of lines actually produced (<= out_capacity). Defined here,
   used by this file's own row rendering and by chat_detail_view.c
   (which includes this header) for its full multi-line body. */
size_t chat_wrap_lines(
    Canvas* canvas,
    const char* text,
    int max_w,
    char out_lines[][CHAT_WRAP_LINE_MAX],
    size_t out_capacity);

/* A message's text is usually "<device_name>: <content>" for anything
   posted through this bridge (see foxchat_menu.c's
   chat_message_submitted()) - the Discord-level author username is no
   longer sent by the firmware at all, since it was always just this
   bot's own fixed account name (see discord.cpp's doRead()). This
   looks for that embedded "name: " prefix so the draw callbacks below
   can render it in a bold font, distinct from the rest of the message.
   Returns true and sets *out_name_len to the prefix length (not
   counting the colon) if one was found; returns false (leaving
   *out_name_len untouched) for plain text with no such prefix, e.g. a
   message someone posted directly in Discord rather than through a
   Flipper. A sane upper bound on name length (FOX_DEVICE_NAME_MAX)
   keeps an ordinary "note: don't forget X"-style message from
   spuriously getting its first word bolded - Flipper device names are
   always much shorter than that. */
#define FOX_DEVICE_NAME_MAX 32
bool chat_find_username_split(const char* text, size_t* out_name_len);

View* chat_list_view_alloc(App* app);
void chat_list_view_free(View* view);
/* Selects+scrolls to the newest message (the last entry in
   app->chat_messages, since the firmware sends oldest-first) so the
   list opens already scrolled to the bottom, like a normal chat, then
   switches to it. Call this after populating app->chat_messages /
   chat_message_count, not before. */
void chat_list_view_show(App* app);
