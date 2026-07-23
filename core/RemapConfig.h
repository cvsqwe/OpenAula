#pragma once

#include <string>
#include <vector>


// One step in a macro: press or release a specific Linux input key code
// (KEY_* from linux/input-event-codes.h - stored as a plain int here so
// this stays libevdev/Qt-free like the rest of core/), plus a pause
// after this step before the next one.
struct MacroStep
{
    int keyCode = 0;
    bool press = true;
    int delayAfterMs = 0;
};


enum class BindingType
{
    Passthrough,  // key behaves normally - the default for every key
    Disabled,     // key press is swallowed, does nothing
    Remap,        // key press is translated to a different single key
    Macro         // key press plays back a fixed sequence of key events
};


struct KeyBinding
{
    int physicalKeyCode = 0;
    BindingType type = BindingType::Passthrough;
    int remapKeyCode = 0;
    std::vector<MacroStep> macro;
};


// Exported so callers outside RemapConfig.cpp (the web bridge's JSON
// encoding/decoding, in particular) share the exact same string spelling
// instead of re-deriving it and risking drift.
std::string bindingTypeToString(BindingType type);
BindingType bindingTypeFromString(const std::string& s);


// The live, currently-applied key-remap/macro configuration - same
// single-active-config, file-is-the-source-of-truth pattern as AppState,
// just for input remapping instead of lighting. Only daemon/RemapEngine
// (not part of Qt-free core/, since it needs libevdev) actually applies
// this by grabbing the keyboard's input device; the GUI only edits the
// file and nudges the remap daemon the same way MainWindow nudges
// openaula-daemon for lighting changes.
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

    // Adds a binding for physicalKeyCode, replacing any existing one.
    void setBinding(const KeyBinding& binding);

    // Removes any binding for physicalKeyCode (equivalent to explicitly
    // setting it back to Passthrough).
    void clearBinding(int physicalKeyCode);

    const KeyBinding* bindingFor(int physicalKeyCode) const;


    void load();

    void save() const;

};
