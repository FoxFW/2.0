#pragma once

#include "../rolljam.h"

// Scene on_enter
void rolljam_scene_menu_on_enter(void* context);
void rolljam_scene_attack_phase1_on_enter(void* context);
void rolljam_scene_attack_phase2_on_enter(void* context);
void rolljam_scene_attack_phase3_on_enter(void* context);
void rolljam_scene_result_on_enter(void* context);

// Scene on_event
bool rolljam_scene_menu_on_event(void* context, SceneManagerEvent event);
bool rolljam_scene_attack_phase1_on_event(void* context, SceneManagerEvent event);
bool rolljam_scene_attack_phase2_on_event(void* context, SceneManagerEvent event);
bool rolljam_scene_attack_phase3_on_event(void* context, SceneManagerEvent event);
bool rolljam_scene_result_on_event(void* context, SceneManagerEvent event);

// Scene on_exit
void rolljam_scene_menu_on_exit(void* context);
void rolljam_scene_attack_phase1_on_exit(void* context);
void rolljam_scene_attack_phase2_on_exit(void* context);
void rolljam_scene_attack_phase3_on_exit(void* context);
void rolljam_scene_result_on_exit(void* context);

// Scene manager handlers (defined in rolljam.c)
extern const SceneManagerHandlers rolljam_scene_handlers;
