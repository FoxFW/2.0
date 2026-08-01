#pragma once

typedef enum {
    FoxChillEventSplashDone = 0,
    FoxChillEventMindfulTick = 1,
    FoxChillEventLongWaitTick = 2,
    FoxChillEventMenuSelectBase = 100, // + item index (0..8)
} FoxChillEvent;
