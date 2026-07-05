/**
 * @file gui.h
 * GUI: main API
 */

#pragma once

#include "view_port.h"
#include "canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Gui layers */
typedef enum {
    GuiLayerDesktop, /**< Desktop layer for internal use. Like fullscreen but with status bar */

    GuiLayerWindow, /**< Window layer, status bar is shown */

    GuiLayerStatusBarLeft,   /**< Status bar left-side layer, auto-layout */
    GuiLayerStatusBarRight,  /**< Status bar right-side layer, auto-layout */
    GuiLayerStatusBarCenter, /**< Status bar center layer, fixed center position */

    GuiLayerFullscreen, /**< Fullscreen layer, no status bar */

    GuiLayerMAX /**< Don't use or move, special value */
} GuiLayer;

/** Gui Canvas Commit Callback */
typedef void (*GuiCanvasCommitCallback)(
    uint8_t* data,
    size_t size,
    CanvasOrientation orientation,
    void* context);

#define RECORD_GUI "gui"

typedef struct Gui Gui;

void gui_add_view_port(Gui* gui, ViewPort* view_port, GuiLayer layer);
void gui_remove_view_port(Gui* gui, ViewPort* view_port);
void gui_view_port_send_to_front(Gui* gui, ViewPort* view_port);
void gui_view_port_send_to_back(Gui* gui, ViewPort* view_port);
void gui_add_framebuffer_callback(Gui* gui, GuiCanvasCommitCallback callback, void* context);
void gui_remove_framebuffer_callback(Gui* gui, GuiCanvasCommitCallback callback, void* context);
size_t gui_get_framebuffer_size(const Gui* gui);
void gui_set_lockdown(Gui* gui, bool lockdown);
void gui_set_lockdown_inhibit(Gui* gui, bool inhibit);
bool gui_is_lockdown(const Gui* gui);
void gui_set_hide_status_bar(Gui* gui, bool hide);
Canvas* gui_direct_draw_acquire(Gui* gui);
void gui_direct_draw_release(Gui* gui);

#ifdef __cplusplus
}
#endif
