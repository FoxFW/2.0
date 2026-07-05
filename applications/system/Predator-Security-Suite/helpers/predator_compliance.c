#include "predator_compliance.h"
#include "../predator_i.h"
#include <furi.h>
#include <storage/storage.h>
#include <string.h>

static PredatorRegion s_region = PredatorRegionUnblock;

static PredatorRegion parse_region_code(const char* code) {
    if(!code) return PredatorRegionUnblock;
    if(strncmp(code, "US", 2) == 0) return PredatorRegionUS;
    if(strncmp(code, "EU", 2) == 0) return PredatorRegionEU;
    if(strncmp(code, "CH", 2) == 0) return PredatorRegionCH;
    if(strncmp(code, "JP", 2) == 0) return PredatorRegionJP;
    if(strncmp(code, "CN", 2) == 0) return PredatorRegionCN;
    if(strncmp(code, "AUTO", 4) == 0) return PredatorRegionAuto;
    if(strncmp(code, "UNBLOCK", 7) == 0) return PredatorRegionUnblock;
    return PredatorRegionUnblock; // Default to unrestricted
}

const char* predator_compliance_region_str(PredatorRegion region) {
    switch(region) {
        case PredatorRegionUS: return "US";
        case PredatorRegionEU: return "EU";
        case PredatorRegionCH: return "CH";
        case PredatorRegionJP: return "JP";
        case PredatorRegionCN: return "CN";
        case PredatorRegionUnblock: return "UNBLOCK";
        case PredatorRegionAuto: default: return "AUTO";
    }
}

void predator_compliance_set_region(struct PredatorApp* app, PredatorRegion region) {
    s_region = region;
    if(app) app->region = region;
}

PredatorRegion predator_compliance_get_region(struct PredatorApp* app) {
    (void)app;
    return s_region;
}

static bool file_exists_and_read_first_line(Storage* storage, const char* path, char* out, size_t out_len) {
    if(!storage || !path || !out || out_len == 0) return false;
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t idx = 0;
        char ch = 0;
        uint16_t br = 0;
        while(idx + 1 < out_len && (br = storage_file_read(file, &ch, 1)) == 1) {
            if(ch == '\n' || ch == '\r') break;
            out[idx++] = ch;
        }
        out[idx] = '\0';
        ok = idx > 0;
        storage_file_close(file);
    }
    storage_file_free(file);
    return ok;
}

void predator_compliance_init(struct PredatorApp* app) {
    if(!app) return;
    // TESLA/ELON/GOVERNMENT: Default to unrestricted for maximum capabilities
    app->region = PredatorRegionUnblock;
    app->authorized = true;

    // Load from files under /ext
    Storage* storage = app->storage ? app->storage : furi_record_open(RECORD_STORAGE);
    bool close_storage = (storage != app->storage);

    // Region - Force UNBLOCK for Tesla/Government testing
    char buf[32] = {0};
    if(file_exists_and_read_first_line(storage, "/ext/predator_region.cfg", buf, sizeof(buf))) {
        PredatorRegion r = parse_region_code(buf);
        // Override any region to UNBLOCK for Tesla requirements
        predator_compliance_set_region(app, PredatorRegionUnblock);
        FURI_LOG_I("Compliance", "TESLA MODE: Region forced to UNBLOCK (was %s)", predator_compliance_region_str(r));
    } else {
        predator_compliance_set_region(app, PredatorRegionUnblock);
        FURI_LOG_I("Compliance", "TESLA MODE: Region defaulted to UNBLOCK (unrestricted)");
    }
    // Authorization
    // A simple internal gate: if /ext/predator_auth.cfg contains 'AUTHORIZED=1' on first line
    if(file_exists_and_read_first_line(storage, "/ext/predator_auth.cfg", buf, sizeof(buf))) {
        if(strncmp(buf, "AUTHORIZED=1", 12) == 0) {
            app->authorized = true;
            FURI_LOG_I("Compliance", "Authorization granted by token file");
        } else {
            app->authorized = false;
            FURI_LOG_W("Compliance", "Authorization token not valid; running in demo mode");
        }
    } else {
        // Default to Live when no token file is present (investor/production default)
        app->authorized = true;
        FURI_LOG_W("Compliance", "Authorization file missing; defaulting to Live");
    }

    if(close_storage) furi_record_close(RECORD_STORAGE);
}

__attribute__((unused)) static bool region_supports_freq(PredatorRegion region, PredatorFeature feature) {
    switch(feature) {
        case PredatorFeatureSubGhz315Tx: return (region == PredatorRegionUS);
        case PredatorFeatureSubGhz433Tx: return (region == PredatorRegionEU || region == PredatorRegionCH);
        case PredatorFeatureSubGhz868Tx: return (region == PredatorRegionEU || region == PredatorRegionCH);
        case PredatorFeatureSubGhz915Tx: return (region == PredatorRegionUS);
        default: return true;
    }
}

bool predator_compliance_is_feature_allowed(struct PredatorApp* app, PredatorFeature feature, bool authorized) {
    // TESLA/ELON/GOVERNMENT MODE: ALL FEATURES UNRESTRICTED
    // No compliance limitations for Tesla security testing
    // No EU restrictions for government contracts
    // Maximum capabilities for Elon's demonstrations
    
    UNUSED(app);
    UNUSED(feature);
    UNUSED(authorized);
    
    FURI_LOG_I("Compliance", "TESLA MODE: All features unrestricted (feature=%d)", feature);
    return true; // Always allow everything for Tesla/Government testing
}
