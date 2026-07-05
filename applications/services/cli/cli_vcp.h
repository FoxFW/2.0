#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_CLI_VCP "cli_vcp"

typedef struct CliVcp CliVcp;

void cli_vcp_enable(CliVcp* cli_vcp);
void cli_vcp_disable(CliVcp* cli_vcp);

// Session-level lock: keeps USB physically connected but tears down
// the active CLI/RPC shell and blocks new sessions until unlocked.
// Use this for "CLI + RPC" disconnect-on-lock mode.
void cli_vcp_session_lock(CliVcp* cli_vcp);
void cli_vcp_session_unlock(CliVcp* cli_vcp);

#ifdef __cplusplus
}
#endif
