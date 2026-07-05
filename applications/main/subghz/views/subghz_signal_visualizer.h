#pragma once

#include <gui/view.h>
#include "../helpers/subghz_custom_event.h"
#include "../helpers/subghz_txrx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SubGhzSignalVisualizer SubGhzSignalVisualizer;

typedef void (*SubGhzSignalVisualizerCallback)(SubGhzCustomEvent event, void* context);

/**
 * Set the event callback for the signal visualizer view.
 *
 * @param instance   Visualizer instance
 * @param callback   Function to call on view events
 * @param context    Context passed to the callback
 */
void subghz_signal_visualizer_set_callback(
    SubGhzSignalVisualizer* instance,
    SubGhzSignalVisualizerCallback callback,
    void* context);

/**
 * Allocate the signal visualizer view.
 *
 * @param txrx  SubGhzTxRx handle used for RSSI polling and freq/mod info
 * @return      Allocated instance (must be freed with subghz_signal_visualizer_free)
 */
SubGhzSignalVisualizer* subghz_signal_visualizer_alloc(SubGhzTxRx* txrx);

/**
 * Free the signal visualizer view and all its resources.
 *
 * @param instance  Visualizer instance to free
 */
void subghz_signal_visualizer_free(SubGhzSignalVisualizer* instance);

/**
 * Get the underlying Gui View for registration with ViewDispatcher.
 *
 * @param instance  Visualizer instance
 * @return          View pointer
 */
View* subghz_signal_visualizer_get_view(SubGhzSignalVisualizer* instance);

/**
 * Start RSSI sampling and display refresh timers.
 * Call from the scene's on_enter after switching to this view.
 *
 * @param instance  Visualizer instance
 */
void subghz_signal_visualizer_start(SubGhzSignalVisualizer* instance);

/**
 * Stop RSSI sampling and display refresh timers.
 * Call from the scene's on_exit before leaving this view.
 *
 * @param instance  Visualizer instance
 */
void subghz_signal_visualizer_stop(SubGhzSignalVisualizer* instance);

/**
 * Read back the current display mode.
 * 0 = Bar, 1 = Line. No "Classic" line-waveform mode exists.
 * Safe to call from any context; acquires the view model lock internally.
 */
uint32_t subghz_signal_visualizer_get_mode(SubGhzSignalVisualizer* instance);

/**
 * Set the display mode before or after start().
 * mode: 0 = Bar, 1 = Line. No "Classic" line-waveform mode exists.
 */
void subghz_signal_visualizer_set_mode(SubGhzSignalVisualizer* instance, uint32_t mode);

#ifdef __cplusplus
}
#endif
