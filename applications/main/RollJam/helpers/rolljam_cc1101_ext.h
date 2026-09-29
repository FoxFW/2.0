#pragma once

#include "../rolljam.h"

void rolljam_ext_gpio_init(void);
void rolljam_ext_set_flux_capacitor(bool enabled);
void rolljam_ext_gpio_deinit(void);
void rolljam_jammer_start(RollJamApp* app);
void rolljam_jammer_stop(RollJamApp* app);
