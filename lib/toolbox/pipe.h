#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>
#include <stddef.h>

typedef enum {
    PipeRoleAlice,
    PipeRoleBob,
} PipeRole;

typedef enum {
    PipeStateOpen,
    PipeStateBroken,
} PipeState;

typedef struct PipeSide PipeSide;

typedef struct {
    PipeSide* alices_side;
    PipeSide* bobs_side;
} PipeSideBundle;

typedef struct {
    size_t capacity;
    size_t trigger_level;
} PipeSideReceiveSettings;

PipeSideBundle pipe_alloc(size_t capacity, size_t trigger_level);

PipeSideBundle pipe_alloc_ex(PipeSideReceiveSettings alice, PipeSideReceiveSettings bob);

PipeRole pipe_role(PipeSide* pipe);

PipeState pipe_state(PipeSide* pipe);

void pipe_free(PipeSide* pipe);

void pipe_install_as_stdio(PipeSide* pipe);

void pipe_set_state_check_period(PipeSide* pipe, FuriWait check_period);

size_t pipe_receive(PipeSide* pipe, void* data, size_t length);

size_t pipe_send(PipeSide* pipe, const void* data, size_t length);

size_t pipe_bytes_available(PipeSide* pipe);

size_t pipe_spaces_available(PipeSide* pipe);

void pipe_attach_to_event_loop(PipeSide* pipe, FuriEventLoop* event_loop);

void pipe_detach_from_event_loop(PipeSide* pipe);

typedef void (*PipeSideDataArrivedCallback)(PipeSide* pipe, void* context);

typedef void (*PipeSideSpaceFreedCallback)(PipeSide* pipe, void* context);

typedef void (*PipeSideBrokenCallback)(PipeSide* pipe, void* context);

void pipe_set_callback_context(PipeSide* pipe, void* context);

void pipe_set_data_arrived_callback(
    PipeSide* pipe,
    PipeSideDataArrivedCallback callback,
    FuriEventLoopEvent event);

void pipe_set_space_freed_callback(
    PipeSide* pipe,
    PipeSideSpaceFreedCallback callback,
    FuriEventLoopEvent event);

void pipe_set_broken_callback(
    PipeSide* pipe,
    PipeSideBrokenCallback callback,
    FuriEventLoopEvent event);

#ifdef __cplusplus
}
#endif
