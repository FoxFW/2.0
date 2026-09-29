#pragma once

#include "event_loop.h"
#include "event_loop_link_i.h"
#include "event_loop_timer_i.h"
#include "event_loop_tick_i.h"
#include "event_loop_thread_flag_interface.h"

#include <m-list.h>
#include <m-bptree.h>
#include <m-i-list.h>

#include "thread.h"
#include "thread_i.h"

struct FuriEventLoopItem {

    FuriEventLoop* owner;

    FuriEventLoopEvent event;
    FuriEventLoopObject* object;
    const FuriEventLoopContract* contract;

    FuriEventLoopEventCallback callback;
    void* callback_context;

    ILIST_INTERFACE(WaitingList, FuriEventLoopItem);
};

ILIST_DEF(WaitingList, FuriEventLoopItem, M_POD_OPLIST)

#define FURI_EVENT_LOOP_TREE_RANK (4)

BPTREE_DEF2(
    FuriEventLoopTree,
    FURI_EVENT_LOOP_TREE_RANK,
    FuriEventLoopObject*,
    M_PTR_OPLIST,
    FuriEventLoopItem*,
    M_PTR_OPLIST)

#define M_OPL_FuriEventLoopTree_t() BPTREE_OPLIST(FuriEventLoopTree, M_POD_OPLIST)

#define FURI_EVENT_LOOP_FLAG_NOTIFY_INDEX (2)

typedef enum {
    FuriEventLoopFlagEvent = (1 << 0),
    FuriEventLoopFlagStop = (1 << 1),
    FuriEventLoopFlagTimer = (1 << 2),
    FuriEventLoopFlagPending = (1 << 3),
    FuriEventLoopFlagThreadFlag = (1 << 4),
} FuriEventLoopFlag;

#define FuriEventLoopFlagAll                                                   \
    (FuriEventLoopFlagEvent | FuriEventLoopFlagStop | FuriEventLoopFlagTimer | \
     FuriEventLoopFlagPending | FuriEventLoopFlagThreadFlag)

typedef enum {
    FuriEventLoopProcessStatusComplete,
    FuriEventLoopProcessStatusIncomplete,
    FuriEventLoopProcessStatusFreeLater,
} FuriEventLoopProcessStatus;

typedef enum {
    FuriEventLoopStateStopped,
    FuriEventLoopStateRunning,
} FuriEventLoopState;

typedef struct {
    FuriEventLoopPendingCallback callback;
    void* context;
} FuriEventLoopPendingQueueItem;

LIST_DUAL_PUSH_DEF(PendingQueue, FuriEventLoopPendingQueueItem, M_POD_OPLIST)

struct FuriEventLoop {

    FuriThreadId thread_id;

    volatile FuriEventLoopState state;
    volatile FuriEventLoopItem* current_item;

    FuriEventLoopTree_t tree;
    WaitingList_t waiting_list;

    TimerList_t timer_list;

    TimerQueue_t timer_queue;

    PendingQueue_t pending_queue;

    FuriEventLoopTick tick;

    bool are_thread_flags_subscribed;
    FuriEventLoopThreadFlagsCallback thread_flags_callback;
    void* thread_flags_callback_context;
};
