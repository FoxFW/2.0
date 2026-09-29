#include <furi_hal_flash.h>
#include <furi_hal_bt.h>
#include <furi_hal_power.h>
#include <furi_hal_cortex.h>
#include <furi.h>
#include <ble/ble.h>
#include <interface/patterns/ble_thread/shci/shci.h>

#include <stm32wbxx.h>
#include <stm32wbxx_ll_hsem.h>
#include <stm32wb55_linker.h>

#include <hsem_map.h>

#include <FreeRTOS.h>
#include <task.h>

#define TAG "FuriHalFlash"

#ifdef FLASH_OP_DEBUG
#undef FURI_LOG_T
#define FURI_LOG_T(...)
#endif

#define FURI_HAL_CRITICAL_MSG       "Critical flash operation fail"
#define FURI_HAL_FLASH_READ_BLOCK   (8U)
#define FURI_HAL_FLASH_WRITE_BLOCK  (8U)
#define FURI_HAL_FLASH_PAGE_SIZE    (4096U)
#define FURI_HAL_FLASH_CYCLES_COUNT (10000U)
#define FURI_HAL_FLASH_TIMEOUT      (1000U)
#define FURI_HAL_FLASH_KEY1         (0x45670123U)
#define FURI_HAL_FLASH_KEY2         (0xCDEF89ABU)
#define FURI_HAL_FLASH_TOTAL_PAGES  (256U)
#define FURI_HAL_FLASH_SR_ERRORS                                                               \
    (FLASH_SR_OPERR | FLASH_SR_PROGERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | \
     FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR | FLASH_SR_OPTVERR)

#define FURI_HAL_FLASH_OPT_KEY1       (0x08192A3BU)
#define FURI_HAL_FLASH_OPT_KEY2       (0x4C5D6E7FU)
#define FURI_HAL_FLASH_OB_TOTAL_WORDS (0x80 / (sizeof(uint32_t) * 2))

#define FURI_HAL_FLASH_C2_LOCK_TIMEOUT_MS (3000U)

#define IS_ADDR_ALIGNED_64BITS(__VALUE__) (((__VALUE__) & 0x7U) == (0x00UL))
#define IS_FLASH_PROGRAM_ADDRESS(__VALUE__)                                             \
    (((__VALUE__) >= FLASH_BASE) && ((__VALUE__) <= (FLASH_BASE + FLASH_SIZE - 8UL)) && \
     (((__VALUE__) % 8UL) == 0UL))

size_t furi_hal_flash_get_base(void) {
    return FLASH_BASE;
}

size_t furi_hal_flash_get_read_block_size(void) {
    return FURI_HAL_FLASH_READ_BLOCK;
}

size_t furi_hal_flash_get_write_block_size(void) {
    return FURI_HAL_FLASH_WRITE_BLOCK;
}

size_t furi_hal_flash_get_page_size(void) {
    return FURI_HAL_FLASH_PAGE_SIZE;
}

size_t furi_hal_flash_get_cycles_count(void) {
    return FURI_HAL_FLASH_CYCLES_COUNT;
}

const void* furi_hal_flash_get_free_start_address(void) {
    return &__free_flash_start__;
}

const void* furi_hal_flash_get_free_end_address(void) {
    uint32_t sfr_reg_val = READ_REG(FLASH->SFR);
    uint32_t sfsa = (READ_BIT(sfr_reg_val, FLASH_SFR_SFSA) >> FLASH_SFR_SFSA_Pos);
    return (const void*)((sfsa * FURI_HAL_FLASH_PAGE_SIZE) + FLASH_BASE);
}

size_t furi_hal_flash_get_free_page_start_address(void) {
    size_t start = (size_t)furi_hal_flash_get_free_start_address();
    size_t page_start = start - start % FURI_HAL_FLASH_PAGE_SIZE;
    if(page_start != start) {
        page_start += FURI_HAL_FLASH_PAGE_SIZE;
    }
    return page_start;
}

size_t furi_hal_flash_get_free_page_count(void) {
    size_t end = (size_t)furi_hal_flash_get_free_end_address();
    size_t page_start = (size_t)furi_hal_flash_get_free_page_start_address();
    return (end - page_start) / FURI_HAL_FLASH_PAGE_SIZE;
}

void furi_hal_flash_init(void) {

    if(READ_BIT(FLASH->SR, FURI_HAL_FLASH_SR_ERRORS)) {
        FURI_LOG_W(TAG, "FLASH->SR 0x%08lX(Known ERRATA)", FLASH->SR);
        WRITE_REG(FLASH->SR, FURI_HAL_FLASH_SR_ERRORS);
    }
}

static void furi_hal_flash_unlock(void) {

    furi_check(READ_BIT(FLASH->CR, FLASH_CR_LOCK) != 0U);

    WRITE_REG(FLASH->KEYR, FURI_HAL_FLASH_KEY1);
    __ISB();
    WRITE_REG(FLASH->KEYR, FURI_HAL_FLASH_KEY2);

    furi_check(READ_BIT(FLASH->CR, FLASH_CR_LOCK) == 0U);
}

static void furi_hal_flash_lock(void) {

    furi_check(READ_BIT(FLASH->CR, FLASH_CR_LOCK) == 0U);

    SET_BIT(FLASH->CR, FLASH_CR_LOCK);

    furi_check(READ_BIT(FLASH->CR, FLASH_CR_LOCK) != 0U);
}

static void furi_hal_flash_begin_with_core2(bool erase_flag) {
    furi_hal_power_insomnia_enter();

    while(LL_HSEM_1StepLock(HSEM, CFG_HW_FLASH_SEMID) != 0) {
        furi_thread_yield();
    }

    furi_hal_flash_unlock();

    if(erase_flag) SHCI_C2_FLASH_EraseActivity(ERASE_ACTIVITY_ON);

    furi_delay_us(5);

    FuriHalCortexTimer timer = furi_hal_cortex_timer_get(FURI_HAL_FLASH_C2_LOCK_TIMEOUT_MS * 1000);
    while(true) {

        while(LL_FLASH_IsActiveFlag_OperationSuspended()) {
            furi_check(!furi_hal_cortex_timer_is_expired(timer));
            furi_thread_yield();
        };

        taskENTER_CRITICAL();

        if(LL_HSEM_IsSemaphoreLocked(HSEM, CFG_HW_BLOCK_FLASH_REQ_BY_CPU1_SEMID)) {
            taskEXIT_CRITICAL();
            furi_check(!furi_hal_cortex_timer_is_expired(timer));
            furi_thread_yield();
            continue;
        }

        if(LL_HSEM_1StepLock(HSEM, CFG_HW_BLOCK_FLASH_REQ_BY_CPU2_SEMID) != 0) {
            taskEXIT_CRITICAL();
            furi_check(!furi_hal_cortex_timer_is_expired(timer));
            furi_thread_yield();
            continue;
        }

        break;
    }
}

static void furi_hal_flash_begin(bool erase_flag) {

    furi_hal_bt_lock_core2();

    if(furi_hal_bt_is_alive()) {
        furi_hal_flash_begin_with_core2(erase_flag);
    } else {
        furi_hal_flash_unlock();
    }
}

static void furi_hal_flash_end_with_core2(bool erase_flag) {

    LL_HSEM_ReleaseLock(HSEM, CFG_HW_BLOCK_FLASH_REQ_BY_CPU2_SEMID, 0);

    taskEXIT_CRITICAL();

    while(READ_BIT(FLASH->SR, FLASH_SR_BSY)) {
        furi_thread_yield();
    }

    if(erase_flag) SHCI_C2_FLASH_EraseActivity(ERASE_ACTIVITY_OFF);

    furi_hal_flash_lock();

    LL_HSEM_ReleaseLock(HSEM, CFG_HW_FLASH_SEMID, 0);
    furi_hal_power_insomnia_exit();
}

static void furi_hal_flash_end(bool erase_flag) {

    if(furi_hal_bt_is_alive()) {
        furi_hal_flash_end_with_core2(erase_flag);
    } else {
        furi_hal_flash_lock();
    }

    furi_hal_bt_unlock_core2();
}

static void furi_hal_flush_cache(void) {

    if(READ_BIT(FLASH->ACR, FLASH_ACR_ICEN) == FLASH_ACR_ICEN) {

        LL_FLASH_DisableInstCache();

        LL_FLASH_EnableInstCacheReset();
        LL_FLASH_DisableInstCacheReset();

        LL_FLASH_EnableInstCache();
    }

    if(READ_BIT(FLASH->ACR, FLASH_ACR_DCEN) == FLASH_ACR_DCEN) {

        LL_FLASH_DisableDataCache();

        LL_FLASH_EnableDataCacheReset();
        LL_FLASH_DisableDataCacheReset();

        LL_FLASH_EnableDataCache();
    }
}

bool furi_hal_flash_wait_last_operation(uint32_t timeout) {
    uint32_t error = 0;

    FuriHalCortexTimer timer = furi_hal_cortex_timer_get(timeout * 1000);
    while(READ_BIT(FLASH->SR, FLASH_SR_BSY)) {
        if(furi_hal_cortex_timer_is_expired(timer)) {
            return false;
        }
    }

    error = FLASH->SR;

    if((error & FLASH_SR_EOP) != 0U) {

        CLEAR_BIT(FLASH->SR, FLASH_SR_EOP);
    }

    error &= FURI_HAL_FLASH_SR_ERRORS;

    furi_check(error == 0);

    CLEAR_BIT(FLASH->SR, error);

    timer = furi_hal_cortex_timer_get(timeout * 1000);
    while(READ_BIT(FLASH->SR, FLASH_SR_CFGBSY)) {
        if(furi_hal_cortex_timer_is_expired(timer)) {
            return false;
        }
    }
    return true;
}

void furi_hal_flash_erase(uint8_t page) {
    uint32_t op_stat = DWT->CYCCNT;
    furi_hal_flash_begin(true);

    furi_check(FLASH->SR == 0);

    furi_check(furi_hal_flash_wait_last_operation(FURI_HAL_FLASH_TIMEOUT));

    MODIFY_REG(
        FLASH->CR, FLASH_CR_PNB, ((page << FLASH_CR_PNB_Pos) | FLASH_CR_PER | FLASH_CR_STRT));

    furi_check(furi_hal_flash_wait_last_operation(FURI_HAL_FLASH_TIMEOUT));

    CLEAR_BIT(FLASH->CR, (FLASH_CR_PER | FLASH_CR_PNB));

    furi_hal_flush_cache();

    furi_hal_flash_end(true);
    op_stat = DWT->CYCCNT - op_stat;
    FURI_LOG_T(
        TAG,
        "erase took %lu clocks or %luus",
        op_stat,
        op_stat / furi_hal_cortex_instructions_per_microsecond());
}

static inline void furi_hal_flash_write_dword_internal_nowait(size_t address, uint64_t* data) {

    *(uint32_t*)address = (uint32_t)*data;

    __ISB();

    *(uint32_t*)(address + 4U) = (uint32_t)(*data >> 32U);
}

static inline void furi_hal_flash_write_dword_internal(size_t address, uint64_t* data) {
    furi_hal_flash_write_dword_internal_nowait(address, data);

    furi_check(furi_hal_flash_wait_last_operation(FURI_HAL_FLASH_TIMEOUT));
}

void furi_hal_flash_write_dword(size_t address, uint64_t data) {
    uint32_t op_stat = DWT->CYCCNT;
    furi_hal_flash_begin(false);

    furi_check(FLASH->SR == 0);

    furi_check(IS_ADDR_ALIGNED_64BITS(address));
    furi_check(IS_FLASH_PROGRAM_ADDRESS(address));

    SET_BIT(FLASH->CR, FLASH_CR_PG);

    furi_hal_flash_write_dword_internal(address, &data);

    CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

    furi_hal_flash_end(false);

    furi_check(furi_hal_flash_wait_last_operation(FURI_HAL_FLASH_TIMEOUT));
    op_stat = DWT->CYCCNT - op_stat;
    FURI_LOG_T(
        TAG,
        "write_dword took %lu clocks or %fus",
        op_stat,
        (double)((float)op_stat / (float)furi_hal_cortex_instructions_per_microsecond()));
}

static size_t furi_hal_flash_get_page_address(uint8_t page) {
    return furi_hal_flash_get_base() + page * FURI_HAL_FLASH_PAGE_SIZE;
}

void furi_hal_flash_program_page(const uint8_t page, const uint8_t* data, uint16_t _length) {
    uint16_t length = _length;
    furi_check(length <= FURI_HAL_FLASH_PAGE_SIZE);

    furi_hal_flash_erase(page);

    uint32_t op_stat = DWT->CYCCNT;
    furi_hal_flash_begin(false);

    furi_check(furi_hal_flash_wait_last_operation(FURI_HAL_FLASH_TIMEOUT));

    furi_check(FLASH->SR == 0);

    size_t page_start_address = furi_hal_flash_get_page_address(page);

    size_t length_written = 0;

    const uint16_t FAST_PROG_BLOCK_SIZE = 512;
    const uint8_t DWORD_PROG_BLOCK_SIZE = 8;

    if(length >= FAST_PROG_BLOCK_SIZE) {
        taskENTER_CRITICAL();

        SET_BIT(FLASH->CR, FLASH_CR_FSTPG);

        while(length_written < (length / FAST_PROG_BLOCK_SIZE * FAST_PROG_BLOCK_SIZE)) {

            furi_hal_flash_write_dword_internal_nowait(
                page_start_address + length_written, (uint64_t*)(data + length_written));
            length_written += DWORD_PROG_BLOCK_SIZE;

            if((length_written % FAST_PROG_BLOCK_SIZE) == 0) {

                furi_check(furi_hal_flash_wait_last_operation(FURI_HAL_FLASH_TIMEOUT));
            }
        }
        CLEAR_BIT(FLASH->CR, FLASH_CR_FSTPG);
        taskEXIT_CRITICAL();
    }

    SET_BIT(FLASH->CR, FLASH_CR_PG);
    if((length % FAST_PROG_BLOCK_SIZE) != 0) {

        while(length_written < (length / DWORD_PROG_BLOCK_SIZE * DWORD_PROG_BLOCK_SIZE)) {
            furi_hal_flash_write_dword_internal(
                page_start_address + length_written, (uint64_t*)&data[length_written]);
            length_written += DWORD_PROG_BLOCK_SIZE;
        }
    }

    if((length % DWORD_PROG_BLOCK_SIZE) != 0) {

        uint64_t tail_data = 0;
        for(int32_t tail_i = 0; tail_i < (length % DWORD_PROG_BLOCK_SIZE); ++tail_i) {
            tail_data |= (((uint64_t)data[length_written + tail_i]) << (tail_i * 8));
        }

        furi_hal_flash_write_dword_internal(page_start_address + length_written, &tail_data);
    }

    CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

    furi_hal_flash_end(false);
    op_stat = DWT->CYCCNT - op_stat;
    FURI_LOG_T(
        TAG,
        "program_page took %lu clocks or %luus",
        op_stat,
        op_stat / furi_hal_cortex_instructions_per_microsecond());
}

int16_t furi_hal_flash_get_page_number(size_t address) {
    const size_t flash_base = furi_hal_flash_get_base();
    if((address < flash_base) ||
       (address > flash_base + FURI_HAL_FLASH_TOTAL_PAGES * FURI_HAL_FLASH_PAGE_SIZE)) {
        return -1;
    }

    return (address - flash_base) / FURI_HAL_FLASH_PAGE_SIZE;
}

uint32_t furi_hal_flash_ob_get_word(size_t word_idx, bool complementary) {
    furi_check(word_idx <= FURI_HAL_FLASH_OB_TOTAL_WORDS);
    const uint32_t* ob_data = (const uint32_t*)(OPTION_BYTE_BASE);
    size_t raw_word_idx = word_idx * 2;
    if(complementary) {
        raw_word_idx += 1;
    }
    return ob_data[raw_word_idx];
}

void furi_hal_flash_ob_unlock(void) {
    furi_check(READ_BIT(FLASH->CR, FLASH_CR_OPTLOCK) != 0U);
    furi_hal_flash_begin(true);
    WRITE_REG(FLASH->OPTKEYR, FURI_HAL_FLASH_OPT_KEY1);
    __ISB();
    WRITE_REG(FLASH->OPTKEYR, FURI_HAL_FLASH_OPT_KEY2);

    furi_check(READ_BIT(FLASH->CR, FLASH_CR_OPTLOCK) == 0U);
}

void furi_hal_flash_ob_lock(void) {
    furi_check(READ_BIT(FLASH->CR, FLASH_CR_OPTLOCK) == 0U);
    SET_BIT(FLASH->CR, FLASH_CR_OPTLOCK);
    furi_hal_flash_end(true);
    furi_check(READ_BIT(FLASH->CR, FLASH_CR_OPTLOCK) != 0U);
}

typedef enum {
    FuriHalFlashObInvalid,
    FuriHalFlashObRegisterUserRead,
    FuriHalFlashObRegisterPCROP1AStart,
    FuriHalFlashObRegisterPCROP1AEnd,
    FuriHalFlashObRegisterWRPA,
    FuriHalFlashObRegisterWRPB,
    FuriHalFlashObRegisterPCROP1BStart,
    FuriHalFlashObRegisterPCROP1BEnd,
    FuriHalFlashObRegisterIPCCMail,
    FuriHalFlashObRegisterSecureFlash,
    FuriHalFlashObRegisterC2Opts,
} FuriHalFlashObRegister;

typedef struct {
    FuriHalFlashObRegister ob_reg;
    uint32_t* ob_register_address;
} FuriHalFlashObMapping;

#define OB_REG_DEF(INDEX, REG) {.ob_reg = INDEX, .ob_register_address = (uint32_t*)(REG)}

static const FuriHalFlashObMapping furi_hal_flash_ob_reg_map[FURI_HAL_FLASH_OB_TOTAL_WORDS] = {
    OB_REG_DEF(FuriHalFlashObRegisterUserRead, (&FLASH->OPTR)),
    OB_REG_DEF(FuriHalFlashObRegisterPCROP1AStart, (&FLASH->PCROP1ASR)),
    OB_REG_DEF(FuriHalFlashObRegisterPCROP1AEnd, (&FLASH->PCROP1AER)),
    OB_REG_DEF(FuriHalFlashObRegisterWRPA, (&FLASH->WRP1AR)),
    OB_REG_DEF(FuriHalFlashObRegisterWRPB, (&FLASH->WRP1BR)),
    OB_REG_DEF(FuriHalFlashObRegisterPCROP1BStart, (&FLASH->PCROP1BSR)),
    OB_REG_DEF(FuriHalFlashObRegisterPCROP1BEnd, (&FLASH->PCROP1BER)),

    OB_REG_DEF(FuriHalFlashObInvalid, (NULL)),
    OB_REG_DEF(FuriHalFlashObInvalid, (NULL)),
    OB_REG_DEF(FuriHalFlashObInvalid, (NULL)),
    OB_REG_DEF(FuriHalFlashObInvalid, (NULL)),
    OB_REG_DEF(FuriHalFlashObInvalid, (NULL)),
    OB_REG_DEF(FuriHalFlashObInvalid, (NULL)),

    OB_REG_DEF(FuriHalFlashObRegisterIPCCMail, (&FLASH->IPCCBR)),
    OB_REG_DEF(FuriHalFlashObRegisterSecureFlash, (NULL)),
    OB_REG_DEF(FuriHalFlashObRegisterC2Opts, (NULL)),
};
#undef OB_REG_DEF

void furi_hal_flash_ob_apply(void) {
    furi_hal_flash_ob_unlock();

    SET_BIT(FLASH->CR, FLASH_CR_OBL_LAUNCH);
    furi_check(furi_hal_flash_wait_last_operation(FURI_HAL_FLASH_TIMEOUT));
    furi_hal_flash_ob_lock();
}

bool furi_hal_flash_ob_set_word(size_t word_idx, const uint32_t value) {
    furi_check(word_idx < FURI_HAL_FLASH_OB_TOTAL_WORDS);

    const FuriHalFlashObMapping* reg_def = &furi_hal_flash_ob_reg_map[word_idx];
    if(reg_def->ob_register_address == NULL) {
        FURI_LOG_E(TAG, "Attempt to set RO OB word %d", word_idx);
        return false;
    }

    FURI_LOG_W(
        TAG,
        "Setting OB reg %d for word %d (addr 0x%08lX) to 0x%08lX",
        reg_def->ob_reg,
        word_idx,
        (uint32_t)reg_def->ob_register_address,
        value);

    furi_hal_flash_ob_unlock();

    *reg_def->ob_register_address = value;

    furi_check(furi_hal_flash_wait_last_operation(FURI_HAL_FLASH_TIMEOUT));
    while(LL_FLASH_IsActiveFlag_OperationSuspended()) {
        furi_thread_yield();
    };

    SET_BIT(FLASH->CR, FLASH_CR_OPTSTRT);

    furi_check(furi_hal_flash_wait_last_operation(FURI_HAL_FLASH_TIMEOUT));
    furi_hal_flash_ob_lock();
    return true;
}

const FuriHalFlashRawOptionByteData* furi_hal_flash_ob_get_raw_ptr(void) {
    return (const FuriHalFlashRawOptionByteData*)OPTION_BYTE_BASE;
}
