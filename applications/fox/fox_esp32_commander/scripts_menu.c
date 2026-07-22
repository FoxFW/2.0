#include "scripts_menu.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    MenuScriptActionRun,
    MenuScriptActionShow,
    MenuScriptActionDelete,
} MenuScriptActionIndex;

void scripts_render_menu(App* app) {
    app->script_count = 0;

    app_log(app, "Loading scripts...");
    app_render_log(app);
    esp_at_send(app->esp_at, "SCRIPTLIST");

    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + 3000;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        if(strcmp(msg.line, "SCRIPTLISTDONE") == 0) break;
        if(strncmp(msg.line, "SCRIPT:", 7) != 0) continue;
        if(app->script_count >= FOX_SCRIPT_MAX) continue;

        char name[FOX_SCRIPT_NAME_MAX] = {0};
        uint32_t bytes = 0;
        if(sscanf(msg.line + 7, "%39[^ ] bytes:%lu", name, &bytes) >= 1) {
            /* Strip .js suffix — sanitizeName() adds it back on the firmware side. */
            size_t len = strlen(name);
            if(len > 3 && strcmp(name + len - 3, ".js") == 0) name[len - 3] = '\0';

            strncpy(app->scripts[app->script_count].name, name, FOX_SCRIPT_NAME_MAX - 1);
            app->scripts[app->script_count].name[FOX_SCRIPT_NAME_MAX - 1] = '\0';
            app->scripts[app->script_count].bytes = bytes;
            app->script_count++;
        }
    }

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Scripts");
    for(size_t i = 0; i < app->script_count; i++) {
        submenu_add_item(app->submenu, app->scripts[i].name, i, app_menu_item_callback, app);
    }
    submenu_add_item(app->submenu, "New Script", app->script_count, app_menu_item_callback, app);
}

void scripts_menu_select(App* app, uint32_t index) {
    if(index < app->script_count) {
        app->script_selected = index;
        app_switch_to_menu(app, MenuContextScriptActions);
        return;
    }

    app_show_text_input(app, "Script name", TextInputPurposeScriptName);
}

void scripts_actions_render_menu(App* app) {
    submenu_reset(app->submenu);
    const char* name = (app->script_selected < app->script_count) ?
                            app->scripts[app->script_selected].name :
                            "Script";
    submenu_set_header(app->submenu, name);
    submenu_add_item(app->submenu, "Run", MenuScriptActionRun, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Show", MenuScriptActionShow, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Delete", MenuScriptActionDelete, app_menu_item_callback, app);
}

static void action_run(App* app, const char* name) {
    char cmd[FOX_SCRIPT_NAME_MAX + 16];
    snprintf(cmd, sizeof(cmd), "SCRIPTRUN:%s", name);
    esp_at_send(app->esp_at, cmd);

    app_log(app, "Running %s...", name);
    app_render_log(app);
    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + 15000;
    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;
        app_log(app, "%s", msg.line);
        if(strcmp(msg.line, "SCRIPTDONE") == 0) break;
    }
    app_render_log(app);
}

static void action_show(App* app, const char* name) {
    char cmd[FOX_SCRIPT_NAME_MAX + 16];
    snprintf(cmd, sizeof(cmd), "SCRIPTSHOW:%s", name);
    esp_at_send(app->esp_at, cmd);

    app_log(app, "Fetching %s...", name);
    app_render_log(app);
    EspAtMsg msg;
    if(esp_at_receive(app->esp_at, &msg, 3000)) {
        app_log(app, "%s", msg.line);
    } else {
        app_log(app, "No response.");
    }
    app_render_log(app);
}

static void action_delete(App* app, const char* name) {
    char cmd[FOX_SCRIPT_NAME_MAX + 16];
    snprintf(cmd, sizeof(cmd), "SCRIPTDEL:%s", name);
    esp_at_send(app->esp_at, cmd);

    app_log(app, "Deleting %s...", name);
    app_render_log(app);
    EspAtMsg msg;
    if(esp_at_receive(app->esp_at, &msg, 3000)) {
        app_log(app, "%s", msg.line);
    } else {
        app_log(app, "No response.");
    }

    /* Return to Scripts menu — ScriptActions entry may no longer exist after delete. */
    app->menu_return_context = MenuContextScripts;
    app_render_log(app);
}

void scripts_actions_select(App* app, uint32_t index) {
    if(app->script_selected >= app->script_count) return;
    const char* name = app->scripts[app->script_selected].name;

    switch((MenuScriptActionIndex)index) {
    case MenuScriptActionRun:
        action_run(app, name);
        break;
    case MenuScriptActionShow:
        action_show(app, name);
        break;
    case MenuScriptActionDelete:
        action_delete(app, name);
        break;
    }
}

void scripts_name_submitted(App* app) {
    if(app->text_input_buffer[0] == '\0') {
        app_switch_to_menu(app, MenuContextScripts);
        return;
    }
    furi_string_set(app->pending_script_name, app->text_input_buffer);
    app_show_text_input(app, "Script source", TextInputPurposeScriptSource);
}

void scripts_source_submitted(App* app) {
    const char* name = furi_string_get_cstr(app->pending_script_name);
    const char* source = app->text_input_buffer;

    char cmd[FOX_SCRIPT_NAME_MAX + FOX_TEXT_INPUT_BUFFER_MAX + 16];
    snprintf(cmd, sizeof(cmd), "SCRIPTSAVE:%s:%s", name, source);
    esp_at_send(app->esp_at, cmd);

    app_log(app, "Saving %s...", name);
    app_render_log(app);
    EspAtMsg msg;
    if(esp_at_receive(app->esp_at, &msg, 3000)) {
        app_log(app, "%s", msg.line);
    } else {
        app_log(app, "No response.");
    }

    app->menu_return_context = MenuContextScripts;
    app_render_log(app);
}
