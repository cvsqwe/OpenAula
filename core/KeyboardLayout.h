#pragma once

#include <string>
#include <vector>


// Describes one physical key. Coordinates are in "key units" (1.0 == the
// width of a standard 1u key); renderers pick their own pixel scale.
struct KeyDef
{
    std::string label;

    double x;
    double y;
    double w;
    double h;

    // Index into the 90-slot per-key colour array understood by
    // AulaProtocol::setColors(). The real firmware->LED-index order isn't
    // documented anywhere for this board (see LedCalibration.h) - this is
    // just a stable "reading order" numbering that LedCalibration then
    // remaps to the real physical order once calibrated.
    int ledIndex;
};


// Physical layout of the Aula F75, matched against official product
// photos: Esc/F-row, number row, QWERTY block, a Delete/PgUp/PgDn/End nav
// column, an inverted-T arrow cluster, and NO dedicated Home key or right
// Alt key (this board omits both). 80 keys, within the protocol's 90 slots.
std::vector<KeyDef> buildF75Layout();


// The board also has a non-backlit rotary volume knob, top-right. It's
// rendered but deliberately excluded from buildF75Layout() - it has no
// LED to address and isn't selectable/colourable.
struct KnobGeometry
{
    double x;
    double y;
    double diameter;
};

KnobGeometry f75Knob();
