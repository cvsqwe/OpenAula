#pragma once

#include <vector>

#include "Color.h"
#include "LightingMode.h"
#include "KeyboardLayout.h"
#include "Layer.h"
#include "SystemSignals.h"


// colour math for every effect. web/app.js has a port of this for the preview,
// keep them in sync
namespace LightingEngine
{

// one frame, one colour per key (visual order, not calibrated).
// t = seconds since start, already scaled by speed.
// baseColors = custom per-key colours, activeColor = the layer colour.
std::vector<Color> computeFrame(
    LightingMode mode,
    double t,
    const std::vector<KeyDef>& keys,
    const std::vector<Color>& baseColors,
    const Color& activeColor,
    const SystemSignals& sys = SystemSignals()
);

// whole layer stack, bottom to top. phases[i] = clock of layer i
std::vector<Color> composite(
    const std::vector<Layer>& layers,
    const std::vector<double>& phases,
    const std::vector<KeyDef>& keys,
    const std::vector<Color>& baseColors,
    const SystemSignals& sys
);

// Custom / Off don't change on their own
bool isAnimated(LightingMode mode);

bool usesSystemMetrics(LightingMode mode);
bool usesKeystrokes(LightingMode mode);

// in place, 0..1
void applyBrightness(std::vector<Color>& frame, double brightness);

}
