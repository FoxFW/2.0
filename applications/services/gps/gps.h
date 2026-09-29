#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_GPS "gps"

typedef struct Gps Gps;

typedef enum {
    GpsStatusOk,
    GpsStatusNotSupported,
    GpsStatusNoPermission,
    GpsStatusDisabled,
    GpsStatusUnknown,
} GpsStatus;

typedef struct {
    int32_t latitude;
    int32_t longitude;
    uint32_t heading;
    uint32_t speed;
    int32_t altitude;
    uint32_t accuracy;
    uint32_t satellites;
} GpsLocation;

static inline uint32_t gps_location_abs_i32(int32_t value) {
    return (value < 0) ? (uint32_t)(-(value + 1)) + 1 : (uint32_t)value;
}

static inline void gps_location_format_fixed_i32(
    char* buffer,
    size_t buffer_size,
    int32_t value,
    uint32_t scale,
    uint8_t decimals) {
    uint32_t abs_value = gps_location_abs_i32(value);
    snprintf(
        buffer,
        buffer_size,
        "%s%lu.%0*lu",
        value < 0 ? "-" : "",
        (unsigned long)(abs_value / scale),
        decimals,
        (unsigned long)(abs_value % scale));
}

static inline void gps_location_format_fixed_u32(
    char* buffer,
    size_t buffer_size,
    uint32_t value,
    uint32_t scale,
    uint8_t decimals) {
    snprintf(
        buffer,
        buffer_size,
        "%lu.%0*lu",
        (unsigned long)(value / scale),
        decimals,
        (unsigned long)(value % scale));
}

static inline void
    gps_location_format_coordinate(char* buffer, size_t buffer_size, int32_t degrees_e7) {
    gps_location_format_fixed_i32(buffer, buffer_size, degrees_e7, 10000000, 7);
}

static inline void
    gps_location_format_heading(char* buffer, size_t buffer_size, uint32_t degrees_centi) {
    gps_location_format_fixed_u32(buffer, buffer_size, degrees_centi / 10, 10, 1);
}

static inline void
    gps_location_format_speed(char* buffer, size_t buffer_size, uint32_t millimeters_per_second) {
    gps_location_format_fixed_u32(buffer, buffer_size, millimeters_per_second / 10, 100, 2);
}

static inline void
    gps_location_format_altitude(char* buffer, size_t buffer_size, int32_t centimeters) {
    gps_location_format_fixed_i32(buffer, buffer_size, centimeters / 10, 10, 1);
}

static inline void
    gps_location_format_accuracy(char* buffer, size_t buffer_size, uint32_t millimeters) {
    gps_location_format_fixed_u32(buffer, buffer_size, millimeters / 100, 10, 1);
}

typedef void (*GpsLocationCallback)(GpsStatus status, const GpsLocation* location, void* context);

bool gps_request_stream(Gps* gps, uint8_t frequency);

bool gps_stop_stream(Gps* gps);

bool gps_request_location(Gps* gps);

bool gps_report_location(Gps* gps, const GpsLocation* location);

void gps_set_location_callback(Gps* gps, GpsLocationCallback callback, void* context);

#ifdef __cplusplus
}
#endif
