#include "SystemMonitor.h"

#include <cmath>
#include <ctime>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>


namespace
{

std::string readFirstLine(const std::string& path)
{
    std::ifstream f(path);
    std::string line;
    std::getline(f, line);
    return line;
}

// CPU temperature sources: every temp*_input of a CPU hwmon driver
// (coretemp / k10temp / zenpower) - the hottest one is used, since some
// CPUs only expose per-core sensors and no package reading - else a
// thermal zone typed like a CPU package, else thermal_zone0.
std::vector<std::string> findTempPaths()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    std::vector<std::string> paths;

    for(const auto& entry : fs::directory_iterator("/sys/class/hwmon", ec))
    {
        std::string name = readFirstLine((entry.path() / "name").string());

        if(name != "coretemp" && name != "k10temp" && name != "zenpower" && name != "cpu_thermal")
            continue;

        for(const auto& f : fs::directory_iterator(entry.path(), ec))
        {
            std::string file = f.path().filename().string();
            if(file.rfind("temp", 0) == 0 && file.size() > 6 && file.compare(file.size() - 6, 6, "_input") == 0)
                paths.push_back(f.path().string());
        }

        if(!paths.empty())
            return paths;
    }

    for(const auto& entry : fs::directory_iterator("/sys/class/thermal", ec))
    {
        std::string type = readFirstLine((entry.path() / "type").string());

        if(type.find("x86_pkg") != std::string::npos || type.find("cpu") != std::string::npos)
            return { (entry.path() / "temp").string() };
    }

    return { "/sys/class/thermal/thermal_zone0/temp" };
}

double smooth(double previous, double next)
{
    return previous + (next - previous) * 0.35;
}

}



SystemMonitor::SystemMonitor()
    : tempPaths(findTempPaths())
{
}



void SystemMonitor::sample(SystemSignals& out)
{
    // --- CPU: busy share of jiffies since the previous sample ---
    double cpu = smoothed.cpu;
    {
        std::istringstream line(readFirstLine("/proc/stat"));
        std::string label;
        unsigned long long v[8] = {0};
        line >> label;
        for(auto& x : v) line >> x;

        unsigned long long idle = v[3] + v[4];
        unsigned long long total = 0;
        for(auto x : v) total += x;

        if(primed && total > lastTotal)
            cpu = 1.0 - (double)(idle - lastIdle) / (double)(total - lastTotal);

        lastIdle = idle;
        lastTotal = total;
    }

    // --- memory ---
    double memory = smoothed.memory;
    {
        std::ifstream f("/proc/meminfo");
        std::string key;
        unsigned long long value = 0, total = 0, available = 0;
        std::string unit;

        while(f >> key >> value)
        {
            std::getline(f, unit);
            if(key == "MemTotal:") total = value;
            else if(key == "MemAvailable:") available = value;
        }

        if(total > 0)
            memory = 1.0 - (double)available / (double)total;
    }

    // --- temperature: 30 degC -> 0, 95 degC -> 1 ---
    double temperature = smoothed.temperature;
    {
        double hottest = -1.0;

        for(const std::string& path : tempPaths)
        {
            std::string raw = readFirstLine(path);
            if(!raw.empty())
                hottest = std::max(hottest, std::atof(raw.c_str()) / 1000.0);
        }

        if(hottest >= 0)
            temperature = std::clamp((hottest - 30.0) / 65.0, 0.0, 1.0);
    }

    // --- network: total rx+tx bytes/s on non-loopback interfaces, log-scaled ---
    double network = smoothed.network;
    {
        std::ifstream f("/proc/net/dev");
        std::string line;
        unsigned long long bytes = 0;

        std::getline(f, line);
        std::getline(f, line);

        while(std::getline(f, line))
        {
            auto colon = line.find(':');
            if(colon == std::string::npos)
                continue;

            std::string iface = line.substr(0, colon);
            iface.erase(0, iface.find_first_not_of(' '));
            if(iface == "lo")
                continue;

            std::istringstream fields(line.substr(colon + 1));
            unsigned long long v[9] = {0};
            for(auto& x : v) fields >> x;

            bytes += v[0] + v[8];   // rx bytes, tx bytes
        }

        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - lastNetAt).count();

        if(primed && dt > 0 && bytes >= lastNetBytes)
        {
            double rate = (bytes - lastNetBytes) / dt;
            // ~1 KB/s -> 0, ~100 MB/s -> 1
            network = std::clamp((std::log10(std::max(rate, 1.0)) - 3.0) / 5.0, 0.0, 1.0);
        }

        lastNetBytes = bytes;
        lastNetAt = now;
    }

    if(primed)
    {
        smoothed.cpu = smooth(smoothed.cpu, std::clamp(cpu, 0.0, 1.0));
        smoothed.memory = memory;
        smoothed.temperature = smooth(smoothed.temperature, temperature);
        smoothed.network = smooth(smoothed.network, network);
    }
    else
    {
        smoothed.memory = memory;
        smoothed.temperature = temperature;
    }

    primed = true;

    out.cpu = smoothed.cpu;
    out.memory = smoothed.memory;
    out.temperature = smoothed.temperature;
    out.network = smoothed.network;

    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);

    out.hour = local.tm_hour;
    out.minute = local.tm_min;
    out.second = local.tm_sec;
}
