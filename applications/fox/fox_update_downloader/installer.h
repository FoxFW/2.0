#pragma once

#include "app.h"

void installer_install_esp32(UpdaterApp* app);

bool installer_install_firmware(UpdaterApp* app, char* error_msg, size_t error_msg_size);
