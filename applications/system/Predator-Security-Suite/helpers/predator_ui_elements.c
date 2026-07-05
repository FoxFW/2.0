#include "predator_ui_elements.h"
#include <furi.h>

void predator_ui_draw_progress_bar(
    Canvas* canvas,
    uint8_t x, uint8_t y, 
    uint8_t width, uint8_t height,
    uint8_t progress, // 0-100
    ProgressBarStyle style) {
    
    // Clamp progress to valid range
    if(progress > 100) progress = 100;
    
    // Calculate filled width based on progress
    uint8_t fill_width = (width * progress) / 100;
    
    switch(style) {
    case ProgressBarStyleBordered:
        // Draw border
        canvas_draw_frame(canvas, x, y, width, height);
        // Draw fill (slightly smaller to not overlap border)
        if(fill_width > 2) {
            canvas_draw_box(canvas, x + 1, y + 1, fill_width - 2, height - 2);
        }
        break;
        
    case ProgressBarStyleDashed:
        {
            // Draw border
            canvas_draw_frame(canvas, x, y, width, height);
            
            // Draw dashed segments inside
            uint8_t segment_count = 10; // Number of segments
            uint8_t segment_width = (width - 2) / segment_count;
            uint8_t filled_segments = (segment_count * progress) / 100;
            
            for(uint8_t i = 0; i < filled_segments; i++) {
                canvas_draw_box(
                    canvas,
                    x + 1 + (i * segment_width),
                    y + 1,
                    segment_width - 1,
                    height - 2);
            }
        }
        break;
        
    case ProgressBarStyleAnimated:
        {
            // Draw border
            canvas_draw_frame(canvas, x, y, width, height);
            
            // Get current time for animation
            uint32_t ticks = furi_get_tick();
            
            if(progress < 100) {
                // For less than 100%, show animated pulsing bar
                uint8_t animation_width = width / 3;
                uint8_t animation_pos = (ticks / 100) % (width - animation_width);
                
                // Draw animated portion
                canvas_draw_box(
                    canvas,
                    x + 1 + animation_pos,
                    y + 1,
                    animation_width,
                    height - 2);
            } else {
                // At 100%, fill completely
                canvas_draw_box(canvas, x + 1, y + 1, width - 2, height - 2);
            }
        }
        break;
        
    case ProgressBarStyleNormal:
    default:
        // Simple progress bar with no border
        canvas_draw_box(canvas, x, y, fill_width, height);
        break;
    }
}

void predator_ui_draw_status_box(
    Canvas* canvas, 
    const char* title, 
    uint8_t x, uint8_t y, 
    uint8_t width, uint8_t height) {
    
    // Draw box with rounded corners
    elements_slightly_rounded_frame(canvas, x, y, width, height);
    
    // Get current font
    Font previous_font = FontSecondary;
    canvas_set_font(canvas, FontSecondary);
    
    // Calculate title width
    int16_t title_width = canvas_string_width(canvas, title);
    
    // Draw white background for title to create a floating title effect
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x + 4, y - 4, title_width + 2, 8);
    
    // Draw the title
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, x + 5, y, title);
    
    // Restore font
    canvas_set_font(canvas, previous_font);
}

void predator_ui_draw_header_text(
    Canvas* canvas,
    const char* header,
    const char* text,
    uint8_t x, uint8_t y) {
    
    // Save current font and set to bold font for header
    Font previous_font = FontSecondary;
    
    // Draw header
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, x, y, header);
    
    // Draw separator line under the header
    uint8_t header_width = canvas_string_width(canvas, header);
    canvas_draw_line(canvas, x, y + 2, x + header_width, y + 2);
    
    // Draw text content below header
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x, y + 12, text);
    
    // Restore previous font
    canvas_set_font(canvas, previous_font);
}

// Draw small vector arrows (6x7) so we don't depend on firmware icon exports
// x,y is the top-left corner of a 6x7 cell
void predator_ui_draw_arrow_up(Canvas* canvas, uint8_t x, uint8_t y) {
    // Up chevron: two lines forming a ^ shape
    canvas_draw_line(canvas, x + 0, y + 5, x + 3, y + 2);
    canvas_draw_line(canvas, x + 3, y + 2, x + 6, y + 5);
}

void predator_ui_draw_arrow_down(Canvas* canvas, uint8_t x, uint8_t y) {
    // Down chevron: two lines forming a v shape
    canvas_draw_line(canvas, x + 0, y + 2, x + 3, y + 5);
    canvas_draw_line(canvas, x + 3, y + 5, x + 6, y + 2);
}

void predator_ui_draw_arrow_left(Canvas* canvas, uint8_t x, uint8_t y) {
    // Left chevron: two lines forming a < shape
    canvas_draw_line(canvas, x + 4, y + 0, x + 1, y + 3);
    canvas_draw_line(canvas, x + 1, y + 3, x + 4, y + 6);
}

void predator_ui_draw_arrow_right(Canvas* canvas, uint8_t x, uint8_t y) {
    // Right chevron: two lines forming a > shape
    canvas_draw_line(canvas, x + 2, y + 0, x + 5, y + 3);
    canvas_draw_line(canvas, x + 5, y + 3, x + 2, y + 6);
}

void predator_ui_draw_scroll_vertical(
    Canvas* canvas,
    uint8_t x,
    uint8_t y_top,
    uint8_t y_bottom,
    bool can_up,
    bool can_down) {
    // Draw up/down chevrons with a subtle 2-frame pulse animation
    uint8_t phase = (furi_get_tick() / 250) % 2; // 0..1
    uint8_t offset = phase ? 1 : 0;

    if(can_up) {
        predator_ui_draw_arrow_up(canvas, x, (uint8_t)(y_top - offset));
        predator_ui_draw_arrow_up(canvas, x, (uint8_t)(y_top + 2 - offset));
    }
    if(can_down) {
        predator_ui_draw_arrow_down(canvas, x, (uint8_t)(y_bottom - 2 + offset));
        predator_ui_draw_arrow_down(canvas, x, (uint8_t)(y_bottom + offset));
    }
}

void predator_ui_draw_nav_hints_lr(
    Canvas* canvas,
    uint8_t y,
    bool show_left,
    bool show_right) {
    if(show_left) predator_ui_draw_arrow_left(canvas, 20, (uint8_t)(y - 3));
    if(show_right) predator_ui_draw_arrow_right(canvas, 102, (uint8_t)(y - 3));
}

void predator_ui_draw_scrollbar(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t height,
    uint8_t total_items,
    uint8_t first_visible,
    uint8_t visible_items) {
    if(height < 4) return;
    if(visible_items == 0 || total_items == 0 || first_visible >= total_items) return;
    if(visible_items > total_items) visible_items = total_items;

    // Draw track
    canvas_draw_frame(canvas, x, y, 2, height);

    // Thumb size proportional to visible/total with a minimum
    uint8_t thumb_h = (uint8_t)((uint16_t)height * visible_items / total_items);
    if(thumb_h < 4) thumb_h = 4;
    if(thumb_h > height - 2) thumb_h = height - 2;

    // Thumb position proportional to first_visible
    uint8_t max_first = total_items - visible_items;
    uint8_t thumb_y = y + 1;
    if(max_first > 0) {
        thumb_y = (uint8_t)(y + 1 + ((uint16_t)(height - 2 - thumb_h) * first_visible) / max_first);
    }

    // Draw thumb
    canvas_draw_box(canvas, x + 1, thumb_y, 1, thumb_h);
}
