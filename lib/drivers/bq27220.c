#include "bq27220.h"
#include "bq27220_reg.h"
#include "bq27220_data_memory.h"

#include <furi.h>
#include <stdbool.h>

#define TAG "Gauge"

#define BQ27220_ID (0x0220u)

#define BQ27220_MAC_WRITE_DELAY_US (250u)

#define BQ27220_SELECT_DELAY_US (1000u)

#define BQ27220_MAGIC_DELAY_US (5000u)

#define BQ27220_CONFIG_DELAY_US (10000u)

#define BQ27220_CONFIG_APPLY_US (2000000u)

#define BQ27220_TIMEOUT_COMMON_US (2000000u)

#define BQ27220_TIMEOUT_RESET_US (4000000u)

#define BQ27220_TIMEOUT_CYCLE_INTERVAL_US (1000u)

#define BQ27220_TIMEOUT(timeout_us) ((timeout_us) / (BQ27220_TIMEOUT_CYCLE_INTERVAL_US))

#ifdef BQ27220_DEBUG
#define BQ27220_DEBUG_LOG(...) FURI_LOG_D(TAG, ##__VA_ARGS__)
#else
#define BQ27220_DEBUG_LOG(...)
#endif

static inline bool bq27220_read_reg(
    const FuriHalI2cBusHandle* handle,
    uint8_t address,
    uint8_t* buffer,
    size_t buffer_size) {
    return furi_hal_i2c_trx(
        handle, BQ27220_ADDRESS, &address, 1, buffer, buffer_size, BQ27220_I2C_TIMEOUT);
}

static inline bool bq27220_write(
    const FuriHalI2cBusHandle* handle,
    uint8_t address,
    const uint8_t* buffer,
    size_t buffer_size) {
    return furi_hal_i2c_write_mem(
        handle, BQ27220_ADDRESS, address, buffer, buffer_size, BQ27220_I2C_TIMEOUT);
}

static inline bool bq27220_control(const FuriHalI2cBusHandle* handle, uint16_t control) {
    return bq27220_write(handle, CommandControl, (uint8_t*)&control, 2);
}

static uint16_t bq27220_read_word(const FuriHalI2cBusHandle* handle, uint8_t address) {
    uint16_t buf = BQ27220_ERROR;

    if(!bq27220_read_reg(handle, address, (uint8_t*)&buf, 2)) {
        FURI_LOG_E(TAG, "bq27220_read_word failed");
    }

    return buf;
}

static uint8_t bq27220_get_checksum(uint8_t* data, uint16_t len) {
    uint8_t ret = 0;
    for(uint16_t i = 0; i < len; i++) {
        ret += data[i];
    }
    return 0xFF - ret;
}

static bool bq27220_parameter_check(
    const FuriHalI2cBusHandle* handle,
    uint16_t address,
    uint32_t value,
    size_t size,
    bool update) {
    furi_assert(size == 1 || size == 2 || size == 4);
    bool ret = false;
    uint8_t buffer[6] = {0};
    uint8_t old_data[4] = {0};

    do {
        buffer[0] = address & 0xFF;
        buffer[1] = (address >> 8) & 0xFF;

        for(size_t i = 0; i < size; i++) {
            buffer[1 + size - i] = (value >> (i * 8)) & 0xFF;
        }

        if(update) {

            if(!bq27220_write(handle, CommandSelectSubclass, buffer, size + 2)) {
                FURI_LOG_E(TAG, "DM write failed");
                break;
            }

            furi_delay_us(BQ27220_MAC_WRITE_DELAY_US);

            uint8_t checksum = bq27220_get_checksum(buffer, size + 2);

            buffer[0] = checksum;

            buffer[1] = 2 + size + 1 + 1;
            if(!bq27220_write(handle, CommandMACDataSum, buffer, 2)) {
                FURI_LOG_E(TAG, "CRC write failed");
                break;
            }

            furi_delay_us(BQ27220_CONFIG_DELAY_US);
            ret = true;
        } else {
            if(!bq27220_write(handle, CommandSelectSubclass, buffer, 2)) {
                FURI_LOG_E(TAG, "DM SelectSubclass for read failed");
                break;
            }

            furi_delay_us(BQ27220_SELECT_DELAY_US);

            if(!bq27220_read_reg(handle, CommandMACData, old_data, size)) {
                FURI_LOG_E(TAG, "DM read failed");
                break;
            }

            furi_delay_us(BQ27220_SELECT_DELAY_US);

            if(*(uint32_t*)&(old_data[0]) != *(uint32_t*)&(buffer[2])) {
                FURI_LOG_E(
                    TAG,
                    "Data at 0x%04x(%zu): 0x%08lx!=0x%08lx",
                    address,
                    size,
                    *(uint32_t*)&(old_data[0]),
                    *(uint32_t*)&(buffer[2]));
            } else {
                ret = true;
            }
        }
    } while(0);

    return ret;
}

static bool bq27220_data_memory_check(
    const FuriHalI2cBusHandle* handle,
    const BQ27220DMData* data_memory,
    bool update) {
    if(update) {
        const uint16_t cfg_request = Control_ENTER_CFG_UPDATE;
        if(!bq27220_write(
               handle, CommandSelectSubclass, (uint8_t*)&cfg_request, sizeof(cfg_request))) {
            FURI_LOG_E(TAG, "ENTER_CFG_UPDATE command failed");
            return false;
        };

        uint32_t timeout = BQ27220_TIMEOUT(BQ27220_TIMEOUT_COMMON_US);
        Bq27220OperationStatus operation_status;
        while(--timeout > 0) {
            if(!bq27220_get_operation_status(handle, &operation_status)) {
                FURI_LOG_W(TAG, "Failed to get operation status, retries left %lu", timeout);
            } else if(operation_status.CFGUPDATE) {
                break;
            };
            furi_delay_us(BQ27220_TIMEOUT_CYCLE_INTERVAL_US);
        }

        if(timeout == 0) {
            FURI_LOG_E(
                TAG,
                "Enter CFGUPDATE mode failed, CFG %u, SEC %u",
                operation_status.CFGUPDATE,
                operation_status.SEC);
            return false;
        }
        BQ27220_DEBUG_LOG("Cycles left: %lu", timeout);
    }

    bool result = true;
    while(data_memory->type != BQ27220DMTypeEnd) {
        if(data_memory->type == BQ27220DMTypeWait) {
            furi_delay_us(data_memory->value.u32);
        } else if(data_memory->type == BQ27220DMTypeU8) {
            result &= bq27220_parameter_check(
                handle, data_memory->address, data_memory->value.u8, 1, update);
        } else if(data_memory->type == BQ27220DMTypeU16) {
            result &= bq27220_parameter_check(
                handle, data_memory->address, data_memory->value.u16, 2, update);
        } else if(data_memory->type == BQ27220DMTypeU32) {
            result &= bq27220_parameter_check(
                handle, data_memory->address, data_memory->value.u32, 4, update);
        } else if(data_memory->type == BQ27220DMTypeI8) {
            result &= bq27220_parameter_check(
                handle, data_memory->address, data_memory->value.i8, 1, update);
        } else if(data_memory->type == BQ27220DMTypeI16) {
            result &= bq27220_parameter_check(
                handle, data_memory->address, data_memory->value.i16, 2, update);
        } else if(data_memory->type == BQ27220DMTypeI32) {
            result &= bq27220_parameter_check(
                handle, data_memory->address, data_memory->value.i32, 4, update);
        } else if(data_memory->type == BQ27220DMTypeF32) {
            result &= bq27220_parameter_check(
                handle, data_memory->address, data_memory->value.u32, 4, update);
        } else if(data_memory->type == BQ27220DMTypePtr8) {
            result &= bq27220_parameter_check(
                handle, data_memory->address, *(uint8_t*)data_memory->value.u32, 1, update);
        } else if(data_memory->type == BQ27220DMTypePtr16) {
            result &= bq27220_parameter_check(
                handle, data_memory->address, *(uint16_t*)data_memory->value.u32, 2, update);
        } else if(data_memory->type == BQ27220DMTypePtr32) {
            result &= bq27220_parameter_check(
                handle, data_memory->address, *(uint32_t*)data_memory->value.u32, 4, update);
        } else {
            furi_crash("Invalid DM Type");
        }
        data_memory++;
    }

    if(update && result) {
        bq27220_control(handle, Control_EXIT_CFG_UPDATE_REINIT);

        furi_delay_us(BQ27220_CONFIG_APPLY_US);

        uint32_t timeout = BQ27220_TIMEOUT(BQ27220_TIMEOUT_COMMON_US);
        Bq27220OperationStatus operation_status;
        while(--timeout > 0) {
            if(!bq27220_get_operation_status(handle, &operation_status)) {
                FURI_LOG_W(TAG, "Failed to get operation status, retries left %lu", timeout);
            } else if(operation_status.CFGUPDATE != true) {
                break;
            }
            furi_delay_us(BQ27220_TIMEOUT_CYCLE_INTERVAL_US);
        }

        if(timeout == 0) {
            FURI_LOG_E(TAG, "Exit CFGUPDATE mode failed");
            return false;
        }
        BQ27220_DEBUG_LOG("Cycles left: %lu", timeout);
    }

    return result;
}

bool bq27220_init(const FuriHalI2cBusHandle* handle, const BQ27220DMData* data_memory) {
    bool result = false;
    bool reset_and_provisioning_required = false;

    do {

        BQ27220_DEBUG_LOG("Checking device ID");
        if(!bq27220_control(handle, Control_DEVICE_NUMBER)) {
            FURI_LOG_E(TAG, "ID: Device is not responding");
            break;
        };

        furi_delay_us(BQ27220_SELECT_DELAY_US);

        uint16_t data = bq27220_read_word(handle, CommandMACData);
        if(data != BQ27220_ID) {
            FURI_LOG_E(TAG, "Invalid Device Number %04x != 0x0220", data);
            break;
        }

        BQ27220_DEBUG_LOG("Unsealing");
        if(!bq27220_unseal(handle)) {
            break;
        }

        BQ27220_DEBUG_LOG("Checking initialization status");
        Bq27220OperationStatus operation_status;
        if(!bq27220_get_operation_status(handle, &operation_status)) {
            FURI_LOG_E(TAG, "Failed to get operation status");
            break;
        }
        if(!operation_status.INITCOMP || operation_status.CFGUPDATE) {
            FURI_LOG_E(TAG, "Incorrect state, reset needed");
            reset_and_provisioning_required = true;
        }

        BQ27220_DEBUG_LOG("Checking chosen profile");
        Bq27220ControlStatus control_status;
        if(!bq27220_get_control_status(handle, &control_status)) {
            FURI_LOG_E(TAG, "Failed to get control status");
            break;
        }
        if(control_status.BATT_ID != 0) {
            FURI_LOG_E(TAG, "Incorrect profile, reset needed");
            reset_and_provisioning_required = true;
        }

        if(!reset_and_provisioning_required) {
            BQ27220_DEBUG_LOG("Checking data memory");
            if(!bq27220_data_memory_check(handle, data_memory, false)) {
                FURI_LOG_E(TAG, "Incorrect configuration data, reset needed");
                reset_and_provisioning_required = true;
            }
        }

        if(reset_and_provisioning_required) {
            FURI_LOG_W(TAG, "Resetting device");
            if(!bq27220_reset(handle)) {
                FURI_LOG_E(TAG, "Failed to reset device");
                break;
            }

            BQ27220_DEBUG_LOG("Acquiring Full Access");
            if(!bq27220_full_access(handle)) {
                break;
            }

            FURI_LOG_W(TAG, "Updating data memory");
            bq27220_data_memory_check(handle, data_memory, true);
            if(!bq27220_data_memory_check(handle, data_memory, false)) {
                FURI_LOG_E(TAG, "Data memory update failed");
                break;
            }
        }

        BQ27220_DEBUG_LOG("Sealing");
        if(!bq27220_seal(handle)) {
            FURI_LOG_E(TAG, "Seal failed");
            break;
        }

        result = true;
    } while(0);

    return result;
}

bool bq27220_reset(const FuriHalI2cBusHandle* handle) {
    bool result = false;
    do {
        if(!bq27220_control(handle, Control_RESET)) {
            FURI_LOG_E(TAG, "Reset request failed");
            break;
        };

        uint32_t timeout = BQ27220_TIMEOUT(BQ27220_TIMEOUT_RESET_US);
        Bq27220OperationStatus operation_status;
        while(--timeout > 0) {
            if(!bq27220_get_operation_status(handle, &operation_status)) {
                FURI_LOG_W(TAG, "Failed to get operation status, retries left %lu", timeout);
            } else if(operation_status.INITCOMP == true) {
                break;
            };
            furi_delay_us(BQ27220_TIMEOUT_CYCLE_INTERVAL_US);
        }

        if(timeout == 0) {
            FURI_LOG_E(TAG, "INITCOMP timeout after reset");
            break;
        }
        BQ27220_DEBUG_LOG("Cycles left: %lu", timeout);

        result = true;
    } while(0);

    return result;
}

bool bq27220_seal(const FuriHalI2cBusHandle* handle) {
    Bq27220OperationStatus operation_status = {0};
    bool result = false;
    do {
        if(!bq27220_get_operation_status(handle, &operation_status)) {
            FURI_LOG_E(TAG, "Status query failed");
            break;
        }
        if(operation_status.SEC == Bq27220OperationStatusSecSealed) {
            result = true;
            break;
        }

        if(!bq27220_control(handle, Control_SEALED)) {
            FURI_LOG_E(TAG, "Seal request failed");
            break;
        }

        furi_delay_us(BQ27220_SELECT_DELAY_US);

        if(!bq27220_get_operation_status(handle, &operation_status)) {
            FURI_LOG_E(TAG, "Status query failed");
            break;
        }
        if(operation_status.SEC != Bq27220OperationStatusSecSealed) {
            FURI_LOG_E(TAG, "Seal failed");
            break;
        }

        result = true;
    } while(0);

    return result;
}

bool bq27220_unseal(const FuriHalI2cBusHandle* handle) {
    Bq27220OperationStatus operation_status = {0};
    bool result = false;
    do {
        if(!bq27220_get_operation_status(handle, &operation_status)) {
            FURI_LOG_E(TAG, "Status query failed");
            break;
        }
        if(operation_status.SEC != Bq27220OperationStatusSecSealed) {
            result = true;
            break;
        }

        bq27220_control(handle, UnsealKey1);
        furi_delay_us(BQ27220_MAGIC_DELAY_US);
        bq27220_control(handle, UnsealKey2);
        furi_delay_us(BQ27220_MAGIC_DELAY_US);

        if(!bq27220_get_operation_status(handle, &operation_status)) {
            FURI_LOG_E(TAG, "Status query failed");
            break;
        }
        if(operation_status.SEC != Bq27220OperationStatusSecUnsealed) {
            FURI_LOG_E(TAG, "Unseal failed %u", operation_status.SEC);
            break;
        }

        result = true;
    } while(0);

    return result;
}

bool bq27220_full_access(const FuriHalI2cBusHandle* handle) {
    bool result = false;

    do {
        uint32_t timeout = BQ27220_TIMEOUT(BQ27220_TIMEOUT_COMMON_US);
        Bq27220OperationStatus operation_status;
        while(--timeout > 0) {
            if(!bq27220_get_operation_status(handle, &operation_status)) {
                FURI_LOG_W(TAG, "Failed to get operation status, retries left %lu", timeout);
            } else {
                break;
            };
            furi_delay_us(BQ27220_TIMEOUT_CYCLE_INTERVAL_US);
        }

        if(timeout == 0) {
            FURI_LOG_E(TAG, "Failed to get operation status");
            break;
        }
        BQ27220_DEBUG_LOG("Cycles left: %lu", timeout);

        if(operation_status.SEC == Bq27220OperationStatusSecFull) {
            result = true;
            break;
        }

        if(operation_status.SEC != Bq27220OperationStatusSecUnsealed) {
            FURI_LOG_E(TAG, "Not in unsealed state");
            break;
        }

        bq27220_control(handle, FullAccessKey);
        furi_delay_us(BQ27220_MAGIC_DELAY_US);
        bq27220_control(handle, FullAccessKey);
        furi_delay_us(BQ27220_MAGIC_DELAY_US);

        if(!bq27220_get_operation_status(handle, &operation_status)) {
            FURI_LOG_E(TAG, "Status query failed");
            break;
        }
        if(operation_status.SEC != Bq27220OperationStatusSecFull) {
            FURI_LOG_E(TAG, "Full access failed %u", operation_status.SEC);
            break;
        }

        result = true;
    } while(0);

    return result;
}

uint16_t bq27220_get_voltage(const FuriHalI2cBusHandle* handle) {
    return bq27220_read_word(handle, CommandVoltage);
}

int16_t bq27220_get_current(const FuriHalI2cBusHandle* handle) {
    return bq27220_read_word(handle, CommandCurrent);
}

bool bq27220_get_control_status(
    const FuriHalI2cBusHandle* handle,
    Bq27220ControlStatus* control_status) {
    return bq27220_read_reg(handle, CommandControl, (uint8_t*)control_status, 2);
}

bool bq27220_get_battery_status(
    const FuriHalI2cBusHandle* handle,
    Bq27220BatteryStatus* battery_status) {
    return bq27220_read_reg(handle, CommandBatteryStatus, (uint8_t*)battery_status, 2);
}

bool bq27220_get_operation_status(
    const FuriHalI2cBusHandle* handle,
    Bq27220OperationStatus* operation_status) {
    return bq27220_read_reg(handle, CommandOperationStatus, (uint8_t*)operation_status, 2);
}

bool bq27220_get_gauging_status(
    const FuriHalI2cBusHandle* handle,
    Bq27220GaugingStatus* gauging_status) {

    if(!bq27220_control(handle, Control_GAUGING_STATUS)) {
        FURI_LOG_E(TAG, "DM SelectSubclass for read failed");
        return false;
    }

    furi_delay_us(BQ27220_SELECT_DELAY_US);

    return bq27220_read_reg(handle, CommandMACData, (uint8_t*)gauging_status, 2);
}

uint16_t bq27220_get_temperature(const FuriHalI2cBusHandle* handle) {
    return bq27220_read_word(handle, CommandTemperature);
}

uint16_t bq27220_get_full_charge_capacity(const FuriHalI2cBusHandle* handle) {
    return bq27220_read_word(handle, CommandFullChargeCapacity);
}

uint16_t bq27220_get_design_capacity(const FuriHalI2cBusHandle* handle) {
    return bq27220_read_word(handle, CommandDesignCapacity);
}

uint16_t bq27220_get_remaining_capacity(const FuriHalI2cBusHandle* handle) {
    return bq27220_read_word(handle, CommandRemainingCapacity);
}

uint16_t bq27220_get_state_of_charge(const FuriHalI2cBusHandle* handle) {
    return bq27220_read_word(handle, CommandStateOfCharge);
}

uint16_t bq27220_get_state_of_health(const FuriHalI2cBusHandle* handle) {
    return bq27220_read_word(handle, CommandStateOfHealth);
}
