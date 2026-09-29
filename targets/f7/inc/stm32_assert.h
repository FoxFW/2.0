/**
  ******************************************************************************
  * @file    stm32_assert.h
  * @author  MCD Application Team
  * @brief   STM32 assert template file.
  *          This file should be copied to the application folder and renamed
  *          to stm32_assert.h.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#ifndef STM32_ASSERT_H
#define STM32_ASSERT_H

#include <core/check.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef FURI_LL_DEBUG
#define assert_param furi_assert
#else
#define assert_param(__e) \
    do {                  \
        ((void)(__e));    \
    } while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif
