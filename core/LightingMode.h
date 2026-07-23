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
    Off
};
