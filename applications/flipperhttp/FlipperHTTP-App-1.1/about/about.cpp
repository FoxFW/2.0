#include "about/about.hpp"

FlipperHTTPAbout::FlipperHTTPAbout(ViewDispatcher **viewDispatcher) : widget(nullptr), viewDispatcherRef(viewDispatcher)
{
    easy_flipper_set_widget(&widget, FlipperHTTPViewAbout, "FlipperHTTP companion app\n\n\n\n\nwww.github.com/jblanked", callbackToSubmenu, viewDispatcherRef);
}

FlipperHTTPAbout::~FlipperHTTPAbout()
{
    if (widget && viewDispatcherRef && *viewDispatcherRef)
    {
        view_dispatcher_remove_view(*viewDispatcherRef, FlipperHTTPViewAbout);
        widget_free(widget);
        widget = nullptr;
    }
}

uint32_t FlipperHTTPAbout::callbackToSubmenu(void *context)
{
    UNUSED(context);
    return FlipperHTTPViewSubmenu;
}
