#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <furi_hal_i2c.h>

typedef enum {
    LP5562ChannelRed = (1 << 0),
    LP5562ChannelGreen = (1 << 1),
    LP5562ChannelBlue = (1 << 2),
    LP5562ChannelWhite = (1 << 3),
} LP5562Channel;

typedef enum {
    LP5562Direct = 0,
    LP5562Engine1 = 1,
    LP5562Engine2 = 2,
    LP5562Engine3 = 3,
} LP5562Engine;

void lp5562_reset(const FuriHalI2cBusHandle* handle);

void lp5562_configure(const FuriHalI2cBusHandle* handle);

void lp5562_enable(const FuriHalI2cBusHandle* handle);

void lp5562_set_channel_current(
    const FuriHalI2cBusHandle* handle,
    LP5562Channel channel,
    uint8_t value);

void lp5562_set_channel_value(
    const FuriHalI2cBusHandle* handle,
    LP5562Channel channel,
    uint8_t value);

uint8_t lp5562_get_channel_value(const FuriHalI2cBusHandle* handle, LP5562Channel channel);

void lp5562_set_channel_src(
    const FuriHalI2cBusHandle* handle,
    LP5562Channel channel,
    LP5562Engine src);

void lp5562_execute_program(
    const FuriHalI2cBusHandle* handle,
    LP5562Engine eng,
    LP5562Channel ch,
    uint16_t* program);

void lp5562_stop_program(const FuriHalI2cBusHandle* handle, LP5562Engine eng);

void lp5562_execute_ramp(
    const FuriHalI2cBusHandle* handle,
    LP5562Engine eng,
    LP5562Channel ch,
    uint8_t val_start,
    uint8_t val_end,
    uint16_t time);

void lp5562_execute_blink(
    const FuriHalI2cBusHandle* handle,
    LP5562Engine eng,
    LP5562Channel ch,
    uint16_t on_time,
    uint16_t period,
    uint8_t brightness);
