#include <stdint.h>
#include <stdbool.h>

#define DIGITAL_SIGNAL_T_TIM      1562
#define DIGITAL_SIGNAL_T_TIM_DIV2 (DIGITAL_SIGNAL_T_TIM / 2)

struct DigitalSignal {
    bool start_level;
    uint32_t size;
    uint32_t max_size;
    int32_t remainder;
    uint32_t data[];
};
