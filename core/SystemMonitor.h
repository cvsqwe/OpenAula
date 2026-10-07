#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "SystemSignals.h"


// Samples the machine-level inputs system effects react to - CPU load
// (/proc/stat), memory use (/proc/meminfo), CPU temperature (hwmon /
// thermal zones), network throughput (/proc/net/dev) and the local time.
// Plain procfs/sysfs reads, no dependencies. CPU and network are rates,
// so they need two samples: the first call after construction reports 0
// for both.
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

    // Takes a fresh sample and writes the smoothed metrics + clock into
    // `out` (leaving its keystroke / lock-key fields untouched).
    void sample(SystemSignals& out);

};
