#pragma once

#include <vector>

#include "Color.h"
#include "LightingMode.h"
#include "KeyboardLayout.h"


// Pure per-key colour math for every lighting mode, shared verbatim by the
// GUI (gui/Qt6/MainWindow) and core/daemon/main.cpp so the background
// daemon can continue an animation exactly as the GUI would have, instead
// of re-implementing (and risking drifting from) the same effects twice.
namespace LightingEngine
{

// Computes one animation frame in "visual key index" space (one entry per
// entry in `keys`, same order/size) - callers remap through
// LedCalibration before sending to hardware.
//
//   t           - elapsed seconds since the mode was activated, already
//                 multiplied by the user's speed setting
//   keys        - board layout (for spatial modes: position drives them)
//   baseColors  - the user's saved per-key design; used as-is for Custom,
//                 and as the brightness base for Breathing
//   activeColor - the single accent colour driving Bounce/Wave/Ripple
std::vector<Color> computeFrame(
    LightingMode mode,
    double t,
    const std::vector<KeyDef>& keys,
    const std::vector<Color>& baseColors,
    const Color& activeColor
);

// Scales every colour in `frame` by `brightness` (0.0..1.0) in place -
// the final step both the GUI's preview and the values actually sent to
// hardware go through, so what's on screen always matches the board.
void applyBrightness(std::vector<Color>& frame, double brightness);

}
