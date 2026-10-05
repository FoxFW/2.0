/*
 * FoxFW: standalone entry point for the externalized MIFARE Fuzzer.
 *
 * This feature used to live inside nfc.fap (the "Fuzzer" item on the NFC start
 * menu). It was moved to its own .fap to cut nfc.fap's resident RAM footprint:
 * nfc.fap now launches this app via the loader and fully exits while it runs.
 * The app owns its NFC HAL instance for the lifetime of the run, exactly as
 * nfc.fap used to provide it to fuzzer_mifare_run().
 */
#include <furi.h>
#include <lib/nfc/nfc.h>
#include "fuzzer.h"

int32_t mifare_fuzzer_app(void* p) {
    UNUSED(p);

    Nfc* nfc = nfc_alloc();
    fuzzer_mifare_run(nfc);
    nfc_free(nfc);

    return 0;
}
