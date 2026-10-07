#pragma once

#include <string>


// calibration wizard state. separate file from AppState because it's
// temporary - while it's active the daemon lights one LED at a time
// instead of the normal effects.
class CalibrationSession
{
private:

    bool sessionActive = false;
    int currentStep = 0;
    int totalSteps = 0;


public:

    static std::string filePath();

    bool active() const { return sessionActive; }
    int step() const { return currentStep; }
    int total() const { return totalSteps; }

    void start(int totalPhysicalLeds);

    // clamped to [0, total()-1]
    void setStep(int step);

    void stop();

    void load();

    void save() const;

};
