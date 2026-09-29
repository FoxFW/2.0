#pragma once

#include "st25r3916_reg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ST25R3916_IRQ_MASK_ALL  (uint32_t)(0xFFFFFFFFUL)
#define ST25R3916_IRQ_MASK_NONE (uint32_t)(0x00000000UL)

#define ST25R3916_IRQ_MASK_OSC (uint32_t)(0x00000080U)
#define ST25R3916_IRQ_MASK_FWL (uint32_t)(0x00000040U)
#define ST25R3916_IRQ_MASK_RXS (uint32_t)(0x00000020U)
#define ST25R3916_IRQ_MASK_RXE (uint32_t)(0x00000010U)
#define ST25R3916_IRQ_MASK_TXE \
    (uint32_t)(0x00000008U)
#define ST25R3916_IRQ_MASK_COL (uint32_t)(0x00000004U)
#define ST25R3916_IRQ_MASK_RX_REST \
    (uint32_t)(0x00000002U)
#define ST25R3916_IRQ_MASK_RFU (uint32_t)(0x00000001U)

#define ST25R3916_IRQ_MASK_DCT \
    (uint32_t)(0x00008000U)
#define ST25R3916_IRQ_MASK_NRE \
    (uint32_t)(0x00004000U)
#define ST25R3916_IRQ_MASK_GPE \
    (uint32_t)(0x00002000U)
#define ST25R3916_IRQ_MASK_EON (uint32_t)(0x00001000U)
#define ST25R3916_IRQ_MASK_EOF \
    (uint32_t)(0x00000800U)
#define ST25R3916_IRQ_MASK_CAC \
    (uint32_t)(0x00000400U)
#define ST25R3916_IRQ_MASK_CAT \
    (uint32_t)(0x00000200U)
#define ST25R3916_IRQ_MASK_NFCT \
    (uint32_t)(0x00000100U)

#define ST25R3916_IRQ_MASK_CRC (uint32_t)(0x00800000U)
#define ST25R3916_IRQ_MASK_PAR (uint32_t)(0x00400000U)
#define ST25R3916_IRQ_MASK_ERR2 \
    (uint32_t)(0x00200000U)
#define ST25R3916_IRQ_MASK_ERR1 \
    (uint32_t)(0x00100000U)
#define ST25R3916_IRQ_MASK_WT (uint32_t)(0x00080000U)
#define ST25R3916_IRQ_MASK_WAM \
    (uint32_t)(0x00040000U)
#define ST25R3916_IRQ_MASK_WPH \
    (uint32_t)(0x00020000U)
#define ST25R3916_IRQ_MASK_WCAP \
    (uint32_t)(0x00010000U)

#define ST25R3916_IRQ_MASK_PPON2 \
    (uint32_t)(0x80000000U)
#define ST25R3916_IRQ_MASK_SL_WL \
    (uint32_t)(0x40000000U)
#define ST25R3916_IRQ_MASK_APON \
    (uint32_t)(0x20000000U)
#define ST25R3916_IRQ_MASK_RXE_PTA \
    (uint32_t)(0x10000000U)
#define ST25R3916_IRQ_MASK_WU_F \
    (uint32_t)(0x08000000U)
#define ST25R3916_IRQ_MASK_RFU2 (uint32_t)(0x04000000U)
#define ST25R3916_IRQ_MASK_WU_A_X \
    (uint32_t)(0x02000000U)
#define ST25R3916_IRQ_MASK_WU_A \
    (uint32_t)(0x01000000U)

void st25r3916_mask_irq(const FuriHalSpiBusHandle* handle, uint32_t mask);

uint32_t st25r3916_get_irq(const FuriHalSpiBusHandle* handle);

void st25r3916_write_fifo(const FuriHalSpiBusHandle* handle, const uint8_t* buff, size_t bits);

bool st25r3916_read_fifo(
    const FuriHalSpiBusHandle* handle,
    uint8_t* buff,
    size_t buff_size,
    size_t* buff_bits);

#ifdef __cplusplus
}
#endif
