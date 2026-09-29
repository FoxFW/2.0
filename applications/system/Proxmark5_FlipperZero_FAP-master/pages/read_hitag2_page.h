#ifndef READ_HITAG2_PAGE_H
#define READ_HITAG2_PAGE_H

#include <gui/view.h>

typedef struct ReadHitag2Page ReadHitag2Page;

// Invoked when the user leaves the page via the on-screen "Back" (left) button.
typedef void (*ReadHitag2PageBackCallback)(void* context);

ReadHitag2Page*
    read_hitag2_page_create(ReadHitag2PageBackCallback back_callback, void* back_callback_context);
void read_hitag2_page_free(ReadHitag2Page* read_hitag2_page);

View* read_hitag2_page_get_view(ReadHitag2Page* read_hitag2_page);

void read_hitag2_page_start(ReadHitag2Page* read_hitag2_page);
void read_hitag2_page_stop(ReadHitag2Page* read_hitag2_page);

#endif // READ_HITAG2_PAGE_H
