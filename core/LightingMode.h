#pragma once


// Shared between the GUI and the background daemon so both drive the
// keyboard identically - see LightingEngine.h.
enum class LightingMode
{
    Custom,
    Breathing,
    ColorCycle,
    Bounce,
    Wave,
    Ripple,
    Starlight,
    Raindrop,
    Comet,
    Fire,
    RainbowWave,
    Heartbeat,
    Strobe,
    Alternating,
    Confetti,
    Snake,
    Spiral,
    Fireworks,
    Sweep,
    Off,

    // Ambient additions.
    Aurora,
    Matrix,
    Gradient,

    // Reactive: driven by real keystrokes (see SystemSignals::keyAge).
    Afterglow,
    Splash,

    // System: driven by live machine metrics (see core/SystemMonitor.h).
    CpuLoad,
    Memory,
    Thermal,
    Network,
    Clock,
    Indicators
};
