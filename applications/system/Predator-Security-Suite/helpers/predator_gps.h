#pragma once

#include "../predator_i.h"

typedef struct PredatorGps PredatorGps;
typedef struct PredatorApp PredatorApp;

// GPS callback for received data
void predator_gps_rx_callback(uint8_t* buf, size_t len, void* context);
void predator_gps_init(PredatorApp* app);
void predator_gps_deinit(PredatorApp* app);
void predator_gps_update(PredatorApp* app);
bool predator_gps_parse_nmea(PredatorApp* app, const char* sentence);
bool predator_gps_get_coordinates(PredatorApp* app, float* lat, float* lon);
uint32_t predator_gps_get_satellites(PredatorApp* app);
bool predator_gps_is_connected(PredatorApp* app);

// GPS data parsing
