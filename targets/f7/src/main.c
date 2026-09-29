#include <furi.h>
#include <furi_hal.h>
#include <flipper.h>
#include <alt_boot.h>
#include <update_util/update_operation.h>

#define TAG "Main"

int32_t init_task(void* context) {
    UNUSED(context);

    furi_hal_init();

    flipper_init();

    furi_background();

    return 0;
}

int main(void) {

    furi_init();

    furi_hal_init_early();

    FuriThread* main_thread = furi_thread_alloc_ex("InitSrv", 1024, init_task, NULL);
    furi_thread_set_priority(main_thread, FuriThreadPriorityInit);

#ifdef FURI_RAM_EXEC

    furi_hal_power_insomnia_enter();
    furi_thread_start(main_thread);
#else
    furi_hal_light_sequence("RGB");

    furi_delay_ms(100);

    FuriHalRtcBootMode boot_mode = furi_hal_rtc_get_boot_mode();
    if(boot_mode == FuriHalRtcBootModeDfu || !furi_hal_gpio_read(&gpio_button_left)) {
        furi_hal_light_sequence("rgb WB");
        furi_hal_rtc_set_boot_mode(FuriHalRtcBootModeNormal);
        flipper_boot_dfu_exec();
        furi_hal_power_reset();
    } else if(boot_mode == FuriHalRtcBootModeUpdate) {
        furi_hal_light_sequence("rgb BR");

        flipper_boot_update_exec();

        furi_hal_rtc_set_boot_mode(FuriHalRtcBootModeNormal);
        furi_hal_power_reset();
    } else if(!furi_hal_gpio_read(&gpio_button_up)) {
        furi_hal_light_sequence("rgb WR");
        flipper_boot_recovery_exec();
        furi_hal_power_reset();
    } else {
        furi_hal_light_sequence("rgb G");
        furi_thread_start(main_thread);
    }
#endif

    furi_run();

    furi_crash("Kernel is Dead");
}

void Error_Handler(void) {
    furi_crash("ErrorHandler");
}

void abort(void) {
    furi_crash("AbortHandler");
}
