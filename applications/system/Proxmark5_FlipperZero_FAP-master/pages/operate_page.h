#ifndef OPERATE_PAGE_H
#define OPERATE_PAGE_H

#include <gui/modules/submenu.h>

typedef void (*OperatePageReadHitag2Callback)(void* context);

typedef struct {
    Submenu* submenu;

    OperatePageReadHitag2Callback open_read_hitag2_callback;
    void* open_read_hitag2_callback_context;
} OperatePage;

OperatePage* operate_page_create(
    OperatePageReadHitag2Callback open_read_hitag2_callback,
    void* open_read_hitag2_callback_context);
void operate_page_free(OperatePage* operate_page);

#endif // OPERATE_PAGE_H
