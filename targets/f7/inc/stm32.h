#ifndef _STM32_H_
#define _STM32_H_

#define _BMD(reg, msk, val) (reg) = (((reg) & ~(msk)) | (val))

#define _BST(reg, bits)     (reg) = ((reg) | (bits))

#define _BCL(reg, bits)     (reg) = ((reg) & ~(bits))

#define _WBS(reg, bits)     while(((reg) & (bits)) == 0)

#define _WBC(reg, bits)     while(((reg) & (bits)) != 0)

#define _WVL(reg, msk, val) while(((reg) & (msk)) != (val))

#define _BV(bit)            (0x01 << (bit))

#include "stm32wbxx.h"

#endif
