#include "AulaKeyboard.h"
#include "protocol.h"


bool AulaKeyboard::connect()
{
    hid_init();


    auto list = hid_enumerate(
        0x258A,
        0x010C
    );


    for(auto d = list; d; d = d->next)
    {

        if(d->usage_page == 0xff00)
        {

            device =
                hid_open_path(
                    d->path
                );

            break;
        }
    }


    hid_free_enumeration(
        list
    );


    return device != nullptr;
}



void AulaKeyboard::setColor(
    unsigned char r,
    unsigned char g,
    unsigned char b
)
{

    {
        std::lock_guard<std::mutex> lock(deviceMutex);

        lastPacket =
            AulaProtocol::setColor(
                r,
                g,
                b
            );
    }


    resend();

}



void AulaKeyboard::setColors(
    const std::vector<Color>& colors
)
{

    {
        std::lock_guard<std::mutex> lock(deviceMutex);

        lastPacket =
            AulaProtocol::setColors(
                colors
            );
    }


    resend();

}



void AulaKeyboard::resend()
{

    std::lock_guard<std::mutex> lock(deviceMutex);


    if(!device)
        return;


    if(lastPacket.empty())
        return;



    hid_send_feature_report(
        device,
        lastPacket.data(),
        lastPacket.size()
    );

}



void AulaKeyboard::close()
{

    std::lock_guard<std::mutex> lock(deviceMutex);


    if(device)
    {
        hid_close(
            device
        );

        device = nullptr;
    }


    hid_exit();

}
