#include "lp5562.h"
#include <core/common_defines.h>
#include "lp5562_reg.h"
#include <furi_hal.h>

void lp5562_reset(const FuriHalI2cBusHandle* handle) {
    Reg0D_Reset reg = {.value = 0xFF};
    furi_hal_i2c_write_reg_8(handle, LP5562_ADDRESS, 0x0D, *(uint8_t*)&reg, LP5562_I2C_TIMEOUT);
}

void lp5562_configure(const FuriHalI2cBusHandle* handle) {
    Reg08_Config config = {.INT_CLK_EN = true, .PS_EN = true, .PWM_HF = true};
    furi_hal_i2c_write_reg_8(handle, LP5562_ADDRESS, 0x08, *(uint8_t*)&config, LP5562_I2C_TIMEOUT);

    Reg70_LedMap map = {
        .red = EngSelectI2C,
        .green = EngSelectI2C,
        .blue = EngSelectI2C,
        .white = EngSelectI2C,
    };
    furi_hal_i2c_write_reg_8(handle, LP5562_ADDRESS, 0x70, *(uint8_t*)&map, LP5562_I2C_TIMEOUT);
}

void lp5562_enable(const FuriHalI2cBusHandle* handle) {
    Reg00_Enable reg = {.CHIP_EN = true, .LOG_EN = true};
    furi_hal_i2c_write_reg_8(handle, LP5562_ADDRESS, 0x00, *(uint8_t*)&reg, LP5562_I2C_TIMEOUT);

    furi_delay_us(500);
}

void lp5562_set_channel_current(
    const FuriHalI2cBusHandle* handle,
    LP5562Channel channel,
    uint8_t value) {
    uint8_t reg_no;
    if(channel == LP5562ChannelRed) {
        reg_no = LP5562_CHANNEL_RED_CURRENT_REGISTER;
    } else if(channel == LP5562ChannelGreen) {
        reg_no = LP5562_CHANNEL_GREEN_CURRENT_REGISTER;
    } else if(channel == LP5562ChannelBlue) {
        reg_no = LP5562_CHANNEL_BLUE_CURRENT_REGISTER;
    } else if(channel == LP5562ChannelWhite) {
        reg_no = LP5562_CHANNEL_WHITE_CURRENT_REGISTER;
    } else {
        return;
    }
    furi_hal_i2c_write_reg_8(handle, LP5562_ADDRESS, reg_no, value, LP5562_I2C_TIMEOUT);
}

void lp5562_set_channel_value(
    const FuriHalI2cBusHandle* handle,
    LP5562Channel channel,
    uint8_t value) {
    uint8_t reg_no;
    if(channel == LP5562ChannelRed) {
        reg_no = LP5562_CHANNEL_RED_VALUE_REGISTER;
    } else if(channel == LP5562ChannelGreen) {
        reg_no = LP5562_CHANNEL_GREEN_VALUE_REGISTER;
    } else if(channel == LP5562ChannelBlue) {
        reg_no = LP5562_CHANNEL_BLUE_VALUE_REGISTER;
    } else if(channel == LP5562ChannelWhite) {
        reg_no = LP5562_CHANNEL_WHITE_VALUE_REGISTER;
    } else {
        return;
    }
    furi_hal_i2c_write_reg_8(handle, LP5562_ADDRESS, reg_no, value, LP5562_I2C_TIMEOUT);
}

uint8_t lp5562_get_channel_value(const FuriHalI2cBusHandle* handle, LP5562Channel channel) {
    uint8_t reg_no;
    uint8_t value;
    if(channel == LP5562ChannelRed) {
        reg_no = LP5562_CHANNEL_RED_VALUE_REGISTER;
    } else if(channel == LP5562ChannelGreen) {
        reg_no = LP5562_CHANNEL_GREEN_VALUE_REGISTER;
    } else if(channel == LP5562ChannelBlue) {
        reg_no = LP5562_CHANNEL_BLUE_VALUE_REGISTER;
    } else if(channel == LP5562ChannelWhite) {
        reg_no = LP5562_CHANNEL_WHITE_VALUE_REGISTER;
    } else {
        return 0;
    }
    furi_hal_i2c_read_reg_8(handle, LP5562_ADDRESS, reg_no, &value, LP5562_I2C_TIMEOUT);
    return value;
}

void lp5562_set_channel_src(
    const FuriHalI2cBusHandle* handle,
    LP5562Channel channel,
    LP5562Engine src) {
    uint8_t reg_val = 0;
    uint8_t bit_offset = 0;

    do {
        if(channel & LP5562ChannelRed) {
            bit_offset = 4;
            channel &= ~LP5562ChannelRed;
        } else if(channel & LP5562ChannelGreen) {
            bit_offset = 2;
            channel &= ~LP5562ChannelGreen;
        } else if(channel & LP5562ChannelBlue) {
            bit_offset = 0;
            channel &= ~LP5562ChannelBlue;
        } else if(channel & LP5562ChannelWhite) {
            bit_offset = 6;
            channel &= ~LP5562ChannelWhite;
        } else {
            return;
        }

        furi_hal_i2c_read_reg_8(handle, LP5562_ADDRESS, 0x70, &reg_val, LP5562_I2C_TIMEOUT);
        reg_val &= ~(0x3 << bit_offset);
        reg_val |= ((src & 0x03) << bit_offset);
        furi_hal_i2c_write_reg_8(handle, LP5562_ADDRESS, 0x70, reg_val, LP5562_I2C_TIMEOUT);
    } while(channel != 0);
}

void lp5562_execute_program(
    const FuriHalI2cBusHandle* handle,
    LP5562Engine eng,
    LP5562Channel ch,
    uint16_t* program) {
    if((eng < LP5562Engine1) || (eng > LP5562Engine3)) return;
    uint8_t reg_val = 0;
    uint8_t bit_offset = 0;
    uint8_t enable_reg = 0;

    furi_hal_i2c_read_reg_8(handle, LP5562_ADDRESS, 0x00, &enable_reg, LP5562_I2C_TIMEOUT);

    bit_offset = (3 - eng) * 2;
    furi_hal_i2c_read_reg_8(handle, LP5562_ADDRESS, 0x01, &reg_val, LP5562_I2C_TIMEOUT);
    reg_val &= ~(0x3 << bit_offset);
    reg_val |= (0x01 << bit_offset);
    furi_hal_i2c_write_reg_8(handle, LP5562_ADDRESS, 0x01, reg_val, LP5562_I2C_TIMEOUT);
    furi_delay_us(100);

    for(uint8_t i = 0; i < 16; i++) {

        program[i] = __REV16(program[i]);
    }
    furi_hal_i2c_write_mem(
        handle,
        LP5562_ADDRESS,
        0x10 + (0x20 * (eng - 1)),
        (uint8_t*)program,
        16 * 2,
        LP5562_I2C_TIMEOUT);

    bit_offset = (3 - eng) * 2;
    furi_hal_i2c_read_reg_8(handle, LP5562_ADDRESS, 0x01, &reg_val, LP5562_I2C_TIMEOUT);
    reg_val &= ~(0x3 << bit_offset);
    reg_val |= (0x02 << bit_offset);
    furi_hal_i2c_write_reg_8(handle, LP5562_ADDRESS, 0x01, reg_val, LP5562_I2C_TIMEOUT);

    lp5562_set_channel_src(handle, ch, eng);

    enable_reg &= ~(0x3 << bit_offset);
    enable_reg |= (0x02 << bit_offset);
    furi_hal_i2c_write_reg_8(handle, LP5562_ADDRESS, 0x00, enable_reg, LP5562_I2C_TIMEOUT);
}

void lp5562_stop_program(const FuriHalI2cBusHandle* handle, LP5562Engine eng) {
    if((eng < LP5562Engine1) || (eng > LP5562Engine3)) return;
    uint8_t reg_val = 0;
    uint8_t bit_offset = 0;

    bit_offset = (3 - eng) * 2;
    furi_hal_i2c_read_reg_8(handle, LP5562_ADDRESS, 0x01, &reg_val, LP5562_I2C_TIMEOUT);
    reg_val &= ~(0x3 << bit_offset);

    furi_hal_i2c_write_reg_8(handle, LP5562_ADDRESS, 0x01, reg_val, LP5562_I2C_TIMEOUT);
}

void lp5562_execute_ramp(
    const FuriHalI2cBusHandle* handle,
    LP5562Engine eng,
    LP5562Channel ch,
    uint8_t val_start,
    uint8_t val_end,
    uint16_t time) {
    if(val_start == val_end) return;

    lp5562_set_channel_src(handle, ch, LP5562Direct);

    uint16_t program[16];
    uint8_t diff = (val_end > val_start) ? (val_end - val_start) : (val_start - val_end);
    if(diff == 0) {
        diff = 1;
    }
    uint16_t time_step = time * 2 / diff;
    uint8_t prescaller = 0;
    if(time_step > 0x3F) {
        time_step /= 32;
        prescaller = 1;
    }

    if(time_step == 0) {
        time_step = 1;
    } else if(time_step > 0x3F)
        time_step = 0x3F;

    program[0] = 0x4000 | val_start;
    if(val_end > val_start) {
        program[1] = (prescaller << 14) | (time_step << 8) | ((diff / 2) & 0x7F);
    } else {
        program[1] = (prescaller << 14) | (time_step << 8) | 0x80 |
                     ((diff / 2) & 0x7F);
    }
    program[2] = 0xA001 | ((2 - 1) << 7);
    program[3] = 0xC000;

    lp5562_execute_program(handle, eng, LP5562ChannelWhite, program);

    lp5562_set_channel_value(handle, ch, val_end);
}

void lp5562_execute_blink(
    const FuriHalI2cBusHandle* handle,
    LP5562Engine eng,
    LP5562Channel ch,
    uint16_t on_time,
    uint16_t period,
    uint8_t brightness) {

    lp5562_set_channel_src(handle, ch, LP5562Direct);

    uint16_t program[16];
    uint16_t time_step = 0;
    uint8_t prescaller = 0;

    program[0] = 0x4000 | brightness;

    time_step = on_time * 2;
    if(time_step > 0x3F) {
        time_step /= 32;
        prescaller = 1;
    } else {
        prescaller = 0;
    }
    if(time_step == 0) {
        time_step = 1;
    } else if(time_step > 0x3F)
        time_step = 0x3F;
    program[1] = (prescaller << 14) | (time_step << 8);

    program[2] = 0x4000 | 0;

    time_step = (period - on_time) * 2;
    if(time_step > 0x3F) {
        time_step /= 32;
        prescaller = 1;
    } else {
        prescaller = 0;
    }
    if(time_step == 0) {
        time_step = 1;
    } else if(time_step > 0x3F)
        time_step = 0x3F;
    program[3] = (prescaller << 14) | (time_step << 8);

    program[4] = 0x0000;

    lp5562_execute_program(handle, eng, ch, program);
}
