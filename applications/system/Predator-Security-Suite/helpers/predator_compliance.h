#pragma once

#include <stdbool.h>
#include <stdint.h>

// Forward declare core app to avoid circular dependency
struct PredatorApp;

typedef enum {
    PredatorRegionAuto = 0,
    PredatorRegionUS,
    PredatorRegionEU,
    PredatorRegionCH,  // Switzerland
    PredatorRegionJP,
    PredatorRegionCN,
    PredatorRegionUnblock, // Testing-only: bypass compliance checks
} PredatorRegion;

typedef enum {
    PredatorFeatureUnknown = 0,
    PredatorFeatureSubGhz315Tx,
    PredatorFeatureSubGhz433Tx,
    PredatorFeatureSubGhz868Tx,
    PredatorFeatureSubGhz915Tx,
    PredatorFeatureWifiScan,
    PredatorFeatureWifiDeauth,
    PredatorFeatureWifiEvilTwin,
    PredatorFeatureBleScan,
    PredatorFeatureBleSpam,
    PredatorFeatureCarTeslaCharge,
    PredatorFeatureCarJamming,
    PredatorFeatureRfidClone,
    PredatorFeatureRfidBruteforce,
    PredatorFeatureGpsTracker,
    PredatorFeatureWardriving,
} PredatorFeature;

// Initialize compliance engine (detect region, load settings). Safe to call multiple times.
void predator_compliance_init(struct PredatorApp* app);

// Set/get region (manual override)
void predator_compliance_set_region(struct PredatorApp* app, PredatorRegion region);
PredatorRegion predator_compliance_get_region(struct PredatorApp* app);

// Return a short region code string
const char* predator_compliance_region_str(PredatorRegion region);

// Check if a feature is allowed to run live given region and authorization.
// Returns true if live operation is allowed; false means restrict to simulation/demonstration.
bool predator_compliance_is_feature_allowed(struct PredatorApp* app, PredatorFeature feature, bool authorized);
