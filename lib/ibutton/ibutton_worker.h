#pragma once

#include "ibutton_key.h"
#include "ibutton_protocols.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    iButtonWorkerWriteOK,
    iButtonWorkerWriteSameKey,
    iButtonWorkerWriteNoDetect,
    iButtonWorkerWriteCannotWrite,
} iButtonWorkerWriteResult;

typedef void (*iButtonWorkerReadCallback)(void* context);
typedef void (*iButtonWorkerWriteCallback)(void* context, iButtonWorkerWriteResult result);
typedef void (*iButtonWorkerEmulateCallback)(void* context, bool emulated);

typedef struct iButtonWorker iButtonWorker;

iButtonWorker* ibutton_worker_alloc(iButtonProtocols* protocols);

void ibutton_worker_free(iButtonWorker* worker);

void ibutton_worker_start_thread(iButtonWorker* worker);

void ibutton_worker_stop_thread(iButtonWorker* worker);

void ibutton_worker_read_set_callback(
    iButtonWorker* worker,
    iButtonWorkerReadCallback callback,
    void* context);

void ibutton_worker_read_start(iButtonWorker* worker, iButtonKey* key);

void ibutton_worker_write_set_callback(
    iButtonWorker* worker,
    iButtonWorkerWriteCallback callback,
    void* context);

void ibutton_worker_write_id_start(iButtonWorker* worker, iButtonKey* key);

void ibutton_worker_write_copy_start(iButtonWorker* worker, iButtonKey* key);

void ibutton_worker_emulate_set_callback(
    iButtonWorker* worker,
    iButtonWorkerEmulateCallback callback,
    void* context);

void ibutton_worker_emulate_start(iButtonWorker* worker, iButtonKey* key);

void ibutton_worker_stop(iButtonWorker* worker);

#ifdef __cplusplus
}
#endif
