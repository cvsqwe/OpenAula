#pragma once

#include <string>
#include <vector>


// one key, coords in key units (1.0 = 1u)
struct KeyDef
{
    std::string label;

    double x;
    double y;
    double w;
    double h;

    // 0..89 slot in the colour array, reading order. calibration maps it
    // to the real LED
    int ledIndex;
};


// Aula F75 layout, 80 keys. no Home, no right Alt.
std::vector<KeyDef> buildF75Layout();


// volume knob, no LED
struct KnobGeometry
{
    double x;
    double y;
    double diameter;
};

KnobGeometry f75Knob();
