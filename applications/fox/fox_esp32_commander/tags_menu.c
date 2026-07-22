#include "tags_menu.h"

#include <string.h>

typedef enum {
    MenuTagFindMy,
    MenuTagSmartTag,
    MenuTagTile,
} MenuTagIndex;

void tags_render_menu(App* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Tag Detection");
    submenu_add_item(
        app->submenu, "AirTag / Find My", MenuTagFindMy, app_menu_item_callback, app);
    submenu_add_item(
        app->submenu, "Samsung SmartTag", MenuTagSmartTag, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Tile", MenuTagTile, app_menu_item_callback, app);
}

static void run_tag_scan(App* app, const char* command, const char* label) {
    app_log(app, "Scanning for %s (8s)...", label);
    app_render_log(app);
    esp_at_send(app->esp_at, command);

    EspAtMsg msg;
    int found = 0;
    uint32_t deadline = furi_get_tick() + 11000;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        if(strncmp(msg.line, "TAG:", 4) == 0) {
            found++;
            app_log(app, "%s", msg.line);
        } else if(strncmp(msg.line, "TAGSCANDONE", 11) == 0) {
            break;
        } else if(strncmp(msg.line, "ERROR", 5) == 0) {
            app_log(app, "%s", msg.line);
            break;
        }
    }
    if(found == 0) app_log(app, "No %s tags found nearby.", label);
    app_render_log(app);
}

void tags_menu_select(App* app, uint32_t index) {
    switch((MenuTagIndex)index) {
    case MenuTagFindMy:
        run_tag_scan(app, "BLETAGSCAN:FINDMY", "AirTag / Find My");
        break;
    case MenuTagSmartTag:
        run_tag_scan(app, "BLETAGSCAN:SMARTTAG", "Samsung SmartTag");
        break;
    case MenuTagTile:
        run_tag_scan(app, "BLETAGSCAN:TILE", "Tile");
        break;
    }
}
