#pragma once

#include <vector>


// Everything "outside" the lighting config that a reactive or system
// effect can respond to. openaula-daemon fills this from the real machine
// (core/SystemMonitor.h for metrics, daemon/KeyWatcher.h for keystrokes);
// the web preview fills an equivalent object in JS from /api/system and
// the browser's own key events.
struct SystemSignals
{
    // 0..1 each, already smoothed by whoever produced them.
    double cpu = 0.0;
    double memory = 0.0;
    double temperature = 0.0;   // 0 at 30 degC .. 1 at 95 degC
    double network = 0.0;       // log-scaled throughput, 0 idle .. 1 ~ 100 MB/s

    // Local wall-clock time.
    int hour = 0;
    int minute = 0;
    int second = 0;

    bool capsLock = false;
    bool numLock = false;
    bool scrollLock = false;

    // Seconds since each visual key (KeyDef::ledIndex) was last pressed;
    // empty or a very large value means "not recently".
    std::vector<double> keyAge;
};
