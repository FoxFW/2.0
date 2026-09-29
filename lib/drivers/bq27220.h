#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <furi_hal_i2c.h>

#define BQ27220_ERROR   0x0
#define BQ27220_SUCCESS 0x1

typedef struct {

    uint8_t BATT_ID : 3;
    bool SNOOZE     : 1;
    bool BCA        : 1;
    bool CCA        : 1;
    uint8_t RSVD0   : 2;

    uint8_t RSVD1;
} Bq27220ControlStatus;

_Static_assert(sizeof(Bq27220ControlStatus) == 2, "Incorrect Bq27220ControlStatus structure size");

typedef struct {

    bool DSG      : 1;
    bool SYSDWN   : 1;
    bool TDA      : 1;
    bool BATTPRES : 1;
    bool AUTH_GD  : 1;
    bool OCVGD    : 1;
    bool TCA      : 1;
    bool RSVD     : 1;

    bool CHGINH   : 1;
    bool FC       : 1;
    bool OTD      : 1;
    bool OTC      : 1;
    bool SLEEP    : 1;
    bool OCVFAIL  : 1;
    bool OCVCOMP  : 1;
    bool FD       : 1;
} Bq27220BatteryStatus;

_Static_assert(sizeof(Bq27220BatteryStatus) == 2, "Incorrect Bq27220BatteryStatus structure size");

typedef enum {
    Bq27220OperationStatusSecSealed = 0b11,
    Bq27220OperationStatusSecUnsealed = 0b10,
    Bq27220OperationStatusSecFull = 0b01,
} Bq27220OperationStatusSec;

typedef struct {

    bool CALMD  : 1;
    uint8_t SEC : 2;
    bool EDV2   : 1;
    bool VDQ : 1;
    bool INITCOMP  : 1;
    bool SMTH      : 1;
    bool BTPINT    : 1;

    uint8_t RSVD1  : 2;
    bool CFGUPDATE : 1;
    uint8_t RSVD0  : 5;
} Bq27220OperationStatus;

_Static_assert(
    sizeof(Bq27220OperationStatus) == 2,
    "Incorrect Bq27220OperationStatus structure size");

typedef struct {

    bool FD       : 1;
    bool FC       : 1;
    bool TD       : 1;
    bool TC       : 1;
    bool RSVD0    : 1;
    bool EDV      : 1;
    bool DSG      : 1;
    bool CF       : 1;

    uint8_t RSVD1 : 2;
    bool FCCX     : 1;
    uint8_t RSVD2 : 2;
    bool EDV1     : 1;
    bool EDV2     : 1;
    bool VDQ      : 1;
} Bq27220GaugingStatus;

_Static_assert(sizeof(Bq27220GaugingStatus) == 2, "Incorrect Bq27220GaugingStatus structure size");

typedef struct BQ27220DMData BQ27220DMData;

bool bq27220_init(const FuriHalI2cBusHandle* handle, const BQ27220DMData* data_memory);

bool bq27220_reset(const FuriHalI2cBusHandle* handle);

bool bq27220_seal(const FuriHalI2cBusHandle* handle);

bool bq27220_unseal(const FuriHalI2cBusHandle* handle);

bool bq27220_full_access(const FuriHalI2cBusHandle* handle);

uint16_t bq27220_get_voltage(const FuriHalI2cBusHandle* handle);

int16_t bq27220_get_current(const FuriHalI2cBusHandle* handle);

bool bq27220_get_control_status(
    const FuriHalI2cBusHandle* handle,
    Bq27220ControlStatus* control_status);

bool bq27220_get_battery_status(
    const FuriHalI2cBusHandle* handle,
    Bq27220BatteryStatus* battery_status);

bool bq27220_get_operation_status(
    const FuriHalI2cBusHandle* handle,
    Bq27220OperationStatus* operation_status);

bool bq27220_get_gauging_status(
    const FuriHalI2cBusHandle* handle,
    Bq27220GaugingStatus* gauging_status);

uint16_t bq27220_get_temperature(const FuriHalI2cBusHandle* handle);

uint16_t bq27220_get_full_charge_capacity(const FuriHalI2cBusHandle* handle);

uint16_t bq27220_get_design_capacity(const FuriHalI2cBusHandle* handle);

uint16_t bq27220_get_remaining_capacity(const FuriHalI2cBusHandle* handle);

uint16_t bq27220_get_state_of_charge(const FuriHalI2cBusHandle* handle);

uint16_t bq27220_get_state_of_health(const FuriHalI2cBusHandle* handle);
