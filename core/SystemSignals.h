#pragma once

#include <vector>


// input for reactive/system effects. daemon fills it from SystemMonitor +
// KeyWatcher, the web preview from /api/system and keydown events
struct SystemSignals
{
    // 0..1
    double cpu = 0.0;
    double memory = 0.0;
    double temperature = 0.0;
    double network = 0.0;

    int hour = 0;
    int minute = 0;
    int second = 0;

    bool capsLock = false;
    bool numLock = false;
    bool scrollLock = false;

    // seconds since each key was last pressed, huge = never
    std::vector<double> keyAge;
};
