#pragma once

#include "app_conf.h"
#include "dbg_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TL_SHCI_CMD_DBG_EN 1
#define TL_SHCI_CMD_DBG_RAW_EN \
    0
#define TL_SHCI_EVT_DBG_EN 1
#define TL_SHCI_EVT_DBG_RAW_EN \
    0

#define TL_HCI_CMD_DBG_EN 1
#define TL_HCI_CMD_DBG_RAW_EN \
    0
#define TL_HCI_EVT_DBG_EN     1
#define TL_HCI_EVT_DBG_RAW_EN 0

#define TL_MM_DBG_EN 1

#if(TL_SHCI_CMD_DBG_EN != 0)
#define TL_SHCI_CMD_DBG_MSG PRINT_MESG_DBG
#define TL_SHCI_CMD_DBG_BUF PRINT_LOG_BUFF_DBG
#else
#define TL_SHCI_CMD_DBG_MSG(...)
#define TL_SHCI_CMD_DBG_BUF(...)
#endif

#if(TL_SHCI_CMD_DBG_RAW_EN != 0)
#define TL_SHCI_CMD_DBG_RAW(_PDATA_, _SIZE_) furi_log_tx(_PDATA_, _SIZE_)
#else
#define TL_SHCI_CMD_DBG_RAW(...)
#endif

#if(TL_SHCI_EVT_DBG_EN != 0)
#define TL_SHCI_EVT_DBG_MSG PRINT_MESG_DBG
#define TL_SHCI_EVT_DBG_BUF PRINT_LOG_BUFF_DBG
#else
#define TL_SHCI_EVT_DBG_MSG(...)
#define TL_SHCI_EVT_DBG_BUF(...)
#endif

#if(TL_SHCI_EVT_DBG_RAW_EN != 0)
#define TL_SHCI_EVT_DBG_RAW(_PDATA_, _SIZE_) furi_log_tx(_PDATA_, _SIZE_)
#else
#define TL_SHCI_EVT_DBG_RAW(...)
#endif

#if(TL_HCI_CMD_DBG_EN != 0)
#define TL_HCI_CMD_DBG_MSG PRINT_MESG_DBG
#define TL_HCI_CMD_DBG_BUF PRINT_LOG_BUFF_DBG
#else
#define TL_HCI_CMD_DBG_MSG(...)
#define TL_HCI_CMD_DBG_BUF(...)
#endif

#if(TL_HCI_CMD_DBG_RAW_EN != 0)
#define TL_HCI_CMD_DBG_RAW(_PDATA_, _SIZE_) furi_log_tx(_PDATA_, _SIZE_)
#else
#define TL_HCI_CMD_DBG_RAW(...)
#endif

#if(TL_HCI_EVT_DBG_EN != 0)
#define TL_HCI_EVT_DBG_MSG PRINT_MESG_DBG
#define TL_HCI_EVT_DBG_BUF PRINT_LOG_BUFF_DBG
#else
#define TL_HCI_EVT_DBG_MSG(...)
#define TL_HCI_EVT_DBG_BUF(...)
#endif

#if(TL_HCI_EVT_DBG_RAW_EN != 0)
#define TL_HCI_EVT_DBG_RAW(_PDATA_, _SIZE_) furi_log_tx(_PDATA_, _SIZE_)
#else
#define TL_HCI_EVT_DBG_RAW(...)
#endif

#if(TL_MM_DBG_EN != 0)
#define TL_MM_DBG_MSG PRINT_MESG_DBG
#else
#define TL_MM_DBG_MSG(...)
#endif

#ifdef __cplusplus
}
#endif
