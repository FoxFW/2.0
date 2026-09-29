#include "app_common.h"
#include "app_debug.h"
#include <interface/patterns/ble_thread/tl/tl.h>
#include <interface/patterns/ble_thread/tl/mbox_def.h>
#include <interface/patterns/ble_thread/shci/shci.h>
#include <utilities/dbg_trace.h>
#include <utilities/utilities_common.h>

#include "stm32wbxx_ll_bus.h"
#include "stm32wbxx_ll_pwr.h"

#include <furi_hal.h>

typedef PACKED_STRUCT {
    GPIO_TypeDef* port;
    uint16_t pin;
    uint8_t enable;
    uint8_t reserved;
}
APPD_GpioConfig_t;

#define GPIO_NBR_OF_RF_SIGNALS           9
#define GPIO_CFG_NBR_OF_FEATURES         34
#define NBR_OF_TRACES_CONFIG_PARAMETERS  4
#define NBR_OF_GENERAL_CONFIG_PARAMETERS 4

#define BLE_DTB_CFG  0

#define SYS_DBG_CFG1 (SHCI_C2_DEBUG_OPTIONS_IPCORE_LP | SHCI_C2_DEBUG_OPTIONS_CPU2_STOP_EN)

PLACE_IN_SECTION("MB_MEM2")
ALIGN(4) static SHCI_C2_DEBUG_TracesConfig_t APPD_TracesConfig = {0, 0, 0, 0};
PLACE_IN_SECTION("MB_MEM2")
ALIGN(4)
static SHCI_C2_DEBUG_GeneralConfig_t APPD_GeneralConfig =
    {BLE_DTB_CFG, SYS_DBG_CFG1, {0, 0}, 0, 0, 0, 0, 0};

static const APPD_GpioConfig_t aGpioConfigList[GPIO_CFG_NBR_OF_FEATURES] = {
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_7, 1, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOB, LL_GPIO_PIN_3, 1, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_6, 1, 0},

    {GPIOC, LL_GPIO_PIN_1, 1, 0},

    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_4, 1, 0},
    {GPIOC, LL_GPIO_PIN_0, 1, 0},

    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},

    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
    {GPIOA, LL_GPIO_PIN_0, 0, 0},
};

#if(BLE_DTB_CFG == 7)
static const APPD_GpioConfig_t aRfConfigList[GPIO_NBR_OF_RF_SIGNALS] = {
    {GPIOB, LL_GPIO_PIN_2, 0, 0},
    {GPIOB, LL_GPIO_PIN_7, 0, 0},
    {GPIOA, LL_GPIO_PIN_8, 0, 0},
    {GPIOA, LL_GPIO_PIN_9, 0, 0},
    {GPIOA, LL_GPIO_PIN_10, 0, 0},
    {GPIOA, LL_GPIO_PIN_11, 0, 0},
    {GPIOB, LL_GPIO_PIN_8, 0, 0},
    {GPIOB, LL_GPIO_PIN_11, 0, 0},
    {GPIOB, LL_GPIO_PIN_10, 0, 0},
};
#endif

static void APPD_SetCPU2GpioConfig(void);
static void APPD_BleDtbCfg(void);

void APPD_Init(void) {
    APPD_SetCPU2GpioConfig();
    APPD_BleDtbCfg();
}

void APPD_EnableCPU2(void) {
    SHCI_C2_DEBUG_Init_Cmd_Packet_t DebugCmdPacket = {
        {{0, 0, 0}},
        {(uint8_t*)aGpioConfigList,
         (uint8_t*)&APPD_TracesConfig,
         (uint8_t*)&APPD_GeneralConfig,
         GPIO_CFG_NBR_OF_FEATURES,
         NBR_OF_TRACES_CONFIG_PARAMETERS,
         NBR_OF_GENERAL_CONFIG_PARAMETERS}};

    TL_TRACES_Init();

    SHCI_C2_DEBUG_Init(&DebugCmdPacket);

    return;
}

static void APPD_SetCPU2GpioConfig(void) {
    LL_GPIO_InitTypeDef gpio_config = {0};
    uint8_t local_loop;
    uint16_t gpioa_pin_list;
    uint16_t gpiob_pin_list;
    uint16_t gpioc_pin_list;

    gpioa_pin_list = 0;
    gpiob_pin_list = 0;
    gpioc_pin_list = 0;

    for(local_loop = 0; local_loop < GPIO_CFG_NBR_OF_FEATURES; local_loop++) {
        if(aGpioConfigList[local_loop].enable != 0) {
            switch((uint32_t)aGpioConfigList[local_loop].port) {
            case(uint32_t)GPIOA:
                gpioa_pin_list |= aGpioConfigList[local_loop].pin;
                break;

            case(uint32_t)GPIOB:
                gpiob_pin_list |= aGpioConfigList[local_loop].pin;
                break;

            case(uint32_t)GPIOC:
                gpioc_pin_list |= aGpioConfigList[local_loop].pin;
                break;

            default:
                break;
            }
        }
    }

    gpio_config.Mode = LL_GPIO_MODE_OUTPUT;
    gpio_config.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_config.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpio_config.Pull = LL_GPIO_PULL_NO;

    if(gpioa_pin_list != 0) {
        gpio_config.Pin = gpioa_pin_list;
        LL_C2_AHB2_GRP1_EnableClock(LL_C2_AHB2_GRP1_PERIPH_GPIOA);
        LL_GPIO_Init(GPIOA, &gpio_config);
        LL_GPIO_ResetOutputPin(GPIOA, gpioa_pin_list);
    }

    if(gpiob_pin_list != 0) {
        gpio_config.Pin = gpiob_pin_list;
        LL_C2_AHB2_GRP1_EnableClock(LL_C2_AHB2_GRP1_PERIPH_GPIOB);
        LL_GPIO_Init(GPIOB, &gpio_config);
        LL_GPIO_ResetOutputPin(GPIOB, gpiob_pin_list);
    }

    if(gpioc_pin_list != 0) {
        gpio_config.Pin = gpioc_pin_list;
        LL_C2_AHB2_GRP1_EnableClock(LL_C2_AHB2_GRP1_PERIPH_GPIOC);
        LL_GPIO_Init(GPIOC, &gpio_config);
        LL_GPIO_ResetOutputPin(GPIOC, gpioc_pin_list);
    }
}

static void APPD_BleDtbCfg(void) {
#if(BLE_DTB_CFG != 0)
    LL_GPIO_InitTypeDef gpio_config = {0};
    uint8_t local_loop;
    uint16_t gpioa_pin_list;
    uint16_t gpiob_pin_list;

    gpioa_pin_list = 0;
    gpiob_pin_list = 0;

    for(local_loop = 0; local_loop < GPIO_NBR_OF_RF_SIGNALS; local_loop++) {
        if(aRfConfigList[local_loop].enable != 0) {
            switch((uint32_t)aRfConfigList[local_loop].port) {
            case(uint32_t)GPIOA:
                gpioa_pin_list |= aRfConfigList[local_loop].pin;
                break;
            case(uint32_t)GPIOB:
                gpiob_pin_list |= aRfConfigList[local_loop].pin;
                break;
            default:
                break;
            }
        }
    }

    gpio_config.Mode = LL_GPIO_MODE_ALTERNATE;
    gpio_config.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_config.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpio_config.Pull = LL_GPIO_PULL_NO;
    gpio_config.Alternate = LL_GPIO_AF_6;
    gpio_config.Pin = LL_GPIO_PIN_15 | LL_GPIO_PIN_14 | LL_GPIO_PIN_13;

    if(gpioa_pin_list != 0) {
        gpio_config.Pin = gpioa_pin_list;
        LL_C2_AHB2_GRP1_EnableClock(LL_C2_AHB2_GRP1_PERIPH_GPIOA);
        LL_GPIO_Init(GPIOA, &gpio_config);
    }

    if(gpiob_pin_list != 0) {
        gpio_config.Pin = gpiob_pin_list;
        LL_C2_AHB2_GRP1_EnableClock(LL_C2_AHB2_GRP1_PERIPH_GPIOB);
        LL_GPIO_Init(GPIOB, &gpio_config);
    }
#endif
}
