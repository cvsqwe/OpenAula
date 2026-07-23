#pragma once

struct Color
{
    unsigned char r;
    unsigned char g;
    unsigned char b;


    static Color fromHex(
        const char* hex
    );
};
