#pragma once

#include <furi.h>
#include <toolbox/pipe.h>
#include <lib/flipper_application/flipper_application.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLI_PLUGIN_API_VERSION 1

typedef enum {
    CliCommandFlagDefault = 0,
    CliCommandFlagParallelSafe = (1 << 0),
    CliCommandFlagInsomniaSafe = (1 << 1),
    CliCommandFlagDontAttachStdio = (1 << 2),
    CliCommandFlagUseShellThread =
        (1
         << 3),

    CliCommandFlagExternal = (1 << 4),
} CliCommandFlag;

typedef void (*CliCommandExecuteCallback)(PipeSide* pipe, FuriString* args, void* context);

typedef struct {
    char* name;
    CliCommandExecuteCallback execute_callback;
    CliCommandFlag flags;
    size_t stack_depth;
} CliCommandDescriptor;

typedef struct {
    const char* search_directory;
    const char* fal_prefix;
    const char* appid;
} CliCommandExternalConfig;

bool cli_is_pipe_broken_or_is_etx_next_char(PipeSide* side);

void cli_print_usage(const char* cmd, const char* usage, const char* arg);

bool cli_sleep(PipeSide* side, uint32_t duration_in_ms);

#define CLI_COMMAND_INTERFACE(name, execute_callback, flags, stack_depth, app_id) \
    static const CliCommandDescriptor cli_##name##_desc = {                       \
        #name,                                                                    \
        &execute_callback,                                                        \
        flags,                                                                    \
        stack_depth,                                                              \
    };                                                                            \
                                                                                  \
    static const FlipperAppPluginDescriptor plugin_descriptor = {                 \
        .appid = app_id,                                                          \
        .ep_api_version = CLI_PLUGIN_API_VERSION,                                 \
        .entry_point = &cli_##name##_desc,                                        \
    };                                                                            \
                                                                                  \
    const FlipperAppPluginDescriptor* cli_##name##_ep(void) {                     \
        return &plugin_descriptor;                                                \
    }

#ifdef __cplusplus
}
#endif
