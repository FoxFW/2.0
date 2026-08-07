#include "settings_view.h"

#include <string.h>

void app_expert_mode_refresh(App* app) {
    esp_at_send(app->esp_at, "SETTINGS");
    EspAtMsg msg;
    for(int i = 0; i < 3; i++) {
        if(!esp_at_receive(app->esp_at, &msg, 1500)) break;
        if(strncmp(msg.line, "EXPERTMODE:", 11) == 0) {
            app->expert_mode = (strcmp(msg.line, "EXPERTMODE:ON") == 0);
            break;
        }
    }
}
