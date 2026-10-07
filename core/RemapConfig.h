#pragma once

#include <string>
#include <vector>


// one macro step: press/release a KEY_* code, then wait delayAfterMs
struct MacroStep
{
    int keyCode = 0;
    bool press = true;
    int delayAfterMs = 0;
};


enum class BindingType
{
    Passthrough,  // default
    Disabled,
    Remap,
    Macro
};


struct KeyBinding
{
    int physicalKeyCode = 0;
    BindingType type = BindingType::Passthrough;
    int remapKeyCode = 0;
    std::vector<MacroStep> macro;
};


std::string bindingTypeToString(BindingType type);
BindingType bindingTypeFromString(const std::string& s);


// key remap/macro config (remap.conf). applied by openaula-remapd,
// the ui only edits the file
class RemapConfig
{
private:

    std::vector<KeyBinding> bindings;
    bool enabled = false;


public:

    static std::string filePath();


    const std::vector<KeyBinding>& all() const { return bindings; }

    bool isEnabled() const { return enabled; }
    void setEnabled(bool e) { enabled = e; }

    // replaces an existing binding
    void setBinding(const KeyBinding& binding);

    // same as setting it back to Passthrough
    void clearBinding(int physicalKeyCode);

    const KeyBinding* bindingFor(int physicalKeyCode) const;


    void load();

    void save() const;

};
