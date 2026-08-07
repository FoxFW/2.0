#include "installer.h"
#include "strutil.h"

#include <toolbox/tar/tar_archive.h>

#include <string.h>
#include <stdio.h>

static uint32_t count_dir_entries(Storage* storage, const char* dir_path) {
    File* dir = storage_file_alloc(storage);
    FileInfo info;
    char name[128];
    uint32_t count = 0;
    if(storage_dir_open(dir, dir_path)) {
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return count;
}

static bool find_manifest(
    Storage* storage,
    const char* dir_path,
    char* out_path,
    size_t out_path_size) {
    File* dir = storage_file_alloc(storage);
    FileInfo info;
    char name[128];
    bool found = false;

    if(storage_dir_open(dir, dir_path)) {
        while(!found && storage_dir_read(dir, &info, name, sizeof(name))) {
            char child_path[UPDATER_PATH_LEN];
            snprintf(child_path, sizeof(child_path), "%s/%s", dir_path, name);
            if(info.flags & FSF_DIRECTORY) {
                found = find_manifest(storage, child_path, out_path, out_path_size);
                continue;
            }
            size_t len = strlen(name);
            if(len >= 4 && strcmp(name + len - 4, ".fuf") == 0) {
                str_copy(out_path, out_path_size, child_path);
                found = true;
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    return found;
}

typedef struct {
    UpdaterApp* app;
    int32_t processed;
} ExtractProgressCtx;

static bool extract_progress_cb(const char* name, bool is_directory, void* context) {
    ExtractProgressCtx* ctx = context;
    ctx->processed++;

    const char* base = name;
    const char* slash = strrchr(name, '/');
    if(slash && slash[1] != '\0') {
        base = slash + 1;
    }

    furi_mutex_acquire(ctx->app->progress_mutex, FuriWaitForever);
    ctx->app->progress_bytes = (uint32_t)ctx->processed;
    if(!is_directory) {
        snprintf(ctx->app->extract_current_name, UPDATER_STR_LEN, "%s", base);
    }
    furi_mutex_release(ctx->app->progress_mutex);
    return true;
}

void installer_install_esp32(UpdaterApp* app) {
    char boot_path[UPDATER_PATH_LEN];
    char part_path[UPDATER_PATH_LEN];
    char fw_path[UPDATER_PATH_LEN];
    str_join2(boot_path, sizeof(boot_path), app->download_path, "/bootloader.bin");
    str_join2(part_path, sizeof(part_path), app->download_path, "/partitions.bin");
    str_join2(fw_path, sizeof(fw_path), app->download_path, "/firmware.bin");

    char args[3 * UPDATER_PATH_LEN + 64];
    snprintf(
        args,
        sizeof(args),
        "AUTOINSTALL|%u|%s|%s|%s",
        (unsigned)app->board_index,
        boot_path,
        part_path,
        fw_path);

    loader_enqueue_launch(app->loader, FOX_ESP32_FLASHER_FAP, args, LoaderDeferredLaunchFlagGui);
    view_dispatcher_stop(app->view_dispatcher);
}

bool installer_verify_firmware_package(
    Storage* storage,
    const char* tar_path,
    const char* pkg_dir,
    char* manifest_path_out,
    size_t manifest_path_out_size,
    char* error_msg,
    size_t error_msg_size,
    UpdaterApp* progress_app) {
    FileInfo src_info;
    if(storage_common_stat(storage, tar_path, &src_info) != FSE_OK) {
        snprintf(error_msg, error_msg_size, "Download missing: %s", tar_path);
        return false;
    }
    if(src_info.size == 0) {
        snprintf(error_msg, error_msg_size, "Downloaded file is empty");
        return false;
    }

    storage_simply_mkdir(storage, "/ext/update");
    storage_simply_mkdir(storage, pkg_dir);

    TarArchive* archive = tar_archive_alloc(storage);
    bool opened = tar_archive_open(archive, tar_path, TarOpenModeRead);

    ExtractProgressCtx progress_ctx = {.app = progress_app, .processed = 0};
    if(opened && progress_app) {
        int32_t total = tar_archive_get_entries_count(archive);
        furi_mutex_acquire(progress_app->progress_mutex, FuriWaitForever);
        progress_app->progress_bytes = 0;
        progress_app->progress_total = (total > 0) ? (uint32_t)total : 0;
        progress_app->extract_current_name[0] = '\0';
        furi_mutex_release(progress_app->progress_mutex);
        tar_archive_set_file_callback(archive, extract_progress_cb, &progress_ctx);
    }

    bool unpacked = opened && tar_archive_unpack_to(archive, pkg_dir, NULL);
    tar_archive_free(archive);

    if(!opened) {
        snprintf(error_msg, error_msg_size, "Could not open tar (%lu bytes)", (unsigned long)src_info.size);
        return false;
    }
    if(!unpacked) {
        snprintf(error_msg, error_msg_size, "Extraction failed partway through");
        return false;
    }

    manifest_path_out[0] = '\0';
    find_manifest(storage, pkg_dir, manifest_path_out, manifest_path_out_size);

    if(manifest_path_out[0] == '\0') {
        uint32_t entries = count_dir_entries(storage, pkg_dir);
        FURI_LOG_W(
            "FoxUpdater",
            "No manifest found in %s (%lu items extracted)",
            pkg_dir,
            (unsigned long)entries);

        uint32_t current_baud = progress_app ? progress_app->settings.baud : UPDATER_BAUD;
        if(current_baud > UPDATER_BAUD) {
            snprintf(error_msg, error_msg_size, "File Corrupt - Download Again at 115200 Baud");
        } else {
            snprintf(error_msg, error_msg_size, "File Corrupt - Retry with Stronger WiFi");
        }
        return false;
    }
    return true;
}

bool installer_install_firmware(UpdaterApp* app, char* error_msg, size_t error_msg_size) {
    char pkg_name[UPDATER_STR_LEN];
    str_join2(pkg_name, sizeof(pkg_name), "foxfw_", app->release.tag[0] ? app->release.tag : "update");
    for(char* p = pkg_name; *p; p++) {
        bool ok_char = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                       (*p >= '0' && *p <= '9') || *p == '_' || *p == '-';
        if(!ok_char) *p = '_';
    }

    char pkg_dir[UPDATER_PATH_LEN];
    snprintf(pkg_dir, sizeof(pkg_dir), "/ext/update/%s", pkg_name);

    char manifest_path[UPDATER_PATH_LEN] = {0};
    if(!installer_verify_firmware_package(
           app->storage,
           app->download_path,
           pkg_dir,
           manifest_path,
           sizeof(manifest_path),
           error_msg,
           error_msg_size,
           app)) {
        return false;
    }

    loader_enqueue_launch(app->loader, "updater_app", manifest_path, LoaderDeferredLaunchFlagGui);
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}
