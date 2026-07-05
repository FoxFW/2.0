#include "predator_icons.h"

// Create a minimal icon implementation
const uint8_t predator_icon_data[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// Array of frame pointers
const uint8_t* const predator_icon_frames[] = {predator_icon_data};

const Icon ICON_PREDATOR = {
    .width = 8,
    .height = 8,
    .frame_count = 1,
    .frame_rate = 0,
    .frames = predator_icon_frames,
};
