#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CC1101_QUARTZ 26000000
#define CC1101_FMASK  0xFFFFFF
#define CC1101_FDIV   0x10000
#define CC1101_IFDIV  0x400

#define CC1101_TIMEOUT 250

#define CC1101_READ  (1 << 7)
#define CC1101_BURST (1 << 6)

#define CC1101_IOCFG2   0x00
#define CC1101_IOCFG1   0x01
#define CC1101_IOCFG0   0x02
#define CC1101_FIFOTHR  0x03
#define CC1101_SYNC1    0x04
#define CC1101_SYNC0    0x05
#define CC1101_PKTLEN   0x06
#define CC1101_PKTCTRL1 0x07
#define CC1101_PKTCTRL0 0x08
#define CC1101_ADDR     0x09
#define CC1101_CHANNR   0x0A
#define CC1101_FSCTRL1  0x0B
#define CC1101_FSCTRL0  0x0C
#define CC1101_FREQ2    0x0D
#define CC1101_FREQ1    0x0E
#define CC1101_FREQ0    0x0F
#define CC1101_MDMCFG4  0x10
#define CC1101_MDMCFG3  0x11
#define CC1101_MDMCFG2  0x12
#define CC1101_MDMCFG1  0x13
#define CC1101_MDMCFG0  0x14
#define CC1101_DEVIATN  0x15
#define CC1101_MCSM2    0x16
#define CC1101_MCSM1    0x17
#define CC1101_MCSM0    0x18
#define CC1101_FOCCFG   0x19
#define CC1101_BSCFG    0x1A
#define CC1101_AGCCTRL2 0x1B
#define CC1101_AGCCTRL1 0x1C
#define CC1101_AGCCTRL0 0x1D
#define CC1101_WOREVT1  0x1E
#define CC1101_WOREVT0  0x1F
#define CC1101_WORCTRL  0x20
#define CC1101_FREND1   0x21
#define CC1101_FREND0   0x22
#define CC1101_FSCAL3   0x23
#define CC1101_FSCAL2   0x24
#define CC1101_FSCAL1   0x25
#define CC1101_FSCAL0   0x26
#define CC1101_RCCTRL1  0x27
#define CC1101_RCCTRL0  0x28
#define CC1101_FSTEST   0x29
#define CC1101_PTEST    0x2A
#define CC1101_AGCTEST  0x2B
#define CC1101_TEST2    0x2C
#define CC1101_TEST1    0x2D
#define CC1101_TEST0    0x2E

#define CC1101_STROBE_SRES 0x30
#define CC1101_STROBE_SFSTXON \
    0x31
#define CC1101_STROBE_SXOFF 0x32
#define CC1101_STROBE_SCAL \
    0x33
#define CC1101_STROBE_SRX \
    0x34
#define CC1101_STROBE_STX \
    0x35
#define CC1101_STROBE_SIDLE \
    0x36
#define CC1101_STROBE_SWOR \
    0x38

#define CC1101_STROBE_SPWD 0x39
#define CC1101_STROBE_SFRX \
    0x3A
#define CC1101_STROBE_SFTX \
    0x3B
#define CC1101_STROBE_SWORRST 0x3C
#define CC1101_STROBE_SNOP \
    0x3D

#define CC1101_STATUS_PARTNUM    0x30
#define CC1101_STATUS_VERSION    0x31
#define CC1101_STATUS_FREQEST    0x32
#define CC1101_STATUS_LQI        0x33
#define CC1101_STATUS_RSSI       0x34
#define CC1101_STATUS_MARCSTATE  0x35
#define CC1101_STATUS_WORTIME1   0x36
#define CC1101_STATUS_WORTIME0   0x37
#define CC1101_STATUS_PKTSTATUS  0x38
#define CC1101_STATUS_VCO_VC_DAC 0x39
#define CC1101_STATUS_TXBYTES \
    0x3A
#define CC1101_STATUS_RXBYTES \
    0x3B
#define CC1101_STATUS_RCCTRL1_STATUS 0x3C
#define CC1101_STATUS_RCCTRL0_STATUS 0x3D

#define CC1101_PATABLE \
    0x3E
#define CC1101_FIFO \
    0x3F
#define CC1101_IOCFG_INV (1 << 6)

typedef enum {
    CC1101IocfgRxFifoThreshold = 0x00,
    CC1101IocfgRxFifoThresholdOrPacket = 0x01,
    CC1101IocfgTxFifoThreshold = 0x02,
    CC1101IocfgTxFifoFull = 0x03,
    CC1101IocfgRxOverflow = 0x04,
    CC1101IocfgTxUnderflow = 0x05,
    CC1101IocfgSyncWord = 0x06,
    CC1101IocfgPacket = 0x07,
    CC1101IocfgPreamble = 0x08,
    CC1101IocfgClearChannel = 0x09,
    CC1101IocfgLockDetector = 0x0A,
    CC1101IocfgSerialClock = 0x0B,
    CC1101IocfgSerialSynchronousDataOutput = 0x0C,
    CC1101IocfgSerialDataOutput = 0x0D,
    CC1101IocfgCarrierSense = 0x0E,
    CC1101IocfgCrcOk = 0x0F,

    CC1101IocfgRxHardData1 = 0x16,
    CC1101IocfgRxHardData0 = 0x17,

    CC1101IocfgPaPd = 0x1B,
    CC1101IocfgLnaPd = 0x1C,
    CC1101IocfgRxSymbolTick = 0x1D,

    CC1101IocfgWorEvnt0 = 0x24,
    CC1101IocfgWorEvnt1 = 0x25,
    CC1101IocfgClk256 = 0x26,
    CC1101IocfgClk32k = 0x27,

    CC1101IocfgChpRdyN = 0x29,

    CC1101IocfgXoscStable = 0x2B,

    CC1101IocfgHighImpedance = 0x2E,
    CC1101IocfgHW = 0x2F,

    CC1101IocfgClkXosc1 = 0x30,
    CC1101IocfgClkXosc1_5 = 0x31,
    CC1101IocfgClkXosc2 = 0x32,
    CC1101IocfgClkXosc3 = 0x33,
    CC1101IocfgClkXosc4 = 0x34,
    CC1101IocfgClkXosc6 = 0x35,
    CC1101IocfgClkXosc8 = 0x36,
    CC1101IocfgClkXosc12 = 0x37,
    CC1101IocfgClkXosc16 = 0x38,
    CC1101IocfgClkXosc24 = 0x39,
    CC1101IocfgClkXosc32 = 0x3A,
    CC1101IocfgClkXosc48 = 0x3B,
    CC1101IocfgClkXosc64 = 0x3C,
    CC1101IocfgClkXosc96 = 0x3D,
    CC1101IocfgClkXosc128 = 0x3E,
    CC1101IocfgClkXosc192 = 0x3F,
} CC1101Iocfg;

typedef enum {
    CC1101StateIDLE = 0b000,
    CC1101StateRX = 0b001,
    CC1101StateTX = 0b010,
    CC1101StateFSTXON = 0b011,
    CC1101StateCALIBRATE = 0b100,
    CC1101StateSETTLING = 0b101,
    CC1101StateRXFIFO_OVERFLOW =
        0b110,
    CC1101StateTXFIFO_UNDERFLOW = 0b111,
} CC1101State;

typedef struct {
    uint8_t FIFO_BYTES_AVAILABLE : 4;
    CC1101State STATE            : 3;
    bool CHIP_RDYn               : 1;
} CC1101Status;

typedef union {
    CC1101Status status;
    uint8_t status_raw;
} CC1101StatusRaw;

typedef struct {
    uint8_t NUM_TXBYTES   : 7;
    bool TXFIFO_UNDERFLOW : 1;
} CC1101TxBytes;

typedef struct {
    uint8_t NUM_RXBYTES  : 7;
    bool RXFIFO_OVERFLOW : 1;
} CC1101RxBytes;

#ifdef __cplusplus
}
#endif
