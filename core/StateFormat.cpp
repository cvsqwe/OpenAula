#include "StateFormat.h"

#include <cstdlib>
#include <sstream>
#include <filesystem>


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

    return LightingMode::Custom;
}

}
