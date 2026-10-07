#include "AppState.h"
#include "StateFormat.h"

#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <unordered_map>


std::string AppState::filePath()
{
    return (std::filesystem::path(StateFormat::configDir()) / "state.conf").string();
}



int AppState::physicalIndexFor(int visualIndex) const
{
    if(visualIndex < 0 || visualIndex >= (int)visualToPhysical.size())
        return -1;

    return visualToPhysical[visualIndex];
}



void AppState::setCalibrationMapping(int visualIndex, int physicalIndex)
{
    if(visualIndex < 0 || visualIndex >= (int)visualToPhysical.size())
        return;

    visualToPhysical[visualIndex] = physicalIndex;
    calibrated = true;
}



void AppState::resetCalibration(int keyCount)
{
    visualToPhysical.resize(keyCount);

    for(int i = 0; i < keyCount; i++)
        visualToPhysical[i] = i;

    calibrated = false;
}



bool AppState::isCalibrated() const
{
    return calibrated;
}



void AppState::load(int keyCount)
{
    resetCalibration(keyCount);
    customColors.assign(keyCount, Color{24, 24, 28});
    layerStack = StateFormat::legacyLayers(currentMode, currentActiveColor, currentSpeed);

    std::ifstream file(filePath());

    if(!file.is_open())
        return;

    std::unordered_map<std::string, std::string> fields;
    std::string line;

    while(std::getline(file, line))
    {
        auto eq = line.find('=');
        if(eq == std::string::npos)
            continue;

        fields[line.substr(0, eq)] = line.substr(eq + 1);
    }

    // A layout change (different key count) invalidates any stored
    // calibration/custom colours - silently reapplying old indices to a
    // reshuffled key set would be worse than starting over.
    auto keyCountIt = fields.find("keycount");
    if(keyCountIt == fields.end() || std::atoi(keyCountIt->second.c_str()) != keyCount)
        return;

    if(fields.count("calibrated"))
        calibrated = fields["calibrated"] == "1";

    if(fields.count("map"))
    {
        auto parts = StateFormat::split(fields["map"], ',');

        for(int i = 0; i < (int)parts.size() && i < keyCount; i++)
            visualToPhysical[i] = std::atoi(parts[i].c_str());
    }

    if(fields.count("mode"))
        currentMode = StateFormat::modeFromString(fields["mode"]);

    if(fields.count("speed"))
        currentSpeed = std::atof(fields["speed"].c_str());

    if(fields.count("brightness"))
        currentBrightness = std::atof(fields["brightness"].c_str());

    if(fields.count("active"))
        currentActiveColor = StateFormat::colorFromString(fields["active"]);

    if(fields.count("colors"))
        customColors = StateFormat::colorsFromString(fields["colors"], keyCount);

    if(fields.count("profile"))
        activeProfileName = fields["profile"];

    layerStack = StateFormat::layersFromFields(fields, keyCount);
    if(!fields.count("layers"))
        layerStack = StateFormat::legacyLayers(currentMode, currentActiveColor, currentSpeed);
}



void AppState::save() const
{
    std::filesystem::path path(filePath());

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream file(path, std::ios::trunc);

    if(!file.is_open())
        return;

    file << "keycount=" << visualToPhysical.size() << "\n";
    file << "calibrated=" << (calibrated ? "1" : "0") << "\n";
    file << "map=" << StateFormat::joinInts(visualToPhysical, ',') << "\n";
    file << "mode=" << StateFormat::modeToString(currentMode) << "\n";
    file << "speed=" << currentSpeed << "\n";
    file << "brightness=" << currentBrightness << "\n";
    file << "active=" << StateFormat::colorToString(currentActiveColor) << "\n";
    file << "colors=" << StateFormat::colorsToString(customColors) << "\n";
    file << "profile=" << activeProfileName << "\n";
    StateFormat::writeLayers(file, layerStack);
}
