#pragma once


#include "AulaKeyboard.h"


#include <thread>
#include <atomic>



class AulaController
{

private:

    AulaKeyboard keyboard;


    std::thread worker;


    std::atomic<bool> running =
        false;



public:


    // joins the keep-alive thread so we don't std::terminate
    ~AulaController();


    bool connect();


    void setColor(
        unsigned char r,
        unsigned char g,
        unsigned char b
    );


    void setColors(
        const std::vector<Color>& colors
    );


    void startKeepAlive();


    void stop();


    AulaKeyboard& getKeyboard();

};
