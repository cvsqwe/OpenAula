#pragma once

#include <vector>

#include "Color.h"
#include "LightingMode.h"


enum class BlendMode
{
    Normal,     // replaces what's below (mixed by opacity)
    Add,        // light adds up
    Lighten,    // brightest channel wins
    Multiply    // tints what's below
};


// one layer of the lighting stack, see LightingEngine::composite
struct Layer
{
    LightingMode effect = LightingMode::Custom;
    Color color{124, 92, 255};
    double speed = 1.0;
    double opacity = 1.0;
    BlendMode blend = BlendMode::Normal;
    bool enabled = true;

    // per visual key, empty = all keys
    std::vector<bool> mask;
};
