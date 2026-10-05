#pragma once

/*
 * FoxFW: MFKey Nonce Collector — standalone external app.
 *
 * This app is the externalized "Extract MFC Keys" (Mfkey32 reader-attack)
 * nonce-collection feature, moved out of the NFC app (nfc.fap) so that nfc.fap
 * no longer carries the detect_reader view + emulation scenes + nonce logger in
 * its resident RAM footprint. The NFC app launches this .fap via the loader
 * (see nfc_run_mfkey_collector in nfc_app.c), exactly like SubGHz launches the
 * Frequency Analyzer: the loading wheel shows instantly, nfc.fap exits, this app
 * runs, and on exit the loader returns to nfc.fap with the menu item re-selected.
 *
 * The collection logic (card emulation, nonce logging, .mfkey32.log save format)
 * is a byte-for-byte copy of the original in-NFC implementation so the saved file
 * is identical and the existing standalone MFKey32 cracker reads it unchanged.
 *
 * Launch arg:
 *   (none)  -> emulate a fresh random MIFARE Classic 4K card (main-menu entry)
 *   <path>  -> load that .nfc card and emulate it (per-card entries; nfc.fap
 *              writes the current card to a temp file and passes its path)
 */

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <loader/loader.h>

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>

#include "views/detect_reader.h"
#include "helpers/mfkey32_logger.h"
#include "scenes/mfkey_collector_scene.h"

/* Paths — must match the NFC app so the shared nonce log and the MFKey32 cracker
 * continue to interoperate byte-for-byte. */
#define NFC_APP_FOLDER                 EXT_PATH("nfc")
#define NFC_APP_MFKEY32_LOGS_FILE_NAME ".mfkey32.log"
#define NFC_APP_MFKEY32_LOGS_FILE_PATH (NFC_APP_FOLDER "/" NFC_APP_MFKEY32_LOGS_FILE_NAME)
#define NFC_MFKEY32_APP_PATH           (EXT_PATH("apps/NFC/mfkey.fap"))

/* Custom event ids — names/values kept identical to the NFC app's so the copied
 * scene bodies compile and behave unchanged. */
typedef enum {
    NfcCustomEventReserved = 100,
    NfcCustomEventViewExit,
    NfcCustomEventWorkerUpdate,
    NfcCustomEventTimerExpired,
} NfcCustomEvent;

/* View ids. */
typedef enum {
    NfcViewWidget,
    NfcViewDetectReader,
} NfcView;

/* App context. The copied scenes reference it as `NfcApp* instance = context;`
 * exactly as in the NFC app; it is a local type in this .fap and never collides
 * with the NFC app's own NfcApp (separate binaries). Only the fields the three
 * scenes + the view + the logger actually use are present. */
typedef struct NfcApp {
    Gui* gui;
    Storage* storage;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Widget* widget;
    DetectReader* detect_reader;

    Nfc* nfc;
    NfcDevice* nfc_device;
    NfcListener* listener;
    Mfkey32Logger* mfkey32_logger;
    FuriTimer* timer;
} NfcApp;

/* Helpers provided by mfkey_collector.c, named to match the NFC app so the
 * copied scene bodies are unchanged. */
void nfc_blink_stop(NfcApp* nfc);

void nfc_app_run_external(NfcApp* nfc, const char* app_path);
