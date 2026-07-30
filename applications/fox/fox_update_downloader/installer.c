#include "installer.h"
#include "strutil.h"

#include <toolbox/tar/tar_archive.h>

#include <string.h>
#include <stdio.h>

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

    loader_enqueue_launch(app->loader, FOX_ESP_FLASHER_FAP, args, LoaderDeferredLaunchFlagGui);
    view_dispatcher_stop(app->view_dispatcher);
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
    storage_simply_mkdir(app->storage, "/ext/update");
    storage_simply_mkdir(app->storage, pkg_dir);

    TarArchive* archive = tar_archive_alloc(app->storage);
    bool ok = tar_archive_open(archive, app->download_path, TarOpenModeRead);
    if(ok) ok = tar_archive_unpack_to(archive, pkg_dir, NULL);
    tar_archive_free(archive);

    if(!ok) {
        snprintf(error_msg, error_msg_size, "Could not extract update package");
        return false;
    }

    char manifest_path[UPDATER_PATH_LEN] = {0};
    File* dir = storage_file_alloc(app->storage);
    FileInfo info;
    char name[128];
    if(storage_dir_open(dir, pkg_dir)) {
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            size_t len = strlen(name);
            if(len >= 4 && strcmp(name + len - 4, ".fuf") == 0) {
                str_join3(manifest_path, sizeof(manifest_path), pkg_dir, "/", name);
                break;
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    if(manifest_path[0] == '\0') {
        snprintf(error_msg, error_msg_size, "No update manifest found in package");
        return false;
    }

    loader_enqueue_launch(app->loader, "updater_app", manifest_path, LoaderDeferredLaunchFlagGui);
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}
