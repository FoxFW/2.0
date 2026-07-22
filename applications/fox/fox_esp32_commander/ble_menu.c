#include "ble_menu.h"

#include <string.h>

typedef enum {
    MenuBleIos,
    MenuBleWindows,
    MenuBleSamsung,
    MenuBleAndroid,
    MenuBleAll,
} MenuBleIndex;

void ble_render_menu(App* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "BLE Spam");
    submenu_add_item(app->submenu, "iOS", MenuBleIos, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Windows", MenuBleWindows, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Samsung", MenuBleSamsung, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Android", MenuBleAndroid, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "All", MenuBleAll, app_menu_item_callback, app);
}

static void run_blespam(App* app, const char* command, const char* label) {
    app_log(app, "BLE spam: %s (8s)...", label);
    app_render_log(app);
    esp_at_send(app->esp_at, command);

    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + 11000;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        app_log(app, "%s", msg.line);
        if(strcmp(msg.line, "ATTACKDONE") == 0) break;
        if(strncmp(msg.line, "ERROR", 5) == 0) break;
    }
    app_render_log(app);
}

void ble_menu_select(App* app, uint32_t index) {
    switch((MenuBleIndex)index) {
    case MenuBleIos:
        run_blespam(app, "BLESPAM:IOS", "iOS");
        break;
    case MenuBleWindows:
        run_blespam(app, "BLESPAM:WINDOWS", "Windows");
        break;
    case MenuBleSamsung:
        run_blespam(app, "BLESPAM:SAMSUNG", "Samsung");
        break;
    case MenuBleAndroid:
        run_blespam(app, "BLESPAM:ANDROID", "Android");
        break;
    case MenuBleAll:
        run_blespam(app, "BLESPAM:ALL", "All");
        break;
    }
}
