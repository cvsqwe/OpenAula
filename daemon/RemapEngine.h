#pragma once

#include <atomic>


// Finds the Aula F75's keyboard *input* device (evdev - separate from
// the hidraw interface AulaKeyboard.cpp uses for lighting), grabs it
// exclusively, and re-emits every keystroke through a virtual uinput
// device - translating, dropping, or expanding into a macro according to
// RemapConfig (see core/RemapConfig.h). This is what makes
// remapping/macros work system-wide (any app, any window) instead of
// only inside OpenAULA.
//
// Deliberately a *separate* process/binary (openaula-remapd, see
// remapd_main.cpp) from openaula-daemon (lighting): grabbing an input
// device is a fundamentally more invasive, higher-risk operation than
// writing LED colours - a bug here can make typing stop working, so it
// gets its own process that can fail/restart independently, and users
// can choose not to install it at all while still getting lighting.
class RemapEngine
{
private:

    std::atomic<bool>& running;

    // Returns an open, non-blocking fd for the keyboard's evdev node, or
    // -1 if it isn't present (unplugged, or this isn't the right
    // interface - keyboards often expose several evdev nodes under the
    // same VID/PID, e.g. one for standard keys and one for consumer
    // control/media keys; only the one with ordinary letter keys is the
    // right target).
    int findKeyboardEventNode() const;


public:

    explicit RemapEngine(std::atomic<bool>& runningFlag);

    // Blocks until `running` is cleared (e.g. by a signal handler).
    // Repeatedly: waits for the keyboard's evdev node to appear, grabs
    // it, creates the uinput passthrough device, and pumps events -
    // reloading RemapConfig from disk periodically so GUI edits apply
    // without restarting this process. On any error, or once disabled
    // in RemapConfig, it ungrabs and retries rather than leaving the
    // physical keyboard stuck grabbed with nothing re-emitting its
    // keystrokes.
    void run();

};
