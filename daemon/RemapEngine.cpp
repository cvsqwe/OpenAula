#include "RemapEngine.h"
#include "../core/RemapConfig.h"

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <linux/input.h>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#include <chrono>
#include <thread>
#include <iostream>
#include <string>


namespace
{

constexpr int VendorId = 0x258A;
constexpr int ProductId = 0x010C;

void sleepWhileRunning(std::atomic<bool>& running, int totalMs)
{
    for(int i = 0; i < totalMs / 100 && running; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

}



RemapEngine::RemapEngine(std::atomic<bool>& runningFlag)
    : running(runningFlag)
{
}



int RemapEngine::findKeyboardEventNode() const
{
    DIR* dir = opendir("/dev/input");

    if(!dir)
        return -1;

    int found = -1;
    struct dirent* entry;

    while((entry = readdir(dir)) != nullptr)
    {
        std::string name = entry->d_name;

        if(name.rfind("event", 0) != 0)
            continue;

        std::string path = "/dev/input/" + name;
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);

        if(fd < 0)
            continue;

        struct libevdev* dev = nullptr;

        if(libevdev_new_from_fd(fd, &dev) < 0)
        {
            close(fd);
            continue;
        }

        bool isTarget =
            libevdev_get_id_vendor(dev) == VendorId &&
            libevdev_get_id_product(dev) == ProductId &&
            libevdev_has_event_type(dev, EV_KEY) &&
            libevdev_has_event_code(dev, EV_KEY, KEY_A);

        libevdev_free(dev);

        if(isTarget)
        {
            found = fd;
            break;
        }

        close(fd);
    }

    closedir(dir);
    return found;
}



void RemapEngine::run()
{
    while(running)
    {
        int fd = findKeyboardEventNode();

        if(fd < 0)
        {
            std::cerr << "openaula-remapd: Aula F75 keyboard input node not found, retrying in 5s" << std::endl;
            sleepWhileRunning(running, 5000);
            continue;
        }

        struct libevdev* dev = nullptr;

        if(libevdev_new_from_fd(fd, &dev) < 0)
        {
            close(fd);
            continue;
        }

        std::cout << "openaula-remapd: found " << libevdev_get_name(dev) << std::endl;

        RemapConfig config;
        config.load();

        if(!config.isEnabled())
        {
            // Nothing to do yet - don't grab (grabbing is the whole risk
            // here); just wait and re-check so enabling it later from the
            // GUI takes effect without restarting this process.
            libevdev_free(dev);
            close(fd);
            sleepWhileRunning(running, 2000);
            continue;
        }

        struct libevdev_uinput* uidev = nullptr;
        int uinputRc = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);

        if(uinputRc < 0)
        {
            std::cerr << "openaula-remapd: could not create virtual keyboard (" << strerror(-uinputRc)
                       << ") - not grabbing the real keyboard without something to re-emit through. "
                          "Is /dev/uinput accessible? See daemon/60-openaula.rules." << std::endl;
            libevdev_free(dev);
            close(fd);
            sleepWhileRunning(running, 5000);
            continue;
        }

        int grabRc = libevdev_grab(dev, LIBEVDEV_GRAB);

        if(grabRc < 0)
        {
            std::cerr << "openaula-remapd: could not grab keyboard (" << strerror(-grabRc) << ")" << std::endl;
            libevdev_uinput_destroy(uidev);
            libevdev_free(dev);
            close(fd);
            sleepWhileRunning(running, 5000);
            continue;
        }

        std::cout << "openaula-remapd: grabbed, remapping active" << std::endl;

        auto lastConfigCheck = std::chrono::steady_clock::now();
        bool deviceOk = true;

        while(running && deviceOk)
        {
            auto now = std::chrono::steady_clock::now();

            if(now - lastConfigCheck > std::chrono::seconds(1))
            {
                lastConfigCheck = now;
                config.load();

                if(!config.isEnabled())
                {
                    std::cout << "openaula-remapd: disabled - releasing keyboard" << std::endl;
                    break;
                }
            }

            struct input_event ev;
            int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

            if(rc == -EAGAIN)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(4));
                continue;
            }

            if(rc == LIBEVDEV_READ_STATUS_SYNC)
            {
                // Dropped events (buffer overrun): drain the forced-sync
                // queue and keep going - nothing here depends on ordering
                // across a drop.
                while(rc == LIBEVDEV_READ_STATUS_SYNC)
                    rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);

                continue;
            }

            if(rc < 0)
            {
                std::cerr << "openaula-remapd: read error (" << strerror(-rc) << "), releasing keyboard" << std::endl;
                deviceOk = false;
                break;
            }

            if(ev.type != EV_KEY)
            {
                libevdev_uinput_write_event(uidev, ev.type, ev.code, ev.value);
                continue;
            }

            const KeyBinding* binding = config.bindingFor(ev.code);

            if(!binding || binding->type == BindingType::Passthrough)
            {
                libevdev_uinput_write_event(uidev, ev.type, ev.code, ev.value);
                continue;
            }

            if(binding->type == BindingType::Disabled)
            {
                continue;
            }

            if(binding->type == BindingType::Remap)
            {
                libevdev_uinput_write_event(uidev, EV_KEY, binding->remapKeyCode, ev.value);
                libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
                continue;
            }

            if(binding->type == BindingType::Macro)
            {
                // Fire once per physical press only - ignore the release
                // and any auto-repeat (value 2) so holding the key down
                // doesn't replay the macro.
                if(ev.value != 1)
                    continue;

                for(const MacroStep& step : binding->macro)
                {
                    libevdev_uinput_write_event(uidev, EV_KEY, step.keyCode, step.press ? 1 : 0);
                    libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);

                    if(step.delayAfterMs > 0)
                        std::this_thread::sleep_for(std::chrono::milliseconds(step.delayAfterMs));
                }

                continue;
            }
        }

        libevdev_grab(dev, LIBEVDEV_UNGRAB);
        libevdev_uinput_destroy(uidev);
        libevdev_free(dev);
        close(fd);
    }
}
