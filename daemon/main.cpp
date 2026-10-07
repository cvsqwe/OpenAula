// openaula-daemon - keeps the backlight running. reads state.conf (re-reads
// every second or on SIGUSR1) and drives the keyboard over hidraw.

#include "../core/AulaController.h"
#include "../core/AppState.h"
#include "../core/LightingEngine.h"
#include "../core/KeyboardLayout.h"
#include "../core/LightingMode.h"
#include "../core/CalibrationSession.h"
#include "../core/SystemMonitor.h"
#include "KeyWatcher.h"

#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <iostream>
#include <algorithm>


namespace
{

std::atomic<bool> running{true};

// SIGUSR1 from webd -> reload now
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

// sleep, but wake up early on SIGUSR1
void sleepUnlessNudged(int ms)
{
    for(int slept = 0; slept < ms && running && !forceReload; slept += 20)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
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
    auto lastFrame = lastStateCheck;
    auto lastSystemSample = lastStateCheck - std::chrono::seconds(10);

    // per-layer clocks, advanced by speed each frame so changing the
    // speed doesn't make the animation jump
    std::vector<double> phases;

    SystemSignals signals;
    SystemMonitor monitor;
    KeyWatcher keyWatcher((int)keys.size());
    keyWatcher.start();

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
        }

        // calibration: only the LED being asked about, white
        if(calib.active())
        {
            std::vector<Color> calibFrame(TotalPhysicalLeds, Color{0, 0, 0});

            if(calib.step() >= 0 && calib.step() < TotalPhysicalLeds)
                calibFrame[calib.step()] = Color{255, 255, 255};

            controller.setColors(calibFrame);

            sleepUnlessNudged(80);
            lastFrame = std::chrono::steady_clock::now();
            continue;
        }

        const std::vector<Layer>& layers = state.layers();

        bool animated = false, wantsMetrics = false;
        for(const Layer& layer : layers)
        {
            if(!layer.enabled) continue;
            animated = animated || LightingEngine::isAnimated(layer.effect);
            wantsMetrics = wantsMetrics || LightingEngine::usesSystemMetrics(layer.effect);
        }

        double dt = std::min(0.25, std::chrono::duration<double>(now - lastFrame).count());
        lastFrame = now;

        phases.resize(layers.size(), 0.0);
        for(size_t i = 0; i < layers.size(); i++)
            phases[i] += dt * std::max(0.05, layers[i].speed);

        if(wantsMetrics && now - lastSystemSample > std::chrono::milliseconds(500))
        {
            monitor.sample(signals);
            lastSystemSample = now;
        }

        keyWatcher.snapshot(signals);

        std::vector<Color> frame = LightingEngine::composite(
            layers, phases, keys, state.customColorsRef(), signals
        );

        LightingEngine::applyBrightness(frame, state.brightness());

        controller.setColors(toPhysical(frame, state, TotalPhysicalLeds));

        sleepUnlessNudged(animated ? 33 : 1000);
    }

    keyWatcher.stop();

    std::cout << "openaula-daemon: stopping" << std::endl;

    controller.stop();

    return 0;
}
