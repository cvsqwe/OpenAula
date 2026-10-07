#pragma once

#include <string>
#include <vector>
#include <ostream>
#include <unordered_map>

#include "Color.h"
#include "LightingMode.h"
#include "Layer.h"


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

const char* blendToString(BlendMode b);

BlendMode blendFromString(const std::string& s);

// "*" for an every-key mask, else one '0'/'1' per visual key.
std::string maskToString(const std::vector<bool>& mask);

std::vector<bool> maskFromString(const std::string& s, int keyCount);

std::string layerToString(const Layer& layer);

Layer layerFromString(const std::string& s, int keyCount);

// Reads the "layers=N" + "layer0=..".."layerN-1=.." fields shared by
// state.conf and each profiles.conf block; empty if there are none.
std::vector<Layer> layersFromFields(const std::unordered_map<std::string, std::string>& fields, int keyCount);

void writeLayers(std::ostream& out, const std::vector<Layer>& layers);

// The layer stack equivalent to a pre-layers config (single mode +
// accent colour + speed), so old state/profile files keep looking the
// same after an upgrade.
std::vector<Layer> legacyLayers(LightingMode mode, const Color& activeColor, double speed);

}
