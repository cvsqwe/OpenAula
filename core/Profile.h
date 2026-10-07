#pragma once

#include <string>
#include <vector>

#include "Color.h"
#include "LightingMode.h"
#include "Layer.h"


// One named, saved lighting configuration - what "backlight profiles"
// lets a user flip between (e.g. "Gaming", "Reading", "Streaming").
// Distinct from AppState's live fields (the currently *applied* config,
// which is all the daemon ever reads): activating a profile just copies
// its fields into AppState's live fields the same way any manual edit
// does, so the daemon never needs to know profiles exist at all.
struct Profile
{
    std::string name;

    LightingMode mode = LightingMode::Custom;
    double speed = 1.0;
    double brightness = 1.0;
    Color activeColor{124, 92, 255};
    std::vector<Color> customColors;
    std::vector<Layer> layers;
};
