#pragma once

typedef enum {
    // Core Tesla functionality
    SubmenuIndexCarTesla,
    
    // Main menu categories
    SubmenuIndexWifiAttacks,
    SubmenuIndexBluetoothAttacks,
    SubmenuIndexSubGhzAttacks,
    SubmenuIndexCarAttacks,
    SubmenuIndexRfidAttacks,
    SubmenuIndexGpsTracker,
    SubmenuIndexWardriving,
    SubmenuIndexSocialEngineering,
    SubmenuIndexQuickPoc,
    SubmenuIndexModuleStatus,
    SubmenuIndexBoardSelection,
    SubmenuIndexSettings,
    SubmenuIndexAbout,
    
    // WiFi attack specific indexes
    SubmenuIndexWifiScan,
    SubmenuIndexWifiDeauth,
    SubmenuIndexWifiEvilTwin,
    SubmenuIndexWifiHandshake,
    SubmenuIndexWifiPmkid,
    
    // Bluetooth attack specific indexes
    SubmenuIndexBleScan,
    SubmenuIndexBleSpam,
    
    // Car attack specific indexes
    SubmenuIndexCarTestResults,
    SubmenuIndexWalkingOpen,
    SubmenuIndexCarModels,
    SubmenuIndexCarKeyBruteforce,
    SubmenuIndexCarJamming,
    SubmenuIndexCarPassiveOpener,
    
    // SubGHz attack specific indexes
    SubmenuIndexSubGhzJamming,
    SubmenuIndexSubGhzRawSend,
    
    // RFID attack specific indexes
    SubmenuIndexRfidClone,
    SubmenuIndexRfidBruteforce,
    SubmenuIndexRfidFuzzing,
} SubmenuIndex;
