#pragma once

#include <furi.h>
#include <toolbox/pipe.h>
#include "../cli_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLI_SHELL_STACK_SIZE (4 * 1024U)

typedef struct CliShell CliShell;

typedef void (*CliShellMotd)(void* context);

CliShell* cli_shell_alloc(
    CliShellMotd motd,
    void* context,
    PipeSide* pipe,
    CliRegistry* registry,
    const CliCommandExternalConfig* ext_config);

void cli_shell_free(CliShell* shell);

void cli_shell_start(CliShell* shell);

void cli_shell_join(CliShell* shell);

void cli_shell_set_prompt(CliShell* shell, const char* prompt);

void cli_shell_preregister_builtin_commands(CliRegistry* registry, bool with_external_reload);

#ifdef __cplusplus
}
#endif
