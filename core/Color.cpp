#include "Color.h"

#include <cstdio>


Color Color::fromHex(
    const char* hex
)
{
    Color c{0,0,0};


    sscanf(
        hex,
        "%02hhx%02hhx%02hhx",
        &c.r,
        &c.g,
        &c.b
    );


    return c;
}
