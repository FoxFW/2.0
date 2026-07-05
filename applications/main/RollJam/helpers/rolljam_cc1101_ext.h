#pragma once

#include "../rolljam.h"

/*
 * External CC1101 module connected via GPIO (bit-bang SPI).
 * Used EXCLUSIVELY for JAMMING (TX).
 *
 * Wiring (as connected):
 *   CC1101 VCC  -> Flipper Pin 9  (3V3)
 *   CC1101 GND  -> Flipper Pin 11 (GND)
 *   CC1101 MOSI -> Flipper Pin 2  (PA7)
 *   CC1101 MISO -> Flipper Pin 3  (PA6)
 *   CC1101 SCK  -> Flipper Pin 5  (PB3)
 *   CC1101 CS   -> Flipper Pin 4  (PA4)
 *   CC1101 GDO0 -> Flipper Pin 6  (PB2)
 */

void rolljam_ext_gpio_init(void);
void rolljam_ext_set_flux_capacitor(bool enabled);
void rolljam_ext_gpio_deinit(void);
void rolljam_jammer_start(RollJamApp* app);
void rolljam_jammer_stop(RollJamApp* app);
