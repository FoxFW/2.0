#pragma once

#include "subratt_worker.h"

#include <lib/subghz/protocols/base.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/environment.h>
#include <storage/storage.h>
#include <lib/flipper_format/flipper_format_i.h>
#include <toolbox/stream/stream.h>

struct SubRattWorker {
    SubRattWorkerState state;
    volatile bool worker_running;
    volatile bool initiated;
    volatile bool transmit_mode;

    uint64_t step;

    FuriThread* thread;

    SubGhzEnvironment* environment;
    SubGhzProtocolDecoderBase* decoder_result;
    SubGhzTransmitter* transmitter;
    const char* protocol_name;
    uint8_t tx_timeout_ms;
    const SubGhzDevice* radio_device;

    SubRattAttacks attack;
    uint32_t frequency;
    FuriHalSubGhzPreset preset;
    SubRattFileProtocol file;
    uint8_t bits;
    uint32_t te;
    uint8_t repeat;
    uint8_t load_index;
    uint64_t file_key;
    uint64_t max_value;
    uint8_t opencode;
    bool two_bytes;

    Storage* keylog_storage;
    File* keylog_file;
    uint32_t keylog_count;

    bool replay_mode;
    FlipperFormat* replay_format;
    Stream* replay_stream;
    uint32_t replay_index;
    uint32_t replay_total;

    bool random_mode;

    uint32_t last_time_tx_data;

    SubRattWorkerCallback callback;
    void* context;
};

int32_t subratt_worker_thread(void* context);

bool subratt_worker_subghz_transmit(SubRattWorker* instance, FlipperFormat* flipper_format);

void subratt_worker_send_callback(SubRattWorker* instance);
