#pragma once

#include <atomic>


// grabs the F75's evdev device and re-emits everything through uinput,
// remapped / disabled / as macros (see RemapConfig).
// separate binary from the lighting daemon - if this breaks, typing
// breaks, so it's opt-in and can crash on its own.
class RemapEngine
{
private:

    std::atomic<bool>& running;

    // fd of the evdev node with the normal letter keys (the board has a
    // few), -1 if not found
    int findKeyboardEventNode() const;


public:

    explicit RemapEngine(std::atomic<bool>& runningFlag);

    // runs until `running` is false. reloads the config now and then and
    // always ungrabs on errors so the keyboard never stays stuck
    void run();

};
