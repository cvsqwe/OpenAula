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

    // Guards device + lastPacket: the keep-alive thread and any effect/UI
    // thread can both send reports concurrently.
    std::mutex deviceMutex;


public:

    bool connect();


    void setColor(
        unsigned char r,
        unsigned char g,
        unsigned char b
    );


    // Sets each key's LED individually (index == protocol LED slot).
    void setColors(
        const std::vector<Color>& colors
    );


    void resend();


    void close();

};
