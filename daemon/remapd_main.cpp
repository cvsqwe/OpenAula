// openaula-remapd: optional key-remap/macro engine for the Aula F75.
// Separate from openaula-daemon (lighting) on purpose - see
// RemapEngine.h for why. Not installed/started unless the user opts in
// via the GUI's Macros & Remap panel (or daemon/install-remap.sh), since
// it needs elevated device access (see 60-openaula.rules) that plain
// lighting control does not.

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
