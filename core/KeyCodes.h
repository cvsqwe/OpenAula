#pragma once

#include <string>
#include <vector>


// ledIndex <-> evdev KEY_* codes. by index, not label - there are two Shift and two Ctrl
namespace KeyCodes
{

// 0 if out of range
int forF75LedIndex(int ledIndex);

// keys you can pick as a remap/macro target (all F75 keys + a few media keys)
const std::vector<std::pair<std::string, int>>& targetKeyList();

std::string nameForCode(int code);

}
