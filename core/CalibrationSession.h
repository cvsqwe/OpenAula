#pragma once

#include <string>


// A tiny, separate piece of persisted state for the physical-LED
// calibration wizard (see the web app's Settings panel) - kept out of
// AppState entirely because it's a transient UI session, not part of the
// user's saved lighting config, and because openaula-daemon needs to
// react to it completely differently: while a session is active, the
// daemon stops running the normal effect loop and instead lights exactly
// one physical LED at a time so the user can identify which key it is.
//
// Same file-is-the-source-of-truth pattern as AppState/RemapConfig: the
// web bridge (which never touches the HID device) writes this file and
// nudges the daemon (SIGUSR1, same as any other lighting change); the
// daemon (which owns the one live HID connection) reads it and acts.
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

    // Begins a session over `totalPhysicalLeds` steps, starting at LED 0.
    void start(int totalPhysicalLeds);

    // Moves to a specific step, clamped to [0, total()-1] - used for both
    // "skip this LED" (step()+1) and "back" (step()-1).
    void setStep(int step);

    void stop();

    void load();

    void save() const;

};
