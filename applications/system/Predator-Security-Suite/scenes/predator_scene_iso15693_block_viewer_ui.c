#include "../predator_i.h"
#include "../helpers/predator_crypto_iso15693.h"
#include <gui/elements.h>

// ISO 15693 Block Viewer - Scrollable block list
// Pattern: Similar to FeliCa history

typedef struct {
    ISO15693Tag tag;
    uint8_t blocks[64][4]; // Max 64 blocks, 4 bytes each
    uint8_t block_count;
    uint8_t scroll_offset;
    char status_text[64];
} BlockViewerState;

static BlockViewerState* viewer_state = NULL;

static void block_viewer_draw_callback(Canvas* canvas, void* context) {
    PredatorApp* app = context;
    if(!viewer_state) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Block Viewer");

    if(viewer_state->block_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "No blocks read");
        return;
    }

    // Draw blocks (4 visible at a time)
    canvas_set_font(canvas, FontSecondary);
    int y = 20;
    
    for(uint8_t i = viewer_state->scroll_offset; 
        i < viewer_state->block_count && i < viewer_state->scroll_offset + 4; 
        i++) {
        
        char block_str[64];
        snprintf(block_str, sizeof(block_str), 
                 "%02u: %02X %02X %02X %02X",
                 i,
                 viewer_state->blocks[i][0],
                 viewer_state->blocks[i][1],
                 viewer_state->blocks[i][2],
                 viewer_state->blocks[i][3]);
        
        canvas_draw_str(canvas, 2, y, block_str);
        y += 10;
    }
    
    // Scroll indicator
    if(viewer_state->block_count > 4) {
        char scroll_info[16];
        snprintf(scroll_info, sizeof(scroll_info), "%u/%u",
                 viewer_state->scroll_offset + 1,
                 viewer_state->block_count);
        canvas_draw_str(canvas, 100, 10, scroll_info);
        
        if(viewer_state->scroll_offset > 0) {
            canvas_draw_str(canvas, 120, 30, "↑");
        }
        if(viewer_state->scroll_offset + 4 < viewer_state->block_count) {
            canvas_draw_str(canvas, 120, 50, "↓");
        }
    }
    
    // Status bar
    canvas_draw_line(canvas, 0, 60, 128, 60);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 63, viewer_state->status_text);
}

static bool block_viewer_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyBack) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        
        if(event->key == InputKeyUp && viewer_state->scroll_offset > 0) {
            viewer_state->scroll_offset--;
            view_port_update(app->view_port);
            return true;
        }
        
        if(event->key == InputKeyDown && 
           viewer_state->scroll_offset + 4 < viewer_state->block_count) {
            viewer_state->scroll_offset++;
            view_port_update(app->view_port);
            return true;
        }
    }
    
    return false;
}

void predator_scene_iso15693_block_viewer_on_enter(void* context) {
    PredatorApp* app = context;
    
    viewer_state = malloc(sizeof(BlockViewerState));
    memset(viewer_state, 0, sizeof(BlockViewerState));
    
    // Read all blocks
    // Real implementation: iso15693_read_all_blocks(app, &viewer_state->tag, viewer_state->blocks)
    viewer_state->block_count = 16; // Typical size
    
    snprintf(viewer_state->status_text, sizeof(viewer_state->status_text),
             "↑/↓ Scroll");
    
    view_port_draw_callback_set(app->view_port, block_viewer_draw_callback, app);
    view_port_input_callback_set(app->view_port, block_viewer_input_callback, app);
    
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
}

bool predator_scene_iso15693_block_viewer_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void predator_scene_iso15693_block_viewer_on_exit(void* context) {
    PredatorApp* app = context;
    
    gui_remove_view_port(app->gui, app->view_port);
    
    if(viewer_state) {
        free(viewer_state);
        viewer_state = NULL;
    }
}
