#pragma once

#include <string>
#include <vector>

#include "Color.h"
#include "LightingMode.h"


// Single source of truth for everything that needs to survive across runs:
//
//  - the LED calibration map (see the big comment below - nobody has ever
//    published this board's real per-key wiring, so it's learned by
//    watching the physical keyboard once via CalibrationDialog)
//  - the currently active lighting mode, speed, accent colour, and the
//    user's saved per-key custom design
//
// Both the GUI and the background daemon (daemon/main.cpp) read and write
// the same file, so closing the GUI doesn't lose anything the daemon needs
// to keep the backlight doing exactly what was configured.
//
// Deliberately Qt-free (plain files, no QSettings) so the daemon binary
// doesn't need to link Qt at all.
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

    // Name of the profile (see Profile.h/ProfileStore.h) that produced the
    // fields above, purely so the GUI can re-highlight it after a restart.
    // The daemon never reads this - it only cares about the live fields.
    std::string activeProfileName;


public:

    static std::string filePath();


    // --- calibration ---
    // Persists the mapping from "visual key index" (a key's position in
    // KeyboardLayout, i.e. KeyDef::ledIndex) to "physical LED index" (the
    // slot AulaProtocol::setColors() actually understood by the real
    // firmware). There's no OpenRGB entry for this device and the one
    // public reverse-engineering write-up for this exact VID/PID
    // confirmed per-key writes land correctly but never mapped which slot
    // is which key - the only reliable way to learn it is to watch the
    // physical keyboard while lighting one LED at a time.
    // Defaults to the identity mapping, so an uncalibrated app behaves
    // exactly as if every visual key mapped straight to its own slot.

    int physicalIndexFor(int visualIndex) const;

    void setCalibrationMapping(int visualIndex, int physicalIndex);

    void resetCalibration(int keyCount);

    bool isCalibrated() const;


    // --- lighting state ---

    LightingMode mode() const { return currentMode; }
    void setMode(LightingMode m) { currentMode = m; }

    double speed() const { return currentSpeed; }
    void setSpeed(double s) { currentSpeed = s; }

    // 0.0 (off) .. 1.0 (full) - a final scale applied to every colour
    // before it reaches the keyboard (and the on-screen preview, so what
    // you see matches what's sent). Independent of Off mode, which zeroes
    // colours outright rather than dimming them.
    double brightness() const { return currentBrightness; }
    void setBrightness(double b) { currentBrightness = b; }

    Color activeColor() const { return currentActiveColor; }
    void setActiveColor(Color c) { currentActiveColor = c; }

    const std::vector<Color>& customColorsRef() const { return customColors; }
    void setCustomColors(const std::vector<Color>& colors) { customColors = colors; }

    const std::string& activeProfile() const { return activeProfileName; }
    void setActiveProfile(const std::string& name) { activeProfileName = name; }


    // --- persistence ---

    void load(int keyCount);

    void save() const;

};
