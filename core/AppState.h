#pragma once

#include <string>
#include <vector>

#include "Color.h"
#include "LightingMode.h"
#include "Layer.h"


// everything that survives a restart: calibration map, lighting, custom colours.
// daemon and webd both read/write this file.
class AppState
{
private:

    std::vector<int> visualToPhysical;
    bool calibrated = false;

    LightingMode currentMode = LightingMode::Custom;
    double currentSpeed = 1.0;
    double currentBrightness = 1.0;
    Color currentActiveColor{124, 92, 255};
    std::vector<Color> customColors;

    // what the daemon actually renders. mode/speed/active above are only kept
    // for old files (see StateFormat::legacyLayers)
    std::vector<Layer> layerStack;

    // only for highlighting the profile in the ui
    std::string activeProfileName;


public:

    static std::string filePath();


    // --- calibration ---
    // visual key index -> physical LED slot. nobody has documented this board's
    // wiring, so it's learned by lighting one LED at a time (see Settings).
    // identity until calibrated.

    int physicalIndexFor(int visualIndex) const;

    void setCalibrationMapping(int visualIndex, int physicalIndex);

    void resetCalibration(int keyCount);

    bool isCalibrated() const;


    // --- lighting state ---

    LightingMode mode() const { return currentMode; }
    void setMode(LightingMode m) { currentMode = m; }

    double speed() const { return currentSpeed; }
    void setSpeed(double s) { currentSpeed = s; }

    // 0..1, applied last (also in the preview)
    double brightness() const { return currentBrightness; }
    void setBrightness(double b) { currentBrightness = b; }

    Color activeColor() const { return currentActiveColor; }
    void setActiveColor(Color c) { currentActiveColor = c; }

    const std::vector<Color>& customColorsRef() const { return customColors; }
    void setCustomColors(const std::vector<Color>& colors) { customColors = colors; }

    const std::vector<Layer>& layers() const { return layerStack; }
    void setLayers(const std::vector<Layer>& l) { layerStack = l; }

    const std::string& activeProfile() const { return activeProfileName; }
    void setActiveProfile(const std::string& name) { activeProfileName = name; }


    // --- persistence ---

    void load(int keyCount);

    void save() const;

};
