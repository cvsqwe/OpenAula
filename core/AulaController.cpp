#include "AulaController.h"


#include <chrono>



AulaController::~AulaController()
{
    stop();
}



bool AulaController::connect()
{
    return keyboard.connect();
}



void AulaController::setColor(
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



void AulaController::setColors(
    const std::vector<Color>& colors
)
{

    keyboard.setColors(
        colors
    );

}



void AulaController::startKeepAlive()
{

    if(running)
        return;


    running = true;



    worker =
    std::thread(
        [this]()
        {

            while(running)
            {

                keyboard.resend();


                std::this_thread::sleep_for(
                    std::chrono::seconds(2)
                );

            }

        }
    );

}



void AulaController::stop()
{

    running = false;


    if(worker.joinable())
        worker.join();


    keyboard.close();

}



AulaKeyboard& AulaController::getKeyboard()
{
    return keyboard;
}
