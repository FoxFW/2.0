#ifndef FMPS_CXT_H
#define FMPS_CXT_H

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>

// Pages
#include "pages/main_page.h"
#include "pages/operate_page.h"
#include "pages/read_hitag2_page.h"

typedef enum {
    // The main page of the app, showing the connection status and some basic info
    MainPageViewId,
    // The operate page, showing operation submenu
    OperatePageViewId,
    // Read Hitag2 operation page
    ReadHitag2PageViewId,
} Proxmark5ViewId;

typedef enum {
    Proxmark5CustomEventOpenOperatePage,
    Proxmark5CustomEventOpenReadHitag2Page,
} Proxmark5CustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    // Pages
    MainPage* main_page;
    OperatePage* operate_page;
    ReadHitag2Page* read_hitag2_page;
} Proxmark5App;

#endif // FMPS_CXT_H
