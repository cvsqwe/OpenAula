#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "../core/SystemSignals.h"


// Watches the F75's own evdev nodes (read-only, never grabbed - typing
// keeps working normally) for the reactive effects: when each key was
// last pressed, plus the Caps/Num/Scroll Lock LED state. Runs on its own
// thread and re-scans for the keyboard if it's unplugged and replugged.
//
// Needs read access to /dev/input/eventN, which daemon/60-openaula.rules
// grants to the logged-in user. If openaula-remapd has the keyboard
// grabbed, events go to it instead and reactive effects stay dark.
class KeyWatcher
{
private:

    using Clock = std::chrono::steady_clock;

    std::thread worker;
    std::atomic<bool> running{false};

    std::mutex mutex;
    std::vector<Clock::time_point> lastPress;   // per visual key
    std::vector<bool> pressedEver;
    bool caps = false, num = false, scroll = false;

    // evdev KEY_* code -> visual key index (-1 if not on the board)
    std::vector<int> codeToKey;

    void run();


public:

    explicit KeyWatcher(int keyCount);

    ~KeyWatcher();

    void start();

    void stop();

    // Fills `out.keyAge` (seconds since each key's last press) and the
    // lock-key flags.
    void snapshot(SystemSignals& out);

};
