#include "cc1101_configs.h"
#include <cc1101_regs.h>

const uint8_t subghz_device_cc1101_preset_ook_270khz_async_regs[] = {

    CC1101_IOCFG0,
    0x0D,

    CC1101_FIFOTHR,
    0x47,

    CC1101_PKTCTRL0,
    0x32,

    CC1101_FSCTRL1,
    0x06,

    CC1101_MDMCFG0,
    0x00,
    CC1101_MDMCFG1,
    0x00,
    CC1101_MDMCFG2,
    0x30,
    CC1101_MDMCFG3,
    0x32,
    CC1101_MDMCFG4,
    0x67,

    CC1101_MCSM0,
    0x18,

    CC1101_FOCCFG,
    0x18,

    CC1101_AGCCTRL0,
    0x40,
    CC1101_AGCCTRL1,
    0x00,
    CC1101_AGCCTRL2,
    0x03,

    CC1101_WORCTRL,
    0xFB,

    CC1101_FREND0,
    0x11,
    CC1101_FREND1,
    0xB6,

    0,
    0,

    0x00,
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

const uint8_t subghz_device_cc1101_preset_ook_650khz_async_regs[] = {

    CC1101_IOCFG0,
    0x0D,

    CC1101_FIFOTHR,
    0x07,

    CC1101_PKTCTRL0,
    0x32,

    CC1101_FSCTRL1,
    0x06,

    CC1101_MDMCFG0,
    0x00,
    CC1101_MDMCFG1,
    0x00,
    CC1101_MDMCFG2,
    0x30,
    CC1101_MDMCFG3,
    0x32,
    CC1101_MDMCFG4,
    0x17,

    CC1101_MCSM0,
    0x18,

    CC1101_FOCCFG,
    0x18,

    CC1101_AGCCTRL0,
    0x91,
    CC1101_AGCCTRL1,
    0x0,
    CC1101_AGCCTRL2,
    0x07,

    CC1101_WORCTRL,
    0xFB,

    CC1101_FREND0,
    0x11,
    CC1101_FREND1,
    0xB6,

    0,
    0,

    0x00,
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

const uint8_t subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs[] = {

    CC1101_IOCFG0,
    0x0D,

    CC1101_FSCTRL1,
    0x06,

    CC1101_PKTCTRL0,
    0x32,
    CC1101_PKTCTRL1,
    0x04,

    CC1101_MDMCFG0,
    0x00,
    CC1101_MDMCFG1,
    0x02,
    CC1101_MDMCFG2,
    0x04,
    CC1101_MDMCFG3,
    0x83,
    CC1101_MDMCFG4,
    0x67,
    CC1101_DEVIATN,
    0x04,

    CC1101_MCSM0,
    0x18,

    CC1101_FOCCFG,
    0x16,

    CC1101_AGCCTRL0,
    0x91,
    CC1101_AGCCTRL1,
    0x00,
    CC1101_AGCCTRL2,
    0x07,

    CC1101_WORCTRL,
    0xFB,

    CC1101_FREND0,
    0x10,
    CC1101_FREND1,
    0x56,

    0,
    0,

    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

const uint8_t subghz_device_cc1101_preset_2fsk_dev12khz_async_regs[] = {

    CC1101_IOCFG0,
    0x0D,

    CC1101_FSCTRL1,
    0x06,

    CC1101_PKTCTRL0,
    0x32,
    CC1101_PKTCTRL1,
    0x04,

    CC1101_MDMCFG0,
    0x00,
    CC1101_MDMCFG1,
    0x02,
    CC1101_MDMCFG2,
    0x04,
    CC1101_MDMCFG3,
    0x83,
    CC1101_MDMCFG4,
    0x67,
    CC1101_DEVIATN,
    0x30,

    CC1101_MCSM0,
    0x18,

    CC1101_FOCCFG,
    0x16,

    CC1101_AGCCTRL0,
    0x91,
    CC1101_AGCCTRL1,
    0x00,
    CC1101_AGCCTRL2,
    0x07,

    CC1101_WORCTRL,
    0xFB,

    CC1101_FREND0,
    0x10,
    CC1101_FREND1,
    0x56,

    0,
    0,

    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

const uint8_t subghz_device_cc1101_preset_2fsk_dev47_6khz_async_regs[] = {

    CC1101_IOCFG0,
    0x0D,

    CC1101_FSCTRL1,
    0x06,

    CC1101_PKTCTRL0,
    0x32,
    CC1101_PKTCTRL1,
    0x04,

    CC1101_MDMCFG0,
    0x00,
    CC1101_MDMCFG1,
    0x02,
    CC1101_MDMCFG2,
    0x04,
    CC1101_MDMCFG3,
    0x83,
    CC1101_MDMCFG4,
    0x67,
    CC1101_DEVIATN,
    0x47,

    CC1101_MCSM0,
    0x18,

    CC1101_FOCCFG,
    0x16,

    CC1101_AGCCTRL0,
    0x91,
    CC1101_AGCCTRL1,
    0x00,
    CC1101_AGCCTRL2,
    0x07,

    CC1101_WORCTRL,
    0xFB,

    CC1101_FREND0,
    0x10,
    CC1101_FREND1,
    0x56,

    0,
    0,

    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

const uint8_t subghz_device_cc1101_preset_msk_99_97kb_async_regs[] = {

    CC1101_IOCFG0,
    0x06,

    CC1101_FIFOTHR,
    0x07,
    CC1101_SYNC1,
    0x46,
    CC1101_SYNC0,
    0x4C,
    CC1101_ADDR,
    0x00,
    CC1101_PKTLEN,
    0x00,
    CC1101_CHANNR,
    0x00,

    CC1101_PKTCTRL0,
    0x05,

    CC1101_FSCTRL0,
    0x23,
    CC1101_FSCTRL1,
    0x06,

    CC1101_MDMCFG0,
    0xF8,
    CC1101_MDMCFG1,
    0x22,
    CC1101_MDMCFG2,
    0x72,
    CC1101_MDMCFG3,
    0xF8,
    CC1101_MDMCFG4,
    0x5B,
    CC1101_DEVIATN,
    0x47,

    CC1101_MCSM0,
    0x18,
    CC1101_FOCCFG,
    0x16,

    CC1101_AGCCTRL0,
    0xB2,
    CC1101_AGCCTRL1,
    0x00,
    CC1101_AGCCTRL2,
    0xC7,

    CC1101_FREND0,
    0x10,
    CC1101_FREND1,
    0x56,

    CC1101_BSCFG,
    0x1C,
    CC1101_FSTEST,
    0x59,

    0,
    0,

    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

const uint8_t subghz_device_cc1101_preset_gfsk_9_99kb_async_regs[] = {

    CC1101_IOCFG0,
    0x06,
    CC1101_FIFOTHR,
    0x47,

    CC1101_PKTCTRL0,
    0x05,

    CC1101_FSCTRL1,
    0x06,

    CC1101_SYNC1,
    0x46,
    CC1101_SYNC0,
    0x4C,
    CC1101_ADDR,
    0x00,
    CC1101_PKTLEN,
    0x00,

    CC1101_MDMCFG4,
    0xC8,
    CC1101_MDMCFG3,
    0x93,
    CC1101_MDMCFG2,
    0x12,

    CC1101_DEVIATN,
    0x34,
    CC1101_MCSM0,
    0x18,
    CC1101_FOCCFG,
    0x16,

    CC1101_AGCCTRL2,
    0x43,
    CC1101_AGCCTRL1,
    0x40,
    CC1101_AGCCTRL0,
    0x91,

    CC1101_WORCTRL,
    0xFB,

    0,
    0,

    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};
