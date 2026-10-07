#pragma once

#include <vector>

#include "Color.h"
#include "LightingMode.h"


// How a layer combines with everything below it in the stack.
enum class BlendMode
{
    Normal,     // replaces what's below (mixed by opacity)
    Add,        // light adds up
    Lighten,    // brightest channel wins
    Multiply    // dims/tints what's below - e.g. Breath over a Canvas
};


// One entry in the lighting stack. The whole backlight is the result of
// compositing every enabled layer bottom-to-top (see
// LightingEngine::composite), each limited to the keys in its mask - so
// different keys can run different effects, or several effects can be
// stacked on the same keys.
struct Layer
{
    LightingMode effect = LightingMode::Custom;
    Color color{124, 92, 255};
    double speed = 1.0;
    double opacity = 1.0;
    BlendMode blend = BlendMode::Normal;
    bool enabled = true;

    // One flag per visual key (KeyDef::ledIndex); empty means every key.
    std::vector<bool> mask;
};
