#pragma once

#include <vector>

#include "Color.h"


namespace AulaProtocol
{

std::vector<unsigned char> setColor(
    unsigned char r,
    unsigned char g,
    unsigned char b
);


// Per-key packet: one Color per LED index (same wire layout as setColor,
// but each of the 90 slots can carry its own color instead of a broadcast).
std::vector<unsigned char> setColors(
    const std::vector<Color>& colors
);


std::vector<unsigned char> getInfo();


std::vector<unsigned char> setMode(
    unsigned char mode
);

}
