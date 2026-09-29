#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SceneManagerEventTypeCustom,
    SceneManagerEventTypeBack,
    SceneManagerEventTypeTick,
} SceneManagerEventType;

typedef struct {
    SceneManagerEventType type;
    uint32_t event;
} SceneManagerEvent;

typedef void (*AppSceneOnEnterCallback)(void* context);

typedef bool (*AppSceneOnEventCallback)(void* context, SceneManagerEvent event);

typedef void (*AppSceneOnExitCallback)(void* context);

typedef struct {
    const AppSceneOnEnterCallback* on_enter_handlers;
    const AppSceneOnEventCallback* on_event_handlers;
    const AppSceneOnExitCallback* on_exit_handlers;
    const uint32_t scene_num;
} SceneManagerHandlers;

typedef struct SceneManager SceneManager;

void scene_manager_set_scene_state(SceneManager* scene_manager, uint32_t scene_id, uint32_t state);

uint32_t scene_manager_get_scene_state(const SceneManager* scene_manager, uint32_t scene_id);

SceneManager* scene_manager_alloc(const SceneManagerHandlers* app_scene_handlers, void* context);

void scene_manager_free(SceneManager* scene_manager);

bool scene_manager_handle_custom_event(SceneManager* scene_manager, uint32_t custom_event);

bool scene_manager_handle_back_event(SceneManager* scene_manager);

void scene_manager_handle_tick_event(SceneManager* scene_manager);

void scene_manager_next_scene(SceneManager* scene_manager, uint32_t next_scene_id);

bool scene_manager_previous_scene(SceneManager* scene_manager);

bool scene_manager_has_previous_scene(const SceneManager* scene_manager, uint32_t scene_id);

bool scene_manager_search_and_switch_to_previous_scene(
    SceneManager* scene_manager,
    uint32_t scene_id);

bool scene_manager_search_and_switch_to_previous_scene_one_of(
    SceneManager* scene_manager,
    const uint32_t* scene_ids,
    size_t scene_ids_size);

bool scene_manager_search_and_switch_to_another_scene(
    SceneManager* scene_manager,
    uint32_t scene_id);

uint32_t scene_manager_get_current_scene(SceneManager* scene_manager);

void scene_manager_stop(SceneManager* scene_manager);

#ifdef __cplusplus
}
#endif
