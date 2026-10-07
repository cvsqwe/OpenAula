// openaula-remapd - key remaps / macros. opt-in, see RemapEngine.h

#include "RemapEngine.h"

#include <csignal>
#include <atomic>


namespace
{

std::atomic<bool> running{true};

void handleSignal(int)
{
    running = false;
}

}



int main()
{
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    RemapEngine engine(running);
    engine.run();

    return 0;
}
