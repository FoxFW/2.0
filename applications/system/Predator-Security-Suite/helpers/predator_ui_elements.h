#pragma once

#include <furi.h>
#include <gui/canvas.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>

// Progress bar styles
typedef enum {
    ProgressBarStyleNormal,    // Default style
    ProgressBarStyleBordered,  // With border
    ProgressBarStyleDashed,    // Dashed/segmented progress
    ProgressBarStyleAnimated   // Animated/pulsing effect for indeterminate progress
} ProgressBarStyle;

// SubMenu indices for RFID attacks
typedef enum {
    SubmenuIndexRfidClone,
    SubmenuIndexRfidBruteforce,
    SubmenuIndexRfidFuzzing,
    SubmenuIndexNfcClone,
    SubmenuIndexMifareHack,
} RfidSubMenuIndices;

// SubMenu indices for car attacks
typedef enum {
    SubmenuIndexCarModels,
    SubmenuIndexCarPassiveOpener,
    SubmenuIndexCarKeyBruteforce,
    SubmenuIndexCarJamming,
    SubmenuIndexCarTesla,
    SubmenuIndexCarRollingCode,
    SubmenuIndexCarTireMonitor,
} CarSubMenuIndices;

// SubMenu indices for WiFi attacks
typedef enum {
    SubmenuIndexWifiScan,
    SubmenuIndexWifiDeauth,
    SubmenuIndexWifiEvilTwin,
    SubmenuIndexWifiHandshakeCapture,
    SubmenuIndexWifiPwnagotchi,
} WifiSubMenuIndices;

// SubMenu indices for BLE attacks
typedef enum {
    SubmenuIndexBleScan,
    SubmenuIndexBleSpam,
} BleSubMenuIndices;

// Shared UI element drawing functions
void predator_ui_draw_progress_bar(
    Canvas* canvas,
    uint8_t x, uint8_t y, 
    uint8_t width, uint8_t height,
    uint8_t progress, // 0-100
    ProgressBarStyle style);

// Draw bordered box with title
void predator_ui_draw_status_box(
    Canvas* canvas, 
    const char* title, 
    uint8_t x, uint8_t y, 
    uint8_t width, uint8_t height);

// Draw styled text with header
void predator_ui_draw_header_text(
    Canvas* canvas,
    const char* header,
    const char* text,
    uint8_t x, uint8_t y);

// Lightweight vector arrows (6x7) to replace firmware icons in external apps
void predator_ui_draw_arrow_up(Canvas* canvas, uint8_t x, uint8_t y);
void predator_ui_draw_arrow_down(Canvas* canvas, uint8_t x, uint8_t y);
void predator_ui_draw_arrow_left(Canvas* canvas, uint8_t x, uint8_t y);
void predator_ui_draw_arrow_right(Canvas* canvas, uint8_t x, uint8_t y);

// Unified scroll helpers with subtle animation
void predator_ui_draw_scroll_vertical(
    Canvas* canvas,
    uint8_t x,
    uint8_t y_top,
    uint8_t y_bottom,
    bool can_up,
    bool can_down);

void predator_ui_draw_nav_hints_lr(
    Canvas* canvas,
    uint8_t y,
    bool show_left,
    bool show_right);

// Slim scrollbar for lists
void predator_ui_draw_scrollbar(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t height,
    uint8_t total_items,
    uint8_t first_visible,
    uint8_t visible_items);


// Helper for sin/cos calculations in animations
extern const uint8_t sin_array[256];
extern const uint8_t cos_array[256];
