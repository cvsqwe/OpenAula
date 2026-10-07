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


// one colour per LED instead of one for all
std::vector<unsigned char> setColors(
    const std::vector<Color>& colors
);


std::vector<unsigned char> getInfo();


std::vector<unsigned char> setMode(
    unsigned char mode
);

}
