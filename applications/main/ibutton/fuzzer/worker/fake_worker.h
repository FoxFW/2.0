#pragma once

#include <furi.h>

#include "protocol.h"

typedef enum {
    FuzzerWorkerAttackTypeDefaultDict = 0,
    FuzzerWorkerAttackTypeLoadFile,
    FuzzerWorkerAttackTypeLoadFileCustomUids,

    FuzzerWorkerAttackTypeMax,
} FuzzerWorkerAttackType;

typedef enum {
    FuzzerWorkerLoadKeyStateBadFile = -2,
    FuzzerWorkerLoadKeyStateUnsuportedProto,
    FuzzerWorkerLoadKeyStateOk = 0,
    FuzzerWorkerLoadKeyStateDifferentProto,

} FuzzerWorkerLoadKeyState;

typedef void (*FuzzerWorkerUidChagedCallback)(void* context);
typedef void (*FuzzerWorkerEndCallback)(void* context);

typedef struct FuzzerWorker FuzzerWorker;

FuzzerWorker* fuzzer_worker_alloc();

void fuzzer_worker_free(FuzzerWorker* instance);

bool fuzzer_worker_start(FuzzerWorker* instance, uint8_t idle_time, uint8_t emu_time);

void fuzzer_worker_stop(FuzzerWorker* instance);

void fuzzer_worker_start_emulate(FuzzerWorker* instance);

void fuzzer_worker_pause(FuzzerWorker* instance);

bool fuzzer_worker_init_attack_dict(FuzzerWorker* instance, FuzzerProtocolsID protocol_index);

bool fuzzer_worker_init_attack_file_dict(
    FuzzerWorker* instance,
    FuzzerProtocolsID protocol_index,
    FuriString* file_path);

bool fuzzer_worker_init_attack_bf_byte(
    FuzzerWorker* instance,
    FuzzerProtocolsID protocol_index,
    const FuzzerPayload* new_uid,
    uint8_t chusen);

void fuzzer_worker_get_current_key(FuzzerWorker* instance, FuzzerPayload* output_key);

bool fuzzer_worker_next_key(FuzzerWorker* instance);
bool fuzzer_worker_previous_key(FuzzerWorker* instance);

FuzzerWorkerLoadKeyState fuzzer_worker_load_key_from_file(
    FuzzerWorker* instance,
    FuzzerProtocolsID* protocol_index,
    const char* filename);

bool fuzzer_worker_save_key(FuzzerWorker* instance, const char* path);

void fuzzer_worker_set_uid_chaged_callback(
    FuzzerWorker* instance,
    FuzzerWorkerUidChagedCallback callback,
    void* context);

void fuzzer_worker_set_end_callback(
    FuzzerWorker* instance,
    FuzzerWorkerEndCallback callback,
    void* context);
