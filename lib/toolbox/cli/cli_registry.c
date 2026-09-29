#include "cli_registry.h"
#include "cli_registry_i.h"
#include <toolbox/pipe.h>
#include <storage/storage.h>

#define TAG "CliRegistry"

struct CliRegistry {
    CliCommandDict_t commands;
    FuriMutex* mutex;
};

CliRegistry* cli_registry_alloc(void) {
    CliRegistry* registry = malloc(sizeof(CliRegistry));
    CliCommandDict_init(registry->commands);
    registry->mutex = furi_mutex_alloc(FuriMutexTypeRecursive);
    return registry;
}

void cli_registry_free(CliRegistry* registry) {
    furi_check(furi_mutex_acquire(registry->mutex, FuriWaitForever) == FuriStatusOk);
    furi_mutex_free(registry->mutex);
    CliCommandDict_clear(registry->commands);
    free(registry);
}

void cli_registry_add_command(
    CliRegistry* registry,
    const char* name,
    CliCommandFlag flags,
    CliCommandExecuteCallback callback,
    void* context) {
    cli_registry_add_command_ex(
        registry, name, flags, callback, context, CLI_BUILTIN_COMMAND_STACK_SIZE);
}

void cli_registry_add_command_ex(
    CliRegistry* registry,
    const char* name,
    CliCommandFlag flags,
    CliCommandExecuteCallback callback,
    void* context,
    size_t stack_size) {
    furi_check(registry);
    furi_check(name);
    furi_check(callback);

    if(flags & CliCommandFlagUseShellThread) furi_check(!(flags & CliCommandFlagDontAttachStdio));

    FuriString* name_str;
    name_str = furi_string_alloc_set(name);

    furi_check(furi_string_search_char(name_str, ' ') == FURI_STRING_FAILURE);

    CliRegistryCommand command = {
        .context = context,
        .execute_callback = callback,
        .flags = flags,
        .stack_depth = stack_size,
    };

    furi_check(furi_mutex_acquire(registry->mutex, FuriWaitForever) == FuriStatusOk);
    CliCommandDict_set_at(registry->commands, name_str, command);
    furi_check(furi_mutex_release(registry->mutex) == FuriStatusOk);

    furi_string_free(name_str);
}

void cli_registry_delete_command(CliRegistry* registry, const char* name) {
    furi_check(registry);
    FuriString* name_str;
    name_str = furi_string_alloc_set(name);
    furi_string_trim(name_str);

    size_t name_replace;
    do {
        name_replace = furi_string_replace(name_str, " ", "_");
    } while(name_replace != FURI_STRING_FAILURE);

    furi_check(furi_mutex_acquire(registry->mutex, FuriWaitForever) == FuriStatusOk);
    CliCommandDict_erase(registry->commands, name_str);
    furi_check(furi_mutex_release(registry->mutex) == FuriStatusOk);

    furi_string_free(name_str);
}

bool cli_registry_get_command(
    CliRegistry* registry,
    FuriString* command,
    CliRegistryCommand* result) {
    furi_assert(registry);
    furi_check(furi_mutex_acquire(registry->mutex, FuriWaitForever) == FuriStatusOk);
    CliRegistryCommand* data = CliCommandDict_get(registry->commands, command);
    if(data) *result = *data;

    furi_check(furi_mutex_release(registry->mutex) == FuriStatusOk);

    return !!data;
}

void cli_registry_remove_external_commands(CliRegistry* registry) {
    furi_check(registry);
    furi_check(furi_mutex_acquire(registry->mutex, FuriWaitForever) == FuriStatusOk);

    CliCommandDict_t internal_cmds;
    CliCommandDict_init(internal_cmds);
    for
        M_EACH(item, registry->commands, CliCommandDict_t) {
            if(!(item->value.flags & CliCommandFlagExternal))
                CliCommandDict_set_at(internal_cmds, item->key, item->value);
        }
    CliCommandDict_move(registry->commands, internal_cmds);

    furi_check(furi_mutex_release(registry->mutex) == FuriStatusOk);
}

static bool cli_registry_external_commands_unchanged(
    CliRegistry* registry,
    const CliCommandExternalConfig* config,
    Storage* storage,
    size_t* current_out) {
    size_t current = 0;
    for
        M_EACH(item, registry->commands, CliCommandDict_t) {
            if(item->value.flags & CliCommandFlagExternal) current++;
        }
    *current_out = current;

    size_t scanned = 0;
    size_t matched = 0;
    File* plugin_dir = storage_file_alloc(storage);
    if(storage_dir_open(plugin_dir, config->search_directory)) {
        char plugin_filename[64];
        FuriString* plugin_name = furi_string_alloc();
        while(storage_dir_read(plugin_dir, NULL, plugin_filename, sizeof(plugin_filename))) {
            furi_string_set_str(plugin_name, plugin_filename);
            if(!furi_string_end_with_str(plugin_name, ".fal")) continue;
            furi_string_replace_all_str(plugin_name, ".fal", "");
            if(!furi_string_start_with_str(plugin_name, config->fal_prefix)) continue;
            furi_string_replace_at(plugin_name, 0, strlen(config->fal_prefix), "");
            scanned++;
            CliRegistryCommand* existing = CliCommandDict_get(registry->commands, plugin_name);
            if(existing && (existing->flags & CliCommandFlagExternal)) matched++;
        }
        furi_string_free(plugin_name);
    }
    storage_dir_close(plugin_dir);
    storage_file_free(plugin_dir);

    return (scanned == current) && (matched == scanned);
}

void cli_registry_reload_external_commands(
    CliRegistry* registry,
    const CliCommandExternalConfig* config) {
    furi_check(registry);
    furi_check(furi_mutex_acquire(registry->mutex, FuriWaitForever) == FuriStatusOk);
    FURI_LOG_D(TAG, "Reloading ext commands");

    Storage* storage = furi_record_open(RECORD_STORAGE);

    size_t current = 0;
    if(cli_registry_external_commands_unchanged(registry, config, storage, &current)) {
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_D(TAG, "Ext commands unchanged");
        furi_check(furi_mutex_release(registry->mutex) == FuriStatusOk);
        return;
    }

    if(current > 0) {
        cli_registry_remove_external_commands(registry);
    }

    File* plugin_dir = storage_file_alloc(storage);

    if(storage_dir_open(plugin_dir, config->search_directory)) {
        char plugin_filename[64];
        FuriString* plugin_name = furi_string_alloc();

        while(storage_dir_read(plugin_dir, NULL, plugin_filename, sizeof(plugin_filename))) {
            FURI_LOG_T(TAG, "Plugin: %s", plugin_filename);
            furi_string_set_str(plugin_name, plugin_filename);

            if(!furi_string_end_with_str(plugin_name, ".fal")) continue;
            furi_string_replace_all_str(plugin_name, ".fal", "");
            if(!furi_string_start_with_str(plugin_name, config->fal_prefix)) continue;
            furi_string_replace_at(plugin_name, 0, strlen(config->fal_prefix), "");

            CliRegistryCommand command = {
                .context = NULL,
                .execute_callback = NULL,
                .flags = CliCommandFlagExternal,
            };
            CliCommandDict_set_at(registry->commands, plugin_name, command);
        }

        furi_string_free(plugin_name);
    }

    storage_dir_close(plugin_dir);
    storage_file_free(plugin_dir);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_D(TAG, "Done reloading ext commands");
    furi_check(furi_mutex_release(registry->mutex) == FuriStatusOk);
}

void cli_registry_lock(CliRegistry* registry) {
    furi_assert(registry);
    furi_check(furi_mutex_acquire(registry->mutex, FuriWaitForever) == FuriStatusOk);
}

void cli_registry_unlock(CliRegistry* registry) {
    furi_assert(registry);
    furi_mutex_release(registry->mutex);
}

CliCommandDict_t* cli_registry_get_commands(CliRegistry* registry) {
    furi_assert(registry);
    return &registry->commands;
}
