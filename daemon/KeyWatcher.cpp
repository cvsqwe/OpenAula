#include "KeyWatcher.h"

#include "../core/KeyCodes.h"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <string>


namespace
{

constexpr const char* AulaVendor = "258a";
constexpr const char* AulaProduct = "010c";

std::string readId(const std::filesystem::path& p)
{
    std::ifstream f(p);
    std::string s;
    f >> s;
    return s;
}

std::vector<int> openKeyboardNodes()
{
    namespace fs = std::filesystem;
    std::vector<int> fds;
    std::error_code ec;

    for(const auto& entry : fs::directory_iterator("/sys/class/input", ec))
    {
        std::string name = entry.path().filename().string();
        if(name.rfind("event", 0) != 0)
            continue;

        fs::path id = entry.path() / "device" / "id";
        if(readId(id / "vendor") != AulaVendor || readId(id / "product") != AulaProduct)
            continue;

        int fd = open(("/dev/input/" + name).c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if(fd >= 0)
            fds.push_back(fd);
    }

    return fds;
}

}



KeyWatcher::KeyWatcher(int keyCount)
    : lastPress(keyCount), pressedEver(keyCount, false), codeToKey(KEY_MAX + 1, -1)
{
    for(int i = 0; i < keyCount; i++)
    {
        int code = KeyCodes::forF75LedIndex(i);
        if(code > 0 && code <= KEY_MAX)
            codeToKey[code] = i;
    }
}



KeyWatcher::~KeyWatcher()
{
    stop();
}



void KeyWatcher::start()
{
    if(running.exchange(true))
        return;

    worker = std::thread(&KeyWatcher::run, this);
}



void KeyWatcher::stop()
{
    running = false;

    if(worker.joinable())
        worker.join();
}



void KeyWatcher::run()
{
    std::vector<int> fds;
    auto lastScan = Clock::now() - std::chrono::seconds(10);

    while(running)
    {
        if(fds.empty() && Clock::now() - lastScan > std::chrono::seconds(3))
        {
            fds = openKeyboardNodes();
            lastScan = Clock::now();
        }

        if(fds.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
        }

        std::vector<pollfd> pfds;
        for(int fd : fds)
            pfds.push_back(pollfd{fd, POLLIN, 0});

        int ready = poll(pfds.data(), pfds.size(), 250);
        bool lost = false;

        for(size_t i = 0; ready > 0 && i < pfds.size(); i++)
        {
            if(pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                lost = true;
                continue;
            }

            if(!(pfds[i].revents & POLLIN))
                continue;

            input_event ev[64];
            ssize_t n;

            while((n = read(pfds[i].fd, ev, sizeof(ev))) > 0)
            {
                std::lock_guard<std::mutex> lock(mutex);

                for(size_t e = 0; e < n / sizeof(input_event); e++)
                {
                    // 1 = press, 2 = autorepeat: both keep the key "lit"
                    if(ev[e].type == EV_KEY && ev[e].value >= 1 && ev[e].code <= KEY_MAX)
                    {
                        int key = codeToKey[ev[e].code];
                        if(key >= 0)
                        {
                            lastPress[key] = Clock::now();
                            pressedEver[key] = true;
                        }
                    }
                }
            }

            if(n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                lost = true;
        }

        // Lock-key LEDs: ask the nodes directly rather than tracking
        // EV_LED events, so the state is right even if it changed before
        // we opened the device.
        unsigned long leds[(LED_MAX + 1 + 8 * sizeof(unsigned long) - 1) / (8 * sizeof(unsigned long))];
        bool c = false, nl = false, s = false;

        for(int fd : fds)
        {
            std::memset(leds, 0, sizeof(leds));
            if(ioctl(fd, EVIOCGLED(sizeof(leds)), leds) < 0)
                continue;

            auto bit = [&](int b) { return (leds[b / (8 * sizeof(unsigned long))] >> (b % (8 * sizeof(unsigned long)))) & 1UL; };
            c = c || bit(LED_CAPSL);
            nl = nl || bit(LED_NUML);
            s = s || bit(LED_SCROLLL);
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            caps = c; num = nl; scroll = s;
        }

        if(lost)
        {
            for(int fd : fds)
                close(fd);

            fds.clear();
        }
    }

    for(int fd : fds)
        close(fd);
}



void KeyWatcher::snapshot(SystemSignals& out)
{
    std::lock_guard<std::mutex> lock(mutex);
    auto now = Clock::now();

    out.keyAge.assign(lastPress.size(), 1e9);

    for(size_t i = 0; i < lastPress.size(); i++)
        if(pressedEver[i])
            out.keyAge[i] = std::chrono::duration<double>(now - lastPress[i]).count();

    out.capsLock = caps;
    out.numLock = num;
    out.scrollLock = scroll;
}
