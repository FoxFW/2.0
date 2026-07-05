/**
 * @file predator_scene_car_core.c
 * @brief Car Core Navigation Implementation
 * 
 * Combines continent selection and model selection scenes into
 * a single cohesive navigation module.
 */

#include "predator_scene_car_core.h"
#include "../../helpers/predator_models_hardcoded.h"

// ============================================================================
// CONTINENT SELECTION SCENE
// ============================================================================

static void continent_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_car_continent_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "🌍 Select Continent");
    
    // Use static labels to save stack memory (counts pre-calculated)
    // Europe: 124, Asia: 154, America: 37 (from database)
    submenu_add_item(app->submenu, "🇪🇺 Europe (124 models)", CarContinentEurope, continent_submenu_callback, app);
    submenu_add_item(app->submenu, "🇯🇵 Asia (154 models)", CarContinentAsia, continent_submenu_callback, app);
    submenu_add_item(app->submenu, "🇺🇸 America (37 models)", CarContinentAmerica, continent_submenu_callback, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_car_continent_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event.type == SceneManagerEventTypeBack) {
        // Go back to main menu
        return false;
    }
    
    if(event.type == SceneManagerEventTypeCustom) {
        // User selected a continent
        if(event.event < CarContinentCount) {
            // Store selected continent in app state
            app->selected_continent = (CarContinent)event.event;
            
            // Navigate to models scene (which will filter by continent)
            scene_manager_next_scene(app->scene_manager, PredatorSceneCarModelsUI);
            return true;
        }
    }
    
    return false;
}

void predator_scene_car_continent_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    // Nothing to cleanup - using shared submenu
}

// ============================================================================
// CAR MODELS SELECTION SCENE
// ============================================================================

// Pagination state
static uint16_t car_models_page = 0;
#define CAR_MODELS_PER_PAGE 16

static void car_models_submenu_cb(void* context, uint32_t index) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_car_models_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;

    submenu_reset(app->submenu);
    
    // Set header with continent name (no dynamic allocation)
    const char* continent_name = predator_models_get_continent_name(app->selected_continent);
    char header[32];
    snprintf(header, sizeof(header), "🚗 %s", continent_name);
    submenu_set_header(app->submenu, header);

    // Count filtered models on-the-fly (no large array allocation)
    const size_t total = predator_models_get_hardcoded_count();
    size_t filtered_count = 0;
    for(size_t i = 0; i < total; i++) {
        if(predator_models_is_continent(i, app->selected_continent)) {
            filtered_count++;
        }
    }
    
    const uint16_t total_pages = (filtered_count + CAR_MODELS_PER_PAGE - 1) / CAR_MODELS_PER_PAGE;
    if(car_models_page >= total_pages) car_models_page = 0;

    // Pagination header
    if(total_pages > 1) {
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "Page %u/%u", (unsigned)(car_models_page + 1), (unsigned)total_pages);
        submenu_add_item(app->submenu, hdr, 10000, car_models_submenu_cb, app);
    }

    // Add models for current page (on-the-fly filtering, no array)
    const size_t start = car_models_page * CAR_MODELS_PER_PAGE;
    const size_t end = start + CAR_MODELS_PER_PAGE;
    size_t displayed = 0;
    size_t filtered_idx = 0;
    
    for(size_t i = 0; i < total && displayed < CAR_MODELS_PER_PAGE; i++) {
        if(!predator_models_is_continent(i, app->selected_continent)) continue;
        
        if(filtered_idx >= start && filtered_idx < end) {
            const PredatorCarModel* model = predator_models_get_hardcoded(i);
            if(model) {
                char label[32];
                snprintf(label, sizeof(label), "%.15s %.15s", model->make, model->model);
                // Store model index in event ID for later protocol detection
                submenu_add_item(app->submenu, label, (uint32_t)(i + 1), car_models_submenu_cb, app);
                displayed++;
            }
        }
        filtered_idx++;
    }

    // Navigation buttons
    if(total_pages > 1) {
        submenu_add_item(app->submenu, "Next Page ▶", 20001, car_models_submenu_cb, app);
        submenu_add_item(app->submenu, "◀ Prev Page", 20002, car_models_submenu_cb, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
}

bool predator_scene_car_models_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;

    if(event.type == SceneManagerEventTypeBack) {
        // Return false to let scene manager navigate back
        return false;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        // Pagination header id = 10000; next=20001; prev=20002; model ids are index+1
        if(event.event == 20001) { // next
            car_models_page++;
            predator_scene_car_models_ui_on_enter(app);
            return true;
        } else if(event.event == 20002) { // prev
            if(car_models_page > 0) car_models_page--; else car_models_page = 0;
            predator_scene_car_models_ui_on_enter(app);
            return true;
        } else if(event.event >= 1 && event.event < 10000) {
            // Model selected
            size_t idx = (size_t)event.event - 1;
            const PredatorCarModel* model = predator_models_get_hardcoded(idx);
            
            if(model) {
                // Store selected model information
                app->selected_model_index = idx;
                app->selected_model_freq = model->frequency;
                strncpy(app->selected_model_make, model->make, sizeof(app->selected_model_make) - 1);
                strncpy(app->selected_model_name, model->model, sizeof(app->selected_model_name) - 1);
                
                FURI_LOG_I("CarModels", "Selected: %s %s (%lu Hz, %s)",
                          model->make, model->model, model->frequency, model->protocol);
                
                // Navigate to model attacks scene
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarModelAttacksUI);
                return true;
            }
        }
    }
    
    return false;
}

void predator_scene_car_models_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    // Nothing to cleanup - using shared submenu
}
