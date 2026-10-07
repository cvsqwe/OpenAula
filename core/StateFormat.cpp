#include "StateFormat.h"

#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <ostream>


namespace StateFormat
{

std::string configDir()
{
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");

    std::filesystem::path base;

    if(xdgConfig && *xdgConfig)
    {
        base = xdgConfig;
    }
    else
    {
        const char* home = std::getenv("HOME");
        base = (home && *home) ? std::filesystem::path(home) / ".config" : std::filesystem::path(".");
    }

    return (base / "openaula").string();
}



std::vector<std::string> split(const std::string& s, char sep)
{
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;

    while(std::getline(ss, item, sep))
        parts.push_back(item);

    return parts;
}



std::string joinInts(const std::vector<int>& values, char sep)
{
    std::string out;

    for(size_t i = 0; i < values.size(); i++)
    {
        if(i > 0) out += sep;
        out += std::to_string(values[i]);
    }

    return out;
}



std::string colorToString(const Color& c)
{
    return std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b);
}



Color colorFromString(const std::string& s)
{
    auto parts = split(s, ',');

    Color c{0, 0, 0};

    if(parts.size() == 3)
    {
        c.r = (unsigned char)std::atoi(parts[0].c_str());
        c.g = (unsigned char)std::atoi(parts[1].c_str());
        c.b = (unsigned char)std::atoi(parts[2].c_str());
    }

    return c;
}



std::string colorsToString(const std::vector<Color>& colors)
{
    std::string out;

    for(size_t i = 0; i < colors.size(); i++)
    {
        if(i > 0) out += ";";
        out += colorToString(colors[i]);
    }

    return out;
}



std::vector<Color> colorsFromString(const std::string& s, int keyCount)
{
    std::vector<Color> colors(keyCount, Color{24, 24, 28});

    auto parts = split(s, ';');

    for(int i = 0; i < (int)parts.size() && i < keyCount; i++)
        colors[i] = colorFromString(parts[i]);

    return colors;
}



const char* modeToString(LightingMode m)
{
    switch(m)
    {
        case LightingMode::Custom:      return "custom";
        case LightingMode::Breathing:   return "breathing";
        case LightingMode::ColorCycle:  return "colorcycle";
        case LightingMode::Bounce:      return "bounce";
        case LightingMode::Wave:        return "wave";
        case LightingMode::Ripple:      return "ripple";
        case LightingMode::Starlight:   return "starlight";
        case LightingMode::Raindrop:    return "raindrop";
        case LightingMode::Comet:       return "comet";
        case LightingMode::Fire:        return "fire";
        case LightingMode::RainbowWave: return "rainbowwave";
        case LightingMode::Heartbeat:   return "heartbeat";
        case LightingMode::Strobe:      return "strobe";
        case LightingMode::Alternating: return "alternating";
        case LightingMode::Confetti:    return "confetti";
        case LightingMode::Snake:       return "snake";
        case LightingMode::Spiral:      return "spiral";
        case LightingMode::Fireworks:   return "fireworks";
        case LightingMode::Sweep:       return "sweep";
        case LightingMode::Off:         return "off";
        case LightingMode::Aurora:      return "aurora";
        case LightingMode::Matrix:      return "matrix";
        case LightingMode::Gradient:    return "gradient";
        case LightingMode::Afterglow:   return "afterglow";
        case LightingMode::Splash:      return "splash";
        case LightingMode::CpuLoad:     return "cpu";
        case LightingMode::Memory:      return "memory";
        case LightingMode::Thermal:     return "thermal";
        case LightingMode::Network:     return "network";
        case LightingMode::Clock:       return "clock";
        case LightingMode::Indicators:  return "indicators";
    }

    return "custom";
}



LightingMode modeFromString(const std::string& s)
{
    if(s == "breathing")   return LightingMode::Breathing;
    if(s == "colorcycle")  return LightingMode::ColorCycle;
    if(s == "bounce")      return LightingMode::Bounce;
    if(s == "wave")        return LightingMode::Wave;
    if(s == "ripple")      return LightingMode::Ripple;
    if(s == "starlight")   return LightingMode::Starlight;
    if(s == "raindrop")    return LightingMode::Raindrop;
    if(s == "comet")       return LightingMode::Comet;
    if(s == "fire")        return LightingMode::Fire;
    if(s == "rainbowwave") return LightingMode::RainbowWave;
    if(s == "heartbeat")   return LightingMode::Heartbeat;
    if(s == "strobe")      return LightingMode::Strobe;
    if(s == "alternating") return LightingMode::Alternating;
    if(s == "confetti")    return LightingMode::Confetti;
    if(s == "snake")       return LightingMode::Snake;
    if(s == "spiral")      return LightingMode::Spiral;
    if(s == "fireworks")   return LightingMode::Fireworks;
    if(s == "sweep")       return LightingMode::Sweep;
    if(s == "off")         return LightingMode::Off;
    if(s == "aurora")      return LightingMode::Aurora;
    if(s == "matrix")      return LightingMode::Matrix;
    if(s == "gradient")    return LightingMode::Gradient;
    if(s == "afterglow")   return LightingMode::Afterglow;
    if(s == "splash")      return LightingMode::Splash;
    if(s == "cpu")         return LightingMode::CpuLoad;
    if(s == "memory")      return LightingMode::Memory;
    if(s == "thermal")     return LightingMode::Thermal;
    if(s == "network")     return LightingMode::Network;
    if(s == "clock")       return LightingMode::Clock;
    if(s == "indicators")  return LightingMode::Indicators;

    return LightingMode::Custom;
}




const char* blendToString(BlendMode b)
{
    switch(b)
    {
        case BlendMode::Normal:   return "normal";
        case BlendMode::Add:      return "add";
        case BlendMode::Lighten:  return "lighten";
        case BlendMode::Multiply: return "multiply";
    }

    return "normal";
}



BlendMode blendFromString(const std::string& s)
{
    if(s == "add")      return BlendMode::Add;
    if(s == "lighten")  return BlendMode::Lighten;
    if(s == "multiply") return BlendMode::Multiply;

    return BlendMode::Normal;
}



std::string maskToString(const std::vector<bool>& mask)
{
    if(mask.empty())
        return "*";

    std::string out(mask.size(), '0');
    for(size_t i = 0; i < mask.size(); i++)
        if(mask[i]) out[i] = '1';

    return out;
}



std::vector<bool> maskFromString(const std::string& s, int keyCount)
{
    if(s.empty() || s == "*")
        return {};

    std::vector<bool> mask(keyCount, false);
    for(int i = 0; i < keyCount && i < (int)s.size(); i++)
        mask[i] = s[i] == '1';

    return mask;
}



// effect|r,g,b|speed|opacity|blend|enabled|mask
std::string layerToString(const Layer& layer)
{
    std::ostringstream ss;
    ss << modeToString(layer.effect) << "|" << colorToString(layer.color) << "|"
       << layer.speed << "|" << layer.opacity << "|" << blendToString(layer.blend) << "|"
       << (layer.enabled ? "1" : "0") << "|" << maskToString(layer.mask);
    return ss.str();
}



Layer layerFromString(const std::string& s, int keyCount)
{
    Layer layer;
    auto parts = split(s, '|');

    if(parts.size() > 0) layer.effect = modeFromString(parts[0]);
    if(parts.size() > 1) layer.color = colorFromString(parts[1]);
    if(parts.size() > 2) layer.speed = std::atof(parts[2].c_str());
    if(parts.size() > 3) layer.opacity = std::atof(parts[3].c_str());
    if(parts.size() > 4) layer.blend = blendFromString(parts[4]);
    if(parts.size() > 5) layer.enabled = parts[5] != "0";
    if(parts.size() > 6) layer.mask = maskFromString(parts[6], keyCount);

    return layer;
}



std::vector<Layer> layersFromFields(const std::unordered_map<std::string, std::string>& fields, int keyCount)
{
    std::vector<Layer> layers;

    auto countIt = fields.find("layers");
    if(countIt == fields.end())
        return layers;

    int count = std::atoi(countIt->second.c_str());

    for(int i = 0; i < count; i++)
    {
        auto it = fields.find("layer" + std::to_string(i));
        if(it != fields.end())
            layers.push_back(layerFromString(it->second, keyCount));
    }

    return layers;
}



void writeLayers(std::ostream& out, const std::vector<Layer>& layers)
{
    out << "layers=" << layers.size() << "\n";

    for(size_t i = 0; i < layers.size(); i++)
        out << "layer" << i << "=" << layerToString(layers[i]) << "\n";
}



std::vector<Layer> legacyLayers(LightingMode mode, const Color& activeColor, double speed)
{
    Layer base;
    base.color = activeColor;
    base.speed = speed;

    // old breathing = breathe the custom colours, so canvas + white breath on top
    if(mode == LightingMode::Breathing)
    {
        Layer breath;
        breath.effect = LightingMode::Breathing;
        breath.color = Color{255, 255, 255};
        breath.speed = speed;
        breath.blend = BlendMode::Multiply;
        return { base, breath };
    }

    base.effect = mode;
    return { base };
}

}
