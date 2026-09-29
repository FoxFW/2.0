#pragma once

#include <furi_hal_spi_types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const LL_SPI_InitTypeDef furi_hal_spi_preset_2edge_low_8m;

extern const LL_SPI_InitTypeDef furi_hal_spi_preset_1edge_low_8m;

extern const LL_SPI_InitTypeDef furi_hal_spi_preset_1edge_low_4m;

extern const LL_SPI_InitTypeDef furi_hal_spi_preset_1edge_low_16m;

extern const LL_SPI_InitTypeDef furi_hal_spi_preset_1edge_low_2m;

extern FuriHalSpiBus furi_hal_spi_bus_r;

extern FuriHalSpiBus furi_hal_spi_bus_d;

extern const FuriHalSpiBusHandle furi_hal_spi_bus_handle_subghz;

extern const FuriHalSpiBusHandle furi_hal_spi_bus_handle_nfc;

extern const FuriHalSpiBusHandle furi_hal_spi_bus_handle_external;

extern const FuriHalSpiBusHandle furi_hal_spi_bus_handle_display;

extern const FuriHalSpiBusHandle furi_hal_spi_bus_handle_sd_fast;

extern const FuriHalSpiBusHandle furi_hal_spi_bus_handle_sd_slow;

#ifdef __cplusplus
}
#endif
