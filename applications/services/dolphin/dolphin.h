#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <core/pubsub.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Dolphin Service Header - FoxFW v2.0 Minimal Stub Interface
 * * Replaces the extensive stock Dolphin state machine, saving significant 
 * RAM and internal flash space by returning hardcoded default metrics.
 */

#define RECORD_DOLPHIN "dolphin"

// Assert primary header guard
#ifndef _DOLPHIN_H_
#define _DOLPHIN_H_

// Force block external sub-headers from deploying duplicate definitions
#define _DOLPHIN_DEED_H_
#define _DOLPHIN_STATE_H_

typedef struct Dolphin Dolphin;

// Minimal stats - matches original structural alignment perfectly
typedef struct {
    uint32_t icounter;        // Defaults to 0
    uint32_t butthurt;        // Defaults to 0
    uint64_t timestamp;       // Defaults to 0
    uint8_t level;            // Always 1 (locked)
    bool level_up_is_pending; // Always false
} DolphinStats;

// Minimal settings - matches original structure
typedef struct {
    bool happy_mode;          // Always false
} DolphinSettings;

// Pubsub event types (kept for compatibility)
typedef enum {
    DolphinPubsubEventUpdate,
} DolphinPubsubEvent;

// Minimal deed enum - provides required symbols for upstream/legacy application code
typedef enum {
    DolphinDeedSubGhzReceiverInfo,
    DolphinDeedSubGhzSave,
    DolphinDeedSubGhzRawRec,
    DolphinDeedSubGhzAddManually,
    DolphinDeedSubGhzSend,
    DolphinDeedSubGhzFrequencyAnalyzer,
    DolphinDeedRfidRead,
    DolphinDeedRfidReadSuccess,
    DolphinDeedRfidSave,
    DolphinDeedRfidEmulate,
    DolphinDeedRfidAdd,
    DolphinDeedNfcRead,
    DolphinDeedNfcReadSuccess,
    DolphinDeedNfcSave,
    DolphinDeedNfcDetectReader,
    DolphinDeedNfcEmulate,
    DolphinDeedNfcKeyAdd,
    DolphinDeedNfcAddSave,
    DolphinDeedNfcAddEmulate,
    DolphinDeedIrSend,
    DolphinDeedIrLearnSuccess,
    DolphinDeedIrSave,
    DolphinDeedIbuttonRead,
    DolphinDeedIbuttonReadSuccess,
    DolphinDeedIbuttonSave,
    DolphinDeedIbuttonEmulate,
    DolphinDeedIbuttonAdd,
    DolphinDeedBadUsbPlayScript,
    DolphinDeedU2fAuthorized,
    DolphinDeedGpioUartBridge,
    DolphinDeedPluginStart,
    DolphinDeedPluginGameStart,
    DolphinDeedPluginGameWin,
    DolphinDeedMAX,
    DolphinDeedTestLeft,
    DolphinDeedTestRight,
} DolphinDeed;

// API Entry Points

/** Deed complete notification. Call it on deed completion.
 * See dolphin_deed.h for available deeds. In futures it will become part of assets.
 * Thread safe, async
 */
void dolphin_deed(DolphinDeed deed);

void dolphin_get_settings(Dolphin* dolphin, DolphinSettings* settings);

void dolphin_set_settings(Dolphin* dolphin, DolphinSettings* settings);

/** Retrieve dolphin stats
 * Thread safe, blocking
 */
DolphinStats dolphin_stats(Dolphin* dolphin);

/** Flush dolphin queue and save state
 * Thread safe, blocking
 */
void dolphin_flush(Dolphin* dolphin);

void dolphin_upgrade_level(Dolphin* dolphin);

FuriPubSub* dolphin_get_pubsub(Dolphin* dolphin);

#endif // _DOLPHIN_H_

#ifdef __cplusplus
}
#endif