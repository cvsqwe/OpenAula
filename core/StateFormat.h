#pragma once

#include <string>
#include <vector>

#include "Color.h"
#include "LightingMode.h"


// Small line/field serialisation helpers shared by AppState (the live,
// currently-applied config) and ProfileStore (the saved library of
// configs you can switch between) - both persist the same kind of data
// (a lighting mode, speed, accent colour, per-key colour array) to plain
// text files, so the parsing/formatting lives here once instead of
// twice.
namespace StateFormat
{

// ~/.config/openaula (or $XDG_CONFIG_HOME/openaula) - where AppState and
// ProfileStore each keep one file.
std::string configDir();

std::vector<std::string> split(const std::string& s, char sep);

std::string joinInts(const std::vector<int>& values, char sep);

std::string colorToString(const Color& c);

Color colorFromString(const std::string& s);

std::string colorsToString(const std::vector<Color>& colors);

std::vector<Color> colorsFromString(const std::string& s, int keyCount);

const char* modeToString(LightingMode m);

LightingMode modeFromString(const std::string& s);

}
