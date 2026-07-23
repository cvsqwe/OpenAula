#pragma once

#include <string>
#include <vector>


// Maps between OpenAULA's visual key layout (KeyDef::ledIndex, see
// core/KeyboardLayout.cpp) and Linux evdev KEY_* scancodes, for the key
// remap/macro editor (RemapConfig stores physical/remap key codes as
// plain ints so core/ doesn't need libevdev - see core/RemapConfig.h).
//
// Keyed by ledIndex rather than by label text because two keys on the
// F75 share the label "Shift" (and "Ctrl") but are different physical
// keys with different scancodes (KEY_LEFTSHIFT vs KEY_RIGHTSHIFT).
namespace KeyCodes
{

// The F75's key at this ledIndex, or 0 if out of range.
// (buildF75Layout() has 80 entries, ledIndex 0..79.)
int forF75LedIndex(int ledIndex);

// All (display name, KEY_* code) pairs offered when picking a *target*
// key for a remap or a macro step - every key the F75 has, plus a
// handful of common extras (media keys) macros might want to send.
const std::vector<std::pair<std::string, int>>& targetKeyList();

std::string nameForCode(int code);

}
