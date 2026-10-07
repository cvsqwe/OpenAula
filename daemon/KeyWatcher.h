#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#include "../core/SystemSignals.h"


// reads key presses from the F75's evdev nodes (no grab) for the reactive
// effects, plus caps/num/scroll lock. reconnects on replug.
// won't see anything while openaula-remapd has the keyboard grabbed.
class KeyWatcher
{
private:

    using Clock = std::chrono::steady_clock;

    std::thread worker;
    std::atomic<bool> running{false};

    std::mutex mutex;
    std::vector<Clock::time_point> lastPress;
    std::vector<bool> pressedEver;
    bool caps = false, num = false, scroll = false;

    // KEY_* -> visual key, -1 if not on the board
    std::vector<int> codeToKey;

    void run();


public:

    explicit KeyWatcher(int keyCount);

    ~KeyWatcher();

    void start();

    void stop();

    void snapshot(SystemSignals& out);

};
