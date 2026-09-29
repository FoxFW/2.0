#pragma once

#include "../rolljam.h"

void rolljam_capture_start(RollJamApp* app);
void rolljam_capture_stop(RollJamApp* app);

bool rolljam_signal_is_valid(RawSignal* signal);

void rolljam_signal_cleanup(RawSignal* signal);
void rolljam_transmit_signal(RollJamApp* app, RawSignal* signal);
void rolljam_save_signal(RollJamApp* app, RawSignal* signal);
