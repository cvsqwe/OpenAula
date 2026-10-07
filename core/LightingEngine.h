#pragma once

#include <vector>

#include "Color.h"
#include "LightingMode.h"
#include "KeyboardLayout.h"
#include "Layer.h"
#include "SystemSignals.h"


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
//   activeColor - the colour driving single-colour effects (a layer's
//                 own colour when called from composite())
//   sys         - live keystroke / machine state for reactive and system
//                 effects; ignored by everything else
std::vector<Color> computeFrame(
    LightingMode mode,
    double t,
    const std::vector<KeyDef>& keys,
    const std::vector<Color>& baseColors,
    const Color& activeColor,
    const SystemSignals& sys = SystemSignals()
);

// Renders the whole layer stack bottom-to-top into one frame. phases[i]
// is layer i's own animation clock (seconds, already scaled by that
// layer's speed); missing entries count as 0.
std::vector<Color> composite(
    const std::vector<Layer>& layers,
    const std::vector<double>& phases,
    const std::vector<KeyDef>& keys,
    const std::vector<Color>& baseColors,
    const SystemSignals& sys
);

// False for effects whose output never changes on its own (Custom, Off),
// so callers can drop to a slow refresh rate.
bool isAnimated(LightingMode mode);

// Effects that read SystemSignals' machine metrics / keystrokes.
bool usesSystemMetrics(LightingMode mode);
bool usesKeystrokes(LightingMode mode);

// Scales every colour in `frame` by `brightness` (0.0..1.0) in place -
// the final step both the GUI's preview and the values actually sent to
// hardware go through, so what's on screen always matches the board.
void applyBrightness(std::vector<Color>& frame, double brightness);

}
