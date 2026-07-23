#include "CalibrationSession.h"
#include "StateFormat.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <unordered_map>


std::string CalibrationSession::filePath()
{
    return (std::filesystem::path(StateFormat::configDir()) / "calibration.conf").string();
}



void CalibrationSession::start(int totalPhysicalLeds)
{
    sessionActive = true;
    totalSteps = std::max(0, totalPhysicalLeds);
    currentStep = 0;
}



void CalibrationSession::setStep(int step)
{
    if(totalSteps <= 0)
    {
        currentStep = 0;
        return;
    }

    currentStep = std::clamp(step, 0, totalSteps - 1);
}



void CalibrationSession::stop()
{
    sessionActive = false;
}



void CalibrationSession::load()
{
    sessionActive = false;
    currentStep = 0;
    totalSteps = 0;

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

    if(fields.count("active"))
        sessionActive = fields["active"] == "1";

    if(fields.count("total"))
        totalSteps = std::atoi(fields["total"].c_str());

    if(fields.count("step"))
        currentStep = std::atoi(fields["step"].c_str());
}



void CalibrationSession::save() const
{
    std::filesystem::path path(filePath());

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream file(path, std::ios::trunc);
    if(!file.is_open())
        return;

    file << "active=" << (sessionActive ? "1" : "0") << "\n";
    file << "total=" << totalSteps << "\n";
    file << "step=" << currentStep << "\n";
}
