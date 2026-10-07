#pragma once

#include <string>
#include <vector>
#include <ostream>
#include <unordered_map>

#include "Color.h"
#include "LightingMode.h"
#include "Layer.h"


// helpers for the plain text config files
namespace StateFormat
{

// $XDG_CONFIG_HOME/openaula or ~/.config/openaula
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

// "*" = all keys, otherwise one 0/1 per key
std::string maskToString(const std::vector<bool>& mask);

std::vector<bool> maskFromString(const std::string& s, int keyCount);

std::string layerToString(const Layer& layer);

Layer layerFromString(const std::string& s, int keyCount);

// layers=N + layer0..layerN-1
std::vector<Layer> layersFromFields(const std::unordered_map<std::string, std::string>& fields, int keyCount);

void writeLayers(std::ostream& out, const std::vector<Layer>& layers);

// converts a pre-layers config (mode + colour + speed) into layers
std::vector<Layer> legacyLayers(LightingMode mode, const Color& activeColor, double speed);

}
