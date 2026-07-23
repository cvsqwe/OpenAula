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


    // Joins the keep-alive thread if it's still running, so destroying a
    // connected controller (e.g. closing the app) can't call
    // std::terminate() from an unjoined std::thread.
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
