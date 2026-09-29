#pragma once

#include <stdbool.h>
#include <stdint.h>

#if defined(BITS_BIG_ENDIAN) && BITS_BIG_ENDIAN == 1
#error Bit structures defined in this file are not portable to BE
#endif

#define BQ25896_ADDRESS     0xD6
#define BQ25896_I2C_TIMEOUT 50

#define IILIM_1600 (1 << 5)
#define IILIM_800  (1 << 4)
#define IILIM_400  (1 << 3)
#define IILIM_200  (1 << 2)
#define IILIM_100  (1 << 1)
#define IILIM_50   (1 << 0)

typedef struct {
    uint8_t IINLIM : 6;
    bool EN_ILIM   : 1;
    bool EN_HIZ    : 1;
} REG00;

#define VINDPM_OS_1600 (1 << 4)
#define VINDPM_OS_800  (1 << 3)
#define VINDPM_OS_400  (1 << 2)
#define VINDPM_OS_200  (1 << 1)
#define VINDPM_OS_100  (1 << 0)

typedef enum {
    Bhot34 = 0b00,
    Bhot37 = 0b01,
    Bhot31 = 0b10,
    BhotDisable = 0b11,
} Bhot;

typedef struct {
    uint8_t VINDPM_OS : 5;
    bool BCOLD        : 1;
    Bhot BHOT         : 2;
} REG01;

typedef struct {
    bool AUTO_DPDM_EN : 1;
    bool FORCE_DPDM   : 1;
    uint8_t RES       : 2;
    bool ICO_EN       : 1;
    bool BOOST_FREQ   : 1;
    bool CONV_RATE    : 1;
    bool CONV_START   : 1;
} REG02;

#define SYS_MIN_400 (1 << 2)
#define SYS_MIN_200 (1 << 1)
#define SYS_MIN_100 (1 << 0)

typedef struct {
    bool MIN_VBAT_SEL : 1;
    uint8_t SYS_MIN   : 3;
    bool CHG_CONFIG   : 1;
    bool OTG_CONFIG   : 1;
    bool WD_RST       : 1;
    bool BAT_LOADEN   : 1;
} REG03;

#define ICHG_4096 (1 << 6)
#define ICHG_2048 (1 << 5)
#define ICHG_1024 (1 << 4)
#define ICHG_512  (1 << 3)
#define ICHG_256  (1 << 2)
#define ICHG_128  (1 << 1)
#define ICHG_64   (1 << 0)

typedef struct {
    uint8_t ICHG  : 7;
    bool EN_PUMPX : 1;
} REG04;

#define IPRETERM_512 (1 << 3)
#define IPRETERM_256 (1 << 2)
#define IPRETERM_128 (1 << 1)
#define IPRETERM_64  (1 << 0)

typedef struct {
    uint8_t ITERM   : 4;
    uint8_t IPRECHG : 4;
} REG05;

#define VREG_512 (1 << 5)
#define VREG_256 (1 << 4)
#define VREG_128 (1 << 3)
#define VREG_64  (1 << 2)
#define VREG_32  (1 << 1)
#define VREG_16  (1 << 0)

typedef struct {
    bool VRECHG  : 1;
    bool BATLOWV : 1;
    uint8_t VREG : 6;
} REG06;

typedef enum {
    WatchdogDisable = 0b00,
    Watchdog40 = 0b01,
    Watchdog80 = 0b10,
    Watchdog160 = 0b11,
} Watchdog;

typedef enum {
    ChgTimer5 = 0b00,
    ChgTimer8 = 0b01,
    ChgTimer12 = 0b10,
    ChgTimer20 = 0b11,
} ChgTimer;

typedef struct {
    bool JEITA_ISET    : 1;
    ChgTimer CHG_TIMER : 2;
    bool EN_TIMER      : 1;
    Watchdog WATCHDOG  : 2;
    bool STAT_DIS      : 1;
    bool EN_TERM       : 1;
} REG07;

#define BAT_COMP_80 (1 << 2)
#define BAT_COMP_40 (1 << 1)
#define BAT_COMP_20 (1 << 0)

#define VCLAMP_128 (1 << 2)
#define VCLAMP_64  (1 << 1)
#define VCLAMP_32  (1 << 0)

#define TREG_60  (0b00)
#define TREG_80  (0b01)
#define TREG_100 (0b10)
#define TREG_120 (0b11)

typedef struct {
    uint8_t TREG     : 2;
    uint8_t VCLAMP   : 3;
    uint8_t BAT_COMP : 3;
} REG08;

typedef struct {
    bool PUMPX_DN      : 1;
    bool PUMPX_UP      : 1;
    bool BATFET_RST_EN : 1;
    bool BATFET_DLY    : 1;
    bool JEITA_VSET    : 1;
    bool BATFET_DIS    : 1;
    bool TMR2X_EN      : 1;
    bool FORCE_ICO     : 1;
} REG09;

#define BOOSTV_512 (1 << 3)
#define BOOSTV_256 (1 << 2)
#define BOOSTV_128 (1 << 1)
#define BOOSTV_64  (1 << 0)

typedef enum {
    BoostLim_500 = 0b000,
    BoostLim_750 = 0b001,
    BoostLim_1200 = 0b010,
    BoostLim_1400 = 0b011,
    BoostLim_1650 = 0b100,
    BoostLim_1875 = 0b101,
    BoostLim_2150 = 0b110,
    BoostLim_Rsvd = 0b111,
} BoostLim;

typedef struct {
    uint8_t BOOST_LIM : 3;
    bool PFM_OTG_DIS  : 1;
    uint8_t BOOSTV    : 4;
} REG0A;

typedef enum {
    VBusStatNo = 0b000,
    VBusStatUSB = 0b001,
    VBusStatExternal = 0b010,
    VBusStatOTG = 0b111,
} VBusStat;

typedef enum {
    ChrgStatNo = 0b00,
    ChrgStatPre = 0b01,
    ChrgStatFast = 0b10,
    ChrgStatDone = 0b11,
} ChrgStat;

typedef struct {
    bool VSYS_STAT     : 1;
    bool RES           : 1;
    bool PG_STAT       : 1;
    ChrgStat CHRG_STAT : 2;
    VBusStat VBUS_STAT : 3;
} REG0B;

typedef enum {
    ChrgFaultNO = 0b00,
    ChrgFaultIN = 0b01,
    ChrgFaultTH = 0b10,
    ChrgFaultTIM = 0b11,
} ChrgFault;

typedef enum {
    NtcFaultNo = 0b000,
    NtcFaultWarm = 0b010,
    NtcFaultCool = 0b011,
    NtcFaultCold = 0b101,
    NtcFaultHot = 0b110,
} NtcFault;

typedef struct {
    NtcFault NTC_FAULT   : 3;
    bool BAT_FAULT       : 1;
    ChrgFault CHRG_FAULT : 2;
    bool BOOST_FAULT     : 1;
    bool WATCHDOG_FAULT  : 1;
} REG0C;

#define VINDPM_6400 (1 << 6)
#define VINDPM_3200 (1 << 5)
#define VINDPM_1600 (1 << 4)
#define VINDPM_800  (1 << 3)
#define VINDPM_400  (1 << 2)
#define VINDPM_200  (1 << 1)
#define VINDPM_100  (1 << 0)

typedef struct {
    uint8_t VINDPM    : 7;
    bool FORCE_VINDPM : 1;
} REG0D;

typedef struct {
    uint8_t BATV    : 7;
    bool THERM_STAT : 1;
} REG0E;

typedef struct {
    uint8_t SYSV : 7;
    uint8_t RES  : 1;
} REG0F;

typedef struct {
    uint8_t TSPCT : 7;
    uint8_t RES   : 1;
} REG10;

typedef struct {
    uint8_t VBUSV : 7;
    bool VBUS_GD  : 1;
} REG11;

typedef struct {
    uint8_t ICHGR : 7;
    uint8_t RES   : 1;
} REG12;

typedef struct {
    uint8_t
        IDPM_LIM : 6;
    bool IDPM_STAT : 1;
    bool VDPM_STAT : 1;
} REG13;

typedef struct {
    uint8_t DEV_REV    : 2;
    bool TS_PROFILE    : 1;
    uint8_t PN         : 3;
    bool ICO_OPTIMIZED : 1;
    bool REG_RST       : 1;
} REG14;
