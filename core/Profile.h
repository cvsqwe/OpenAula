#pragma once

#include <string>
#include <vector>

#include "Color.h"
#include "LightingMode.h"
#include "Layer.h"


// a saved lighting setup. applying it just copies it into AppState,
// the daemon doesn't know profiles exist
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
