#include "KeyCodes.h"

#include <linux/input-event-codes.h>


namespace
{

// same order as buildF75Layout()
constexpr int F75KeyCodes[] = {
    // Row 0
    KEY_ESC,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4,
    KEY_F5, KEY_F6, KEY_F7, KEY_F8,
    KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    // Row 1
    KEY_GRAVE,
    KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
    KEY_MINUS, KEY_EQUAL,
    KEY_BACKSPACE,
    KEY_DELETE,
    // Row 2
    KEY_TAB,
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P,
    KEY_LEFTBRACE, KEY_RIGHTBRACE,
    KEY_BACKSLASH,
    KEY_PAGEUP,
    // Row 3
    KEY_CAPSLOCK,
    KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L,
    KEY_SEMICOLON, KEY_APOSTROPHE,
    KEY_ENTER,
    KEY_PAGEDOWN,
    // Row 4
    KEY_LEFTSHIFT,
    KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M,
    KEY_COMMA, KEY_DOT, KEY_SLASH,
    KEY_RIGHTSHIFT,
    KEY_UP,
    KEY_END,
    // Row 5
    KEY_LEFTCTRL,
    KEY_LEFTMETA,
    KEY_LEFTALT,
    KEY_SPACE,
    KEY_FN,
    KEY_RIGHTCTRL,
    KEY_LEFT,
    KEY_DOWN,
    KEY_RIGHT,
};

constexpr int F75KeyCount = sizeof(F75KeyCodes) / sizeof(F75KeyCodes[0]);

}



namespace KeyCodes
{

int forF75LedIndex(int ledIndex)
{
    if(ledIndex < 0 || ledIndex >= F75KeyCount)
        return 0;

    return F75KeyCodes[ledIndex];
}



const std::vector<std::pair<std::string, int>>& targetKeyList()
{
    static const std::vector<std::pair<std::string, int>> list = {
        { "Esc", KEY_ESC },
        { "F1", KEY_F1 }, { "F2", KEY_F2 }, { "F3", KEY_F3 }, { "F4", KEY_F4 },
        { "F5", KEY_F5 }, { "F6", KEY_F6 }, { "F7", KEY_F7 }, { "F8", KEY_F8 },
        { "F9", KEY_F9 }, { "F10", KEY_F10 }, { "F11", KEY_F11 }, { "F12", KEY_F12 },
        { "`", KEY_GRAVE },
        { "1", KEY_1 }, { "2", KEY_2 }, { "3", KEY_3 }, { "4", KEY_4 }, { "5", KEY_5 },
        { "6", KEY_6 }, { "7", KEY_7 }, { "8", KEY_8 }, { "9", KEY_9 }, { "0", KEY_0 },
        { "-", KEY_MINUS }, { "=", KEY_EQUAL },
        { "Backspace", KEY_BACKSPACE },
        { "Delete", KEY_DELETE },
        { "Tab", KEY_TAB },
        { "Q", KEY_Q }, { "W", KEY_W }, { "E", KEY_E }, { "R", KEY_R }, { "T", KEY_T },
        { "Y", KEY_Y }, { "U", KEY_U }, { "I", KEY_I }, { "O", KEY_O }, { "P", KEY_P },
        { "[", KEY_LEFTBRACE }, { "]", KEY_RIGHTBRACE },
        { "\\", KEY_BACKSLASH },
        { "PgUp", KEY_PAGEUP }, { "PgDn", KEY_PAGEDOWN },
        { "Caps Lock", KEY_CAPSLOCK },
        { "A", KEY_A }, { "S", KEY_S }, { "D", KEY_D }, { "F", KEY_F }, { "G", KEY_G },
        { "H", KEY_H }, { "J", KEY_J }, { "K", KEY_K }, { "L", KEY_L },
        { ";", KEY_SEMICOLON }, { "'", KEY_APOSTROPHE },
        { "Enter", KEY_ENTER },
        { "Left Shift", KEY_LEFTSHIFT }, { "Right Shift", KEY_RIGHTSHIFT },
        { "Z", KEY_Z }, { "X", KEY_X }, { "C", KEY_C }, { "V", KEY_V }, { "B", KEY_B },
        { "N", KEY_N }, { "M", KEY_M },
        { ",", KEY_COMMA }, { ".", KEY_DOT }, { "/", KEY_SLASH },
        { "Up", KEY_UP }, { "Down", KEY_DOWN }, { "Left", KEY_LEFT }, { "Right", KEY_RIGHT },
        { "End", KEY_END }, { "Home", KEY_HOME },
        { "Left Ctrl", KEY_LEFTCTRL }, { "Right Ctrl", KEY_RIGHTCTRL },
        { "Left Alt", KEY_LEFTALT }, { "Right Alt", KEY_RIGHTALT },
        { "Win/Super", KEY_LEFTMETA },
        { "Space", KEY_SPACE },
        { "Fn", KEY_FN },
        { "Mute", KEY_MUTE }, { "Volume Down", KEY_VOLUMEDOWN }, { "Volume Up", KEY_VOLUMEUP },
        { "Play/Pause", KEY_PLAYPAUSE }, { "Next Track", KEY_NEXTSONG }, { "Previous Track", KEY_PREVIOUSSONG },
    };

    return list;
}



std::string nameForCode(int code)
{
    for(const auto& entry : targetKeyList())
        if(entry.second == code)
            return entry.first;

    return "Key " + std::to_string(code);
}

}
