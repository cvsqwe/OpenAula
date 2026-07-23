// openaula-daemon: keeps the F75's backlight showing exactly what the GUI
// last configured (static design or a running animation), independent of
// whether the GUI is even open. Reads the same state file the GUI writes
// (see core/AppState.h) and re-polls it roughly once a second, so changes
// made in the GUI while this is running take effect without a restart.
//
// Deliberately has zero Qt dependency - it links only core/ and hidapi, so
// it's cheap enough to run permanently as a systemd --user service (see
// openaula-daemon.service).

#include "../core/AulaController.h"
#include "../core/AppState.h"
#include "../core/LightingEngine.h"
#include "../core/KeyboardLayout.h"
#include "../core/LightingMode.h"
#include "../core/CalibrationSession.h"

#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <iostream>
#include <algorithm>


namespace
{

std::atomic<bool> running{true};

// Set by SIGUSR1 (sent by the GUI right after it edits AppState's file)
// so a change made while daemon-attached takes effect immediately instead
// of waiting for the next ~1s poll.
std::atomic<bool> forceReload{false};

void handleSignal(int)
{
    running = false;
}

void handleReloadSignal(int)
{
    forceReload = true;
}

std::vector<Color> toPhysical(const std::vector<Color>& visualFrame, const AppState& state, int totalPhysicalLeds)
{
    std::vector<Color> physical(totalPhysicalLeds, Color{0, 0, 0});

    for(size_t i = 0; i < visualFrame.size(); i++)
    {
        int phys = state.physicalIndexFor((int)i);

        if(phys >= 0 && phys < (int)physical.size())
            physical[phys] = visualFrame[i];
    }

    return physical;
}

bool isAnimatedMode(LightingMode mode)
{
    return mode != LightingMode::Custom && mode != LightingMode::Off;
}

}



int main()
{
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
    std::signal(SIGUSR1, handleReloadSignal);

    static constexpr int TotalPhysicalLeds = 90;

    AulaController controller;

    std::cout << "openaula-daemon: connecting..." << std::endl;

    while(running && !controller.connect())
    {
        std::cerr << "openaula-daemon: keyboard not found, retrying in 5s" << std::endl;

        for(int i = 0; i < 50 && running; i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if(!running)
        return 0;

    std::vector<KeyDef> keys = buildF75Layout();

    AppState state;
    state.load((int)keys.size());

    std::cout << "openaula-daemon: connected, " << keys.size() << " keys, mode="
              << (int)state.mode() << (state.isCalibrated() ? " (calibrated)" : " (NOT calibrated)")
              << std::endl;

    auto lastStateCheck = std::chrono::steady_clock::now();
    auto modeStart = lastStateCheck;
    LightingMode lastMode = state.mode();

    CalibrationSession calib;
    calib.load();

    while(running)
    {
        auto now = std::chrono::steady_clock::now();
        bool reload = forceReload.exchange(false) || now - lastStateCheck > std::chrono::seconds(1);

        if(reload)
        {
            lastStateCheck = now;
            state.load((int)keys.size());
            calib.load();

            if(state.mode() != lastMode)
            {
                lastMode = state.mode();
                modeStart = now;
            }
        }

        // While a calibration session (see the web app's Settings panel)
        // is active, that takes over the hardware entirely: light exactly
        // the one physical LED the wizard is currently asking about
        // (white, full brightness, everything else off) instead of
        // whatever effect is otherwise configured, so the user can watch
        // the real keyboard and see which key it is. Bridge writes the
        // step forward/back as the user answers and nudges us the same
        // way it does for any other lighting change.
        if(calib.active())
        {
            std::vector<Color> calibFrame(TotalPhysicalLeds, Color{0, 0, 0});

            if(calib.step() >= 0 && calib.step() < TotalPhysicalLeds)
                calibFrame[calib.step()] = Color{255, 255, 255};

            controller.setColors(calibFrame);

            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            continue;
        }

        double elapsed = std::chrono::duration<double>(now - modeStart).count() * std::max(0.1, state.speed());

        std::vector<Color> frame = LightingEngine::computeFrame(
            state.mode(), elapsed, keys, state.customColorsRef(), state.activeColor()
        );

        LightingEngine::applyBrightness(frame, state.brightness());

        controller.setColors(toPhysical(frame, state, TotalPhysicalLeds));

        std::this_thread::sleep_for(
            std::chrono::milliseconds(isAnimatedMode(state.mode()) ? 33 : 2000)
        );
    }

    std::cout << "openaula-daemon: stopping" << std::endl;

    controller.stop();

    return 0;
}
