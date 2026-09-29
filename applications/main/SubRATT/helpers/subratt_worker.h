#pragma once

#include "../subratt_protocols.h"
#include "subratt_radio_device_loader.h"

typedef enum {
    SubRattWorkerStateIDLE,
    SubRattWorkerStateReady,
    SubRattWorkerStateTx,
    SubRattWorkerStateFinished
} SubRattWorkerState;

typedef void (*SubRattWorkerCallback)(void* context, SubRattWorkerState state);

typedef struct SubRattWorker SubRattWorker;

SubRattWorker* subratt_worker_alloc(const SubGhzDevice* radio_device);

void subratt_worker_free(SubRattWorker* instance);

uint64_t subratt_worker_get_step(SubRattWorker* instance);

bool subratt_worker_set_step(SubRattWorker* instance, uint64_t step);

bool subratt_worker_is_running(SubRattWorker* instance);

bool subratt_worker_init_default_attack(
    SubRattWorker* instance,
    SubRattAttacks attack_type,
    uint64_t step,
    const SubRattProtocol* protocol,
    uint8_t repeats);

bool subratt_worker_init_file_attack(
    SubRattWorker* instance,
    uint64_t step,
    uint8_t load_index,
    uint64_t file_key,
    SubRattProtocol* protocol,
    uint8_t repeats,
    bool two_bytes);

bool subratt_worker_init_replay_attack(
    SubRattWorker* instance,
    uint32_t frequency,
    FuriHalSubGhzPreset preset,
    SubRattFileProtocol file,
    uint8_t bits,
    uint32_t te,
    uint8_t repeat,
    uint8_t opencode,
    bool is_file_attack,
    uint8_t load_index,
    uint64_t file_key,
    bool two_bytes,
    uint32_t total_keys,
    const char* file_path);

uint32_t subratt_worker_get_keylog_count(SubRattWorker* instance);

uint32_t subratt_worker_get_replay_index(SubRattWorker* instance);

uint32_t subratt_worker_get_replay_total(SubRattWorker* instance);

bool subratt_worker_start(SubRattWorker* instance);

void subratt_worker_stop(SubRattWorker* instance);

bool subratt_worker_transmit_current_key(SubRattWorker* instance, uint64_t step);

bool subratt_worker_can_manual_transmit(SubRattWorker* instance);

void subratt_worker_set_callback(
    SubRattWorker* instance,
    SubRattWorkerCallback callback,
    void* context);

uint8_t subratt_worker_get_timeout(SubRattWorker* instance);

void subratt_worker_set_timeout(SubRattWorker* instance, uint8_t timeout);

uint8_t subratt_worker_get_repeats(SubRattWorker* instance);

void subratt_worker_set_repeats(SubRattWorker* instance, uint8_t repeats);

uint32_t subratt_worker_get_te(SubRattWorker* instance);

void subratt_worker_set_te(SubRattWorker* instance, uint32_t te);

bool subratt_worker_is_tx_allowed(SubRattWorker* instance, uint32_t value);

void subratt_worker_set_opencode(SubRattWorker* instance, uint8_t opencode);

uint8_t subratt_worker_get_opencode(SubRattWorker* instance);

bool subratt_worker_get_is_pt2262(SubRattWorker* instance);

void subratt_worker_set_random_mode(SubRattWorker* instance, bool random_mode);

bool subratt_worker_get_random_mode(SubRattWorker* instance);
