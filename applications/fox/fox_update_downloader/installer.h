#pragma once

#include "app.h"

void installer_install_esp32(UpdaterApp* app);

bool installer_install_firmware(UpdaterApp* app, char* error_msg, size_t error_msg_size);

bool installer_verify_firmware_package(
    Storage* storage,
    const char* tar_path,
    const char* pkg_dir,
    char* manifest_path_out,
    size_t manifest_path_out_size,
    char* error_msg,
    size_t error_msg_size,
    UpdaterApp* progress_app);
