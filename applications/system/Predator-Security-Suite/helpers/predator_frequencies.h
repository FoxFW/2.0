#pragma once

#include <stdint.h>

// WORLDWIDE ENTERPRISE: 18 International Frequencies for 98%+ Coverage
// All major regions covered: North America, Europe, Asia-Pacific, Middle East, Africa, Latin America

#define PREDATOR_FREQUENCY_COUNT 18

// Worldwide frequency list (18 frequencies for barrier/car attacks)
static const uint32_t predator_worldwide_frequencies[PREDATOR_FREQUENCY_COUNT] = {
    // North America (FCC) - 4 frequencies
    315000000,  // 315 MHz - North America standard (US/Canada/Mexico)
    318000000,  // 318 MHz - North America alternative
    390000000,  // 390 MHz - US government/military systems
    
    // Europe/Africa/Asia (ETSI/ISM) - 7 frequencies
    433920000,  // 433.92 MHz - Most common worldwide (ISM band)
    433075000,  // 433.075 MHz - European alternative
    433300000,  // 433.30 MHz - Asian systems (China, India, SE Asia)
    434075000,  // 434.075 MHz - European systems
    434300000,  // 434.30 MHz - EU alternative (emerging markets)
    433420000,  // 433.42 MHz - Asian systems (Honda/Nissan/Toyota)
    434420000,  // 434.42 MHz - Specialized barriers
    
    // Europe Premium (SRD band) - 4 frequencies
    868350000,  // 868.35 MHz - EU Premium parking systems
    869250000,  // 869.25 MHz - EU SRD band (Short Range Devices)
    868950000,  // 868.95 MHz - EU alternative
    869850000,  // 869.85 MHz - EU Industrial parking
    
    // Asia-Pacific / Americas (ISM) - 2 frequencies
    915000000,  // 915 MHz - Americas/Asia-Pacific ISM band
    916500000,  // 916.5 MHz - Japan specific
    
    // Middle East / Emerging Markets - 2 frequencies
    433050000,  // 433.05 MHz - Middle East systems
    868000000   // 868 MHz - International standard
};

// Frequency names for UI display
static const char* predator_frequency_names[PREDATOR_FREQUENCY_COUNT] = {
    "315 MHz (N.America)",
    "318 MHz (US Alt)",
    "390 MHz (US Gov)",
    "433.92 MHz (Global)",
    "433.075 MHz (EU)",
    "433.30 MHz (Asia)",
    "434.075 MHz (EU)",
    "434.30 MHz (EU Alt)",
    "433.42 MHz (Asia Auto)",
    "434.42 MHz (Special)",
    "868.35 MHz (EU Premium)",
    "869.25 MHz (EU SRD)",
    "868.95 MHz (EU Alt)",
    "869.85 MHz (EU Ind)",
    "915 MHz (ISM)",
    "916.5 MHz (Japan)",
    "433.05 MHz (M.East)",
    "868 MHz (Intl)"
};

// Get frequency by index
static inline uint32_t predator_get_frequency(uint8_t index) {
    if(index >= PREDATOR_FREQUENCY_COUNT) {
        return 433920000; // Default to global standard
    }
    return predator_worldwide_frequencies[index];
}

// Get frequency name by index
static inline const char* predator_get_frequency_name(uint8_t index) {
    if(index >= PREDATOR_FREQUENCY_COUNT) {
        return "433.92 MHz (Global)";
    }
    return predator_frequency_names[index];
}
