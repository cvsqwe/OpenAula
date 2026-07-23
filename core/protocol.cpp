#include "protocol.h"


namespace AulaProtocol
{


std::vector<unsigned char> setColor(
    unsigned char r,
    unsigned char g,
    unsigned char b
)
{
    std::vector<unsigned char> buf(520, 0);


    buf[0x00] = 0x06;
    buf[0x01] = 0x08;
    buf[0x04] = 0x01;
    buf[0x06] = 0x7A;
    buf[0x07] = 0x01;


    for(int i = 0; i < 90; i++)
    {
        buf[8 + i * 3]     = r;
        buf[9 + i * 3]     = g;
        buf[10 + i * 3]    = b;
    }


    return buf;
}



std::vector<unsigned char> setColors(
    const std::vector<Color>& colors
)
{
    std::vector<unsigned char> buf(520, 0);


    buf[0x00] = 0x06;
    buf[0x01] = 0x08;
    buf[0x04] = 0x01;
    buf[0x06] = 0x7A;
    buf[0x07] = 0x01;


    for(int i = 0; i < 90; i++)
    {
        unsigned char r = 0;
        unsigned char g = 0;
        unsigned char b = 0;

        if(i < (int)colors.size())
        {
            r = colors[i].r;
            g = colors[i].g;
            b = colors[i].b;
        }

        buf[8 + i * 3]     = r;
        buf[9 + i * 3]     = g;
        buf[10 + i * 3]    = b;
    }


    return buf;
}



std::vector<unsigned char> getInfo()
{
    return
    {
        0x06,
        0x82,
        0x01,
        0x00,
        0x01,
        0x00,
        0x06
    };
}



std::vector<unsigned char> setMode(
    unsigned char mode
)
{
    std::vector<unsigned char> buf(208,0);


    buf[0] = 0x06;
    buf[1] = 0x08;

    buf[4] = 0x01;

    buf[6] = mode;


    return buf;
}


}
