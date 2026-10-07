#pragma once

#include <hidapi/hidapi.h>

#include <vector>
#include <mutex>

#include "Color.h"


class AulaKeyboard
{

private:

    hid_device* device = nullptr;

    std::vector<unsigned char> lastPacket;

    // keep-alive thread and the rest can both send
    std::mutex deviceMutex;


public:

    bool connect();


    void setColor(
        unsigned char r,
        unsigned char g,
        unsigned char b
    );


    // one colour per LED slot
    void setColors(
        const std::vector<Color>& colors
    );


    void resend();


    void close();

};
