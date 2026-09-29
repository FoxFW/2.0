#pragma once

#include <furi.h>
#include <toolbox/cli/cli_command.h>
#include <nfc/nfc.h>

typedef void NfcCliActionContext;

typedef NfcCliActionContext* (*NfcCliActionContextAlloc)(Nfc* nfc);

typedef void (*NfcCliActionContextFree)(NfcCliActionContext* action_ctx);

typedef bool (*NfcCliActionContextCanReuse)(NfcCliActionContext* ctx);

typedef void (*NfcCliActionHandlerCallback)(PipeSide* pipe, NfcCliActionContext* ctx);

typedef bool (*NfcCliArgParseCallback)(FuriString* value, NfcCliActionContext* ctx);

typedef struct NfcCliKeyDescriptor NfcCliKeyDescriptor;

typedef struct NfcCliActionDescriptor NfcCliActionDescriptor;

typedef struct NfcCliCommandDescriptor NfcCliCommandDescriptor;
