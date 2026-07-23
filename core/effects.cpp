#include "effects.h"

#include <chrono>
#include <thread>
#include <cmath>


void effect_static(
    AulaKeyboard& keyboard,
    unsigned char r,
    unsigned char g,
    unsigned char b
)
{
    keyboard.setColor(
        r,
        g,
        b
    );
}



void effect_breathing(
    AulaKeyboard& keyboard,
    unsigned char r,
    unsigned char g,
    unsigned char b
)
{
    while(true)
    {
        for(int i = 0; i < 255; i++)
        {
            float k = i / 255.0f;


            keyboard.setColor(
                r * k,
                g * k,
                b * k
            );


            std::this_thread::sleep_for(
                std::chrono::milliseconds(10)
            );
        }


        for(int i = 255; i > 0; i--)
        {
            float k = i / 255.0f;


            keyboard.setColor(
                r * k,
                g * k,
                b * k
            );


            std::this_thread::sleep_for(
                std::chrono::milliseconds(10)
            );
        }
    }
}



void hsv(
    float h,
    unsigned char& r,
    unsigned char& g,
    unsigned char& b
)
{
    float s = 1.0f;
    float v = 1.0f;


    float sector = h * 6.0f;

    int i = (int)sector;

    float f = sector - i;


    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));


    float rf, gf, bf;


    switch(i % 6)
    {
        case 0:
            rf=v; gf=t; bf=p;
            break;

        case 1:
            rf=q; gf=v; bf=p;
            break;

        case 2:
            rf=p; gf=v; bf=t;
            break;

        case 3:
            rf=p; gf=q; bf=v;
            break;

        case 4:
            rf=t; gf=p; bf=v;
            break;

        default:
            rf=v; gf=p; bf=q;
            break;
    }


    r = rf * 255;
    g = gf * 255;
    b = bf * 255;
}


void effect_rainbow(
    AulaKeyboard& keyboard
)
{
    float hue = 0;


    while(true)
    {
        unsigned char r,g,b;


        hsv(
            hue,
            r,
            g,
            b
        );


        keyboard.setColor(
            r,
            g,
            b
        );


        hue += 0.01;


        if(hue >= 1)
            hue = 0;


        std::this_thread::sleep_for(
            std::chrono::milliseconds(30)
        );
    }
}

void effect_keep(
    AulaKeyboard& keyboard,
    unsigned char r,
    unsigned char g,
    unsigned char b
)
{
    keyboard.setColor(
        r,
        g,
        b
    );


    while(true)
    {
        keyboard.resend();


        std::this_thread::sleep_for(
            std::chrono::seconds(2)
        );
    }
}
