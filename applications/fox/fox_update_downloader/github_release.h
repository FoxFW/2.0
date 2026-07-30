#pragma once

#include "app.h"

bool github_probe(EspAt* esp_at, uint32_t timeout_ms);

bool github_release_check(
    EspAt* esp_at,
    const char* repo,
    bool need_commit,
    bool need_assets,
    ReleaseInfo* info,
    uint32_t timeout_ms,
    UpdaterApp* app);
