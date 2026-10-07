#include "KeyboardLayout.h"


std::vector<KeyDef> buildF75Layout()
{
    std::vector<KeyDef> keys;

    auto add = [&](const std::string& label, double x, double y, double w = 1.0, double h = 1.0)
    {
        keys.push_back(KeyDef{label, x, y, w, h, (int)keys.size()});
    };


    // row 0, the knob top-right isn't a key
    add("Esc", 0, 0);
    add("F1", 2, 0);  add("F2", 3, 0);  add("F3", 4, 0);  add("F4", 5, 0);
    add("F5", 6.5, 0); add("F6", 7.5, 0); add("F7", 8.5, 0); add("F8", 9.5, 0);
    add("F9", 11, 0); add("F10", 12, 0); add("F11", 13, 0); add("F12", 14, 0);

    // row 1, Delete at the end
    add("`", 0, 1.25);
    add("1", 1, 1.25); add("2", 2, 1.25); add("3", 3, 1.25); add("4", 4, 1.25);
    add("5", 5, 1.25); add("6", 6, 1.25); add("7", 7, 1.25); add("8", 8, 1.25);
    add("9", 9, 1.25); add("0", 10, 1.25);
    add("-", 11, 1.25); add("=", 12, 1.25);
    add("Backspace", 13, 1.25, 2.0);
    add("Delete", 15.25, 1.25);

    // row 2
    add("Tab", 0, 2.25, 1.5);
    add("Q", 1.5, 2.25); add("W", 2.5, 2.25); add("E", 3.5, 2.25); add("R", 4.5, 2.25);
    add("T", 5.5, 2.25); add("Y", 6.5, 2.25); add("U", 7.5, 2.25); add("I", 8.5, 2.25);
    add("O", 9.5, 2.25); add("P", 10.5, 2.25);
    add("[", 11.5, 2.25); add("]", 12.5, 2.25);
    add("\\", 13.5, 2.25, 1.75);
    add("PgUp", 15.25, 2.25);

    // row 3
    add("Caps", 0, 3.25, 1.75);
    add("A", 1.75, 3.25); add("S", 2.75, 3.25); add("D", 3.75, 3.25); add("F", 4.75, 3.25);
    add("G", 5.75, 3.25); add("H", 6.75, 3.25); add("J", 7.75, 3.25); add("K", 8.75, 3.25);
    add("L", 9.75, 3.25);
    add(";", 10.75, 3.25); add("'", 11.75, 3.25);
    add("Enter", 12.75, 3.25, 2.5);
    add("PgDn", 15.25, 3.25);

    // row 4
    add("Shift", 0, 4.25, 2.25);
    add("Z", 2.25, 4.25); add("X", 3.25, 4.25); add("C", 4.25, 4.25); add("V", 5.25, 4.25);
    add("B", 6.25, 4.25); add("N", 7.25, 4.25); add("M", 8.25, 4.25);
    add(",", 9.25, 4.25); add(".", 10.25, 4.25); add("/", 11.25, 4.25);
    add("Shift", 12.25, 4.25, 2.0);
    add("Up", 14.25, 4.25);
    add("End", 15.25, 4.25);

    // row 5, no right Alt (Fn is there)
    add("Ctrl", 0, 5.25, 1.25);
    add("Win", 1.25, 5.25, 1.25);
    add("Alt", 2.5, 5.25, 1.25);
    add("Space", 3.75, 5.25, 6.75);
    add("Fn", 10.5, 5.25, 1.25);
    add("Ctrl", 11.75, 5.25, 1.25);
    add("Left", 13.25, 5.25);
    add("Down", 14.25, 5.25);
    add("Right", 15.25, 5.25);

    return keys;
}



KnobGeometry f75Knob()
{
    return KnobGeometry{ 15.55, 0.05, 0.9 };
}
