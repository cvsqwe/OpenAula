#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "SystemSignals.h"


// cpu, memory, temperature, network and the clock for the system effects.
// cpu and network need two samples, so the first one reads 0
class SystemMonitor
{
private:

    unsigned long long lastIdle = 0, lastTotal = 0;
    unsigned long long lastNetBytes = 0;
    std::chrono::steady_clock::time_point lastNetAt;
    bool primed = false;

    std::vector<std::string> tempPaths;

    SystemSignals smoothed;


public:

    SystemMonitor();

    // fills the metrics + clock, leaves the key fields alone
    void sample(SystemSignals& out);

};
