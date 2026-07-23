#include "RemapConfig.h"
#include "StateFormat.h"

#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <unordered_map>


std::string bindingTypeToString(BindingType t)
{
    switch(t)
    {
        case BindingType::Passthrough: return "passthrough";
        case BindingType::Disabled:    return "disabled";
        case BindingType::Remap:       return "remap";
        case BindingType::Macro:       return "macro";
    }

    return "passthrough";
}



BindingType bindingTypeFromString(const std::string& s)
{
    if(s == "disabled") return BindingType::Disabled;
    if(s == "remap")    return BindingType::Remap;
    if(s == "macro")    return BindingType::Macro;

    return BindingType::Passthrough;
}



namespace
{

std::string macroToString(const std::vector<MacroStep>& steps)
{
    std::string out;

    for(size_t i = 0; i < steps.size(); i++)
    {
        if(i > 0) out += ";";

        out += std::to_string(steps[i].keyCode) + ":"
             + (steps[i].press ? "1" : "0") + ":"
             + std::to_string(steps[i].delayAfterMs);
    }

    return out;
}



std::vector<MacroStep> macroFromString(const std::string& s)
{
    std::vector<MacroStep> steps;

    for(const std::string& part : StateFormat::split(s, ';'))
    {
        auto fields = StateFormat::split(part, ':');
        if(fields.size() != 3)
            continue;

        MacroStep step;
        step.keyCode = std::atoi(fields[0].c_str());
        step.press = fields[1] == "1";
        step.delayAfterMs = std::atoi(fields[2].c_str());

        steps.push_back(step);
    }

    return steps;
}

}



std::string RemapConfig::filePath()
{
    return (std::filesystem::path(StateFormat::configDir()) / "remap.conf").string();
}



void RemapConfig::setBinding(const KeyBinding& binding)
{
    for(KeyBinding& existing : bindings)
    {
        if(existing.physicalKeyCode == binding.physicalKeyCode)
        {
            existing = binding;
            return;
        }
    }

    bindings.push_back(binding);
}



void RemapConfig::clearBinding(int physicalKeyCode)
{
    for(size_t i = 0; i < bindings.size(); i++)
    {
        if(bindings[i].physicalKeyCode == physicalKeyCode)
        {
            bindings.erase(bindings.begin() + i);
            return;
        }
    }
}



const KeyBinding* RemapConfig::bindingFor(int physicalKeyCode) const
{
    for(const KeyBinding& b : bindings)
        if(b.physicalKeyCode == physicalKeyCode)
            return &b;

    return nullptr;
}



void RemapConfig::load()
{
    bindings.clear();
    enabled = false;

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

    if(fields.count("enabled"))
        enabled = fields["enabled"] == "1";

    int count = fields.count("count") ? std::atoi(fields["count"].c_str()) : 0;

    for(int i = 0; i < count; i++)
    {
        std::string prefix = "binding" + std::to_string(i) + ".";

        if(!fields.count(prefix + "key"))
            continue;

        KeyBinding binding;
        binding.physicalKeyCode = std::atoi(fields[prefix + "key"].c_str());

        if(fields.count(prefix + "type"))
            binding.type = bindingTypeFromString(fields[prefix + "type"]);

        if(fields.count(prefix + "remap"))
            binding.remapKeyCode = std::atoi(fields[prefix + "remap"].c_str());

        if(fields.count(prefix + "macro"))
            binding.macro = macroFromString(fields[prefix + "macro"]);

        bindings.push_back(binding);
    }
}



void RemapConfig::save() const
{
    std::filesystem::path path(filePath());

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream file(path, std::ios::trunc);

    if(!file.is_open())
        return;

    file << "enabled=" << (enabled ? "1" : "0") << "\n";
    file << "count=" << bindings.size() << "\n";

    for(size_t i = 0; i < bindings.size(); i++)
    {
        const KeyBinding& b = bindings[i];
        std::string prefix = "binding" + std::to_string(i) + ".";

        file << prefix << "key=" << b.physicalKeyCode << "\n";
        file << prefix << "type=" << bindingTypeToString(b.type) << "\n";
        file << prefix << "remap=" << b.remapKeyCode << "\n";
        file << prefix << "macro=" << macroToString(b.macro) << "\n";
    }
}
