#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <lib/subghz/subghz_tx_rx_worker.h>
#include <stdint.h>

// cursor_position 0-5 : six frequency digits (100 MHz down to 1 kHz)
// cursor_position 6   : Modulation (radio preset)
// cursor_position 7   : Attack method (waveform / data pattern)
#define CURSOR_FREQ_DIGITS  6
#define CURSOR_MODULATION   6
#define CURSOR_METHOD       7
#define CURSOR_MAX          7

// Radio presets — what the CC1101 is configured to transmit
#define JAM_MOD_COUNT 5
static const char* const s_mod_names[JAM_MOD_COUNT] = {
    "OOK 650kHz",
    "2FSK 2.38kHz",
    "2FSK 47.6kHz",
    "MSK 99.97Kb/s",
    "GFSK 9.99Kb/s",
};

// Attack methods — what data pattern is sent over the air
#define JAM_METHOD_COUNT 11
static const char* const s_method_names[JAM_METHOD_COUNT] = {
    "Brute 0xFF",
    "Alternating",
    "Sine Wave",
    "Square Wave",
    "Sawtooth",
    "White Noise",
    "Triangle",
    "Chirp",
    "Gaussian",
    "Burst",
    "Pure Random",
};

typedef struct {
    Gui*                gui;
    ViewPort*           view_port;
    FuriMessageQueue*   event_queue;

    uint32_t            frequency;
    uint8_t             cursor_position;  // 0-5: freq, 6: mod, 7: method

    bool                running;     // main loop alive
    bool                tx_active;   // user toggled jamming ON
    bool                tx_running;  // TX thread executing

    uint8_t             mod_idx;     // index into s_mod_names    (radio preset)
    uint8_t             method_idx;  // index into s_method_names (data pattern)

    const SubGhzDevice* device;
    SubGhzTxRxWorker*   subghz_txrx;
    FuriThread*         tx_thread;
} FoxRFJammer;

FoxRFJammer* fox_rf_jammer_alloc(void);
void         fox_rf_jammer_free(FoxRFJammer* app);
int32_t      fox_rf_jammer_app(void* p);
