#include "ProfileStore.h"
#include "StateFormat.h"

#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <unordered_map>


std::string ProfileStore::filePath()
{
    return (std::filesystem::path(StateFormat::configDir()) / "profiles.conf").string();
}



int ProfileStore::addProfile(const Profile& profile)
{
    profiles.push_back(profile);
    return (int)profiles.size() - 1;
}



void ProfileStore::updateProfile(int index, const Profile& profile)
{
    if(index < 0 || index >= (int)profiles.size())
        return;

    profiles[index] = profile;
}



void ProfileStore::renameProfile(int index, const std::string& newName)
{
    if(index < 0 || index >= (int)profiles.size())
        return;

    profiles[index].name = newName;
}



void ProfileStore::removeProfile(int index)
{
    if(index < 0 || index >= (int)profiles.size())
        return;

    profiles.erase(profiles.begin() + index);

    if(activeIndex == index)
        activeIndex = -1;
    else if(activeIndex > index)
        activeIndex--;
}



int ProfileStore::indexByName(const std::string& name) const
{
    for(size_t i = 0; i < profiles.size(); i++)
    {
        if(profiles[i].name == name)
            return (int)i;
    }

    return -1;
}



void ProfileStore::load(int keyCount)
{
    profiles.clear();
    activeIndex = -1;

    std::ifstream file(filePath());

    if(!file.is_open())
        return;

    std::unordered_map<std::string, std::string> fields;
    bool inBlock = false;

    auto flushBlock = [&]()
    {
        if(!inBlock)
            return;

        Profile p;
        p.name = fields.count("name") ? fields["name"] : "Untitled";

        if(fields.count("mode"))
            p.mode = StateFormat::modeFromString(fields["mode"]);

        if(fields.count("speed"))
            p.speed = std::atof(fields["speed"].c_str());

        if(fields.count("brightness"))
            p.brightness = std::atof(fields["brightness"].c_str());

        if(fields.count("active"))
            p.activeColor = StateFormat::colorFromString(fields["active"]);

        p.customColors = fields.count("colors")
            ? StateFormat::colorsFromString(fields["colors"], keyCount)
            : std::vector<Color>(keyCount, Color{24, 24, 28});

        p.layers = fields.count("layers")
            ? StateFormat::layersFromFields(fields, keyCount)
            : StateFormat::legacyLayers(p.mode, p.activeColor, p.speed);

        profiles.push_back(p);
        fields.clear();
    };

    std::string line;

    while(std::getline(file, line))
    {
        if(!line.empty() && line.front() == '[')
        {
            flushBlock();
            inBlock = true;
            continue;
        }

        auto eq = line.find('=');
        if(eq == std::string::npos)
            continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if(!inBlock)
        {
            if(key == "activeIndex")
                activeIndex = std::atoi(value.c_str());

            continue;
        }

        fields[key] = value;
    }

    flushBlock();

    if(activeIndex < -1 || activeIndex >= (int)profiles.size())
        activeIndex = -1;
}



void ProfileStore::save() const
{
    std::filesystem::path path(filePath());

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream file(path, std::ios::trunc);

    if(!file.is_open())
        return;

    file << "activeIndex=" << activeIndex << "\n";
    file << "count=" << profiles.size() << "\n";

    for(size_t i = 0; i < profiles.size(); i++)
    {
        const Profile& p = profiles[i];

        // no newlines in names
        std::string safeName = p.name;
        for(char& c : safeName)
            if(c == '\n' || c == '\r') c = ' ';

        file << "[" << i << "]\n";
        file << "name=" << safeName << "\n";
        file << "mode=" << StateFormat::modeToString(p.mode) << "\n";
        file << "speed=" << p.speed << "\n";
        file << "brightness=" << p.brightness << "\n";
        file << "active=" << StateFormat::colorToString(p.activeColor) << "\n";
        file << "colors=" << StateFormat::colorsToString(p.customColors) << "\n";
        StateFormat::writeLayers(file, p.layers);
    }
}
