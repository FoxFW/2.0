#pragma once

#include "../rolljam.h"

void rolljam_scene_menu_on_enter(void* context);
void rolljam_scene_attack_phase1_on_enter(void* context);
void rolljam_scene_attack_phase2_on_enter(void* context);
void rolljam_scene_attack_phase3_on_enter(void* context);
void rolljam_scene_result_on_enter(void* context);

bool rolljam_scene_menu_on_event(void* context, SceneManagerEvent event);
bool rolljam_scene_attack_phase1_on_event(void* context, SceneManagerEvent event);
bool rolljam_scene_attack_phase2_on_event(void* context, SceneManagerEvent event);
bool rolljam_scene_attack_phase3_on_event(void* context, SceneManagerEvent event);
bool rolljam_scene_result_on_event(void* context, SceneManagerEvent event);

void rolljam_scene_menu_on_exit(void* context);
void rolljam_scene_attack_phase1_on_exit(void* context);
void rolljam_scene_attack_phase2_on_exit(void* context);
void rolljam_scene_attack_phase3_on_exit(void* context);
void rolljam_scene_result_on_exit(void* context);

extern const SceneManagerHandlers rolljam_scene_handlers;
