// openaula-webd - web ui + json api.
// never opens the keyboard itself, just writes the state files and
// pokes openaula-daemon with SIGUSR1.

#include "HttpServer.h"
#include "Json.h"

#include "../core/AppState.h"
#include "../core/ProfileStore.h"
#include "../core/KeyboardLayout.h"
#include "../core/StateFormat.h"
#include "../core/Color.h"
#include "../core/RemapConfig.h"
#include "../core/KeyCodes.h"
#include "../core/CalibrationSession.h"
#include "../core/SystemMonitor.h"
#include "../core/Layer.h"

#include <cstdlib>
#include <cstring>
#include <csignal>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <mutex>

#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


namespace
{

int gKeyCount = 0;

// same as TotalPhysicalLeds in daemon/main.cpp
constexpr int TotalPhysicalLeds = 90;


// --- json helpers ---

std::string jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());

    for(char c : s)
    {
        switch(c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if((unsigned char)c < 0x20)
                    out += ' ';
                else
                    out += c;
        }
    }

    return out;
}

std::string jsonColor(const Color& c)
{
    std::ostringstream ss;
    ss << "{\"r\":" << (int)c.r << ",\"g\":" << (int)c.g << ",\"b\":" << (int)c.b << "}";
    return ss.str();
}

std::string jsonColors(const std::vector<Color>& colors)
{
    std::ostringstream ss;
    ss << "[";

    for(size_t i = 0; i < colors.size(); i++)
    {
        if(i > 0) ss << ",";
        ss << jsonColor(colors[i]);
    }

    ss << "]";
    return ss.str();
}

Color colorFromJson(const json::Value& v)
{
    Color c{0, 0, 0};

    if(v.type() != json::Value::Type::Object)
        return c;

    auto clamp255 = [](double x) { return (unsigned char)std::max(0.0, std::min(255.0, x)); };

    c.r = clamp255(v["r"].asNumber(0));
    c.g = clamp255(v["g"].asNumber(0));
    c.b = clamp255(v["b"].asNumber(0));
    return c;
}


// --- daemon start/stop ---

std::vector<pid_t> daemonPids()
{
    std::vector<pid_t> pids;

    FILE* pipe = popen("pgrep -x openaula-daemon", "r");
    if(!pipe)
        return pids;

    char line[64];
    while(fgets(line, sizeof(line), pipe))
    {
        pid_t pid = (pid_t)std::strtol(line, nullptr, 10);
        if(pid > 0)
            pids.push_back(pid);
    }

    pclose(pipe);
    return pids;
}

bool isDaemonRunning()
{
    return !daemonPids().empty();
}

void nudgeDaemon()
{
    for(pid_t pid : daemonPids())
        kill(pid, SIGUSR1);
}

bool startDaemon()
{
    if(isDaemonRunning())
        return true;

    pid_t pid = fork();

    if(pid < 0)
        return false;

    if(pid == 0)
    {
        // child: new session so it outlives webd
        setsid();
        execlp("openaula-daemon", "openaula-daemon", (char*)nullptr);
        _exit(127); // execlp only returns on failure
    }

    return true;
}

void stopDaemon()
{
    for(pid_t pid : daemonPids())
        kill(pid, SIGTERM);
}


// --- remapd start/stop ---

std::vector<pid_t> remapdPids()
{
    std::vector<pid_t> pids;

    FILE* pipe = popen("pgrep -x openaula-remapd", "r");
    if(!pipe)
        return pids;

    char line[64];
    while(fgets(line, sizeof(line), pipe))
    {
        pid_t pid = (pid_t)std::strtol(line, nullptr, 10);
        if(pid > 0)
            pids.push_back(pid);
    }

    pclose(pipe);
    return pids;
}

bool isRemapdRunning()
{
    return !remapdPids().empty();
}

bool isRemapdInstalled()
{
    FILE* pipe = popen("command -v openaula-remapd 2>/dev/null", "r");
    if(!pipe)
        return false;

    char line[512];
    bool found = fgets(line, sizeof(line), pipe) != nullptr;
    pclose(pipe);
    return found;
}

bool startRemapd()
{
    if(isRemapdRunning())
        return true;

    if(!isRemapdInstalled())
        return false;

    pid_t pid = fork();

    if(pid < 0)
        return false;

    if(pid == 0)
    {
        setsid();
        execlp("openaula-remapd", "openaula-remapd", (char*)nullptr);
        _exit(127);
    }

    return true;
}

void stopRemapd()
{
    for(pid_t pid : remapdPids())
        kill(pid, SIGTERM);
}


// --- layers <-> JSON ---

std::string layersToJson(const std::vector<Layer>& layers)
{
    std::ostringstream ss;
    ss << "[";

    for(size_t i = 0; i < layers.size(); i++)
    {
        const Layer& l = layers[i];
        if(i > 0) ss << ",";

        ss << "{\"effect\":\"" << StateFormat::modeToString(l.effect) << "\","
           << "\"color\":" << jsonColor(l.color) << ","
           << "\"speed\":" << l.speed << ","
           << "\"opacity\":" << l.opacity << ","
           << "\"blend\":\"" << StateFormat::blendToString(l.blend) << "\","
           << "\"enabled\":" << (l.enabled ? "true" : "false") << ","
           << "\"mask\":\"" << StateFormat::maskToString(l.mask) << "\"}";
    }

    ss << "]";
    return ss.str();
}

std::vector<Layer> layersFromJson(const json::Value& arr)
{
    std::vector<Layer> layers;

    // cap it, just in case
    for(const json::Value& item : arr.items())
    {
        if(layers.size() >= 16)
            break;

        Layer l;
        l.effect = StateFormat::modeFromString(item["effect"].asString("custom"));
        l.color = colorFromJson(item["color"]);
        l.speed = std::max(0.1, std::min(3.0, item["speed"].asNumber(1.0)));
        l.opacity = std::max(0.0, std::min(1.0, item["opacity"].asNumber(1.0)));
        l.blend = StateFormat::blendFromString(item["blend"].asString("normal"));
        l.enabled = item["enabled"].asBool(true);
        l.mask = StateFormat::maskFromString(item["mask"].asString("*"), gKeyCount);
        layers.push_back(l);
    }

    return layers;
}


// --- state <-> JSON ---

std::string stateToJson(const AppState& state)
{
    std::ostringstream ss;

    ss << "{"
       << "\"mode\":\"" << StateFormat::modeToString(state.mode()) << "\","
       << "\"speed\":" << state.speed() << ","
       << "\"brightness\":" << state.brightness() << ","
       << "\"activeColor\":" << jsonColor(state.activeColor()) << ","
       << "\"customColors\":" << jsonColors(state.customColorsRef()) << ","
       << "\"layers\":" << layersToJson(state.layers()) << ","
       << "\"activeProfile\":\"" << jsonEscape(state.activeProfile()) << "\","
       << "\"calibrated\":" << (state.isCalibrated() ? "true" : "false") << ","
       << "\"daemonRunning\":" << (isDaemonRunning() ? "true" : "false")
       << "}";

    return ss.str();
}

std::string calibrationToJson(const CalibrationSession& calib, bool calibrated)
{
    std::ostringstream ss;
    ss << "{\"active\":" << (calib.active() ? "true" : "false") << ","
       << "\"step\":" << calib.step() << ","
       << "\"total\":" << calib.total() << ","
       << "\"calibrated\":" << (calibrated ? "true" : "false") << "}";
    return ss.str();
}

std::string layoutToJson()
{
    std::vector<KeyDef> keys = buildF75Layout();
    KnobGeometry knob = f75Knob();

    std::ostringstream ss;
    ss << "{\"keys\":[";

    for(size_t i = 0; i < keys.size(); i++)
    {
        const KeyDef& k = keys[i];
        if(i > 0) ss << ",";

        ss << "{\"label\":\"" << jsonEscape(k.label) << "\","
           << "\"x\":" << k.x << ",\"y\":" << k.y << ",\"w\":" << k.w << ",\"h\":" << k.h << ","
           << "\"ledIndex\":" << k.ledIndex << "}";
    }

    ss << "],\"knob\":{\"x\":" << knob.x << ",\"y\":" << knob.y << ",\"diameter\":" << knob.diameter << "},"
       << "\"keyCount\":" << keys.size() << "}";

    return ss.str();
}

std::string profilesToJson(const ProfileStore& profiles)
{
    std::ostringstream ss;
    ss << "{\"activeIndex\":" << profiles.activeProfileIndex() << ",\"profiles\":[";

    const auto& list = profiles.all();

    for(size_t i = 0; i < list.size(); i++)
    {
        const Profile& p = list[i];
        if(i > 0) ss << ",";

        ss << "{\"name\":\"" << jsonEscape(p.name) << "\","
           << "\"mode\":\"" << StateFormat::modeToString(p.mode) << "\","
           << "\"speed\":" << p.speed << ","
           << "\"brightness\":" << p.brightness << ","
           << "\"activeColor\":" << jsonColor(p.activeColor) << "}";
    }

    ss << "]}";
    return ss.str();
}


// --- remap/macro config <-> JSON ---

std::string macroStepToJson(const MacroStep& step)
{
    std::ostringstream ss;
    ss << "{\"code\":" << step.keyCode << ",\"name\":\"" << jsonEscape(KeyCodes::nameForCode(step.keyCode)) << "\","
       << "\"press\":" << (step.press ? "true" : "false") << ",\"delayAfterMs\":" << step.delayAfterMs << "}";
    return ss.str();
}

std::string bindingToJson(const KeyBinding& b)
{
    std::ostringstream ss;
    ss << "{\"key\":" << b.physicalKeyCode << ",\"keyName\":\"" << jsonEscape(KeyCodes::nameForCode(b.physicalKeyCode)) << "\","
       << "\"type\":\"" << bindingTypeToString(b.type) << "\","
       << "\"remap\":" << b.remapKeyCode << ",\"remapName\":\"" << jsonEscape(KeyCodes::nameForCode(b.remapKeyCode)) << "\","
       << "\"macro\":[";

    for(size_t i = 0; i < b.macro.size(); i++)
    {
        if(i > 0) ss << ",";
        ss << macroStepToJson(b.macro[i]);
    }

    ss << "]}";
    return ss.str();
}

std::string targetKeysToJson()
{
    std::ostringstream ss;
    ss << "[";

    const auto& list = KeyCodes::targetKeyList();
    for(size_t i = 0; i < list.size(); i++)
    {
        if(i > 0) ss << ",";
        ss << "{\"name\":\"" << jsonEscape(list[i].first) << "\",\"code\":" << list[i].second << "}";
    }

    ss << "]";
    return ss.str();
}

std::string remapConfigToJson(const RemapConfig& config)
{
    std::ostringstream ss;
    ss << "{\"enabled\":" << (config.isEnabled() ? "true" : "false") << ","
       << "\"running\":" << (isRemapdRunning() ? "true" : "false") << ","
       << "\"installed\":" << (isRemapdInstalled() ? "true" : "false") << ","
       << "\"targetKeys\":" << targetKeysToJson() << ","
       << "\"bindings\":[";

    const auto& bindings = config.all();
    for(size_t i = 0; i < bindings.size(); i++)
    {
        if(i > 0) ss << ",";
        ss << bindingToJson(bindings[i]);
    }

    ss << "]}";
    return ss.str();
}

std::vector<MacroStep> macroFromJson(const json::Value& arr)
{
    std::vector<MacroStep> steps;

    for(const json::Value& item : arr.items())
    {
        MacroStep step;
        step.keyCode = (int)item["code"].asNumber(0);
        step.press = item["press"].asBool(true);
        step.delayAfterMs = std::max(0, (int)item["delayAfterMs"].asNumber(0));
        steps.push_back(step);
    }

    return steps;
}


// --- web root resolution ---

std::string exeDir()
{
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if(len <= 0)
        return ".";

    buf[len] = '\0';
    return std::filesystem::path(buf).parent_path().string();
}

std::string resolveWebRoot()
{
    if(const char* override = std::getenv("OPENAULA_WEB_ROOT"))
        return override;

    std::vector<std::string> candidates = {
        exeDir() + "/../web",              // running from build/ next to the source tree
        exeDir() + "/web",                 // installed layout: binary and web/ side by side
        "/usr/local/share/openaula/web",
        "/usr/share/openaula/web",
    };

    for(const std::string& candidate : candidates)
    {
        if(std::filesystem::exists(std::filesystem::path(candidate) / "index.html"))
            return candidate;
    }

    return candidates.front();
}


void printAccessUrls(int port)
{
    std::cout << "openaula-webd: listening on port " << port << "\n";
    std::cout << "  http://localhost:" << port << "/\n";

    ifaddrs* addrs = nullptr;
    if(getifaddrs(&addrs) != 0)
        return;

    for(ifaddrs* it = addrs; it != nullptr; it = it->ifa_next)
    {
        if(!it->ifa_addr || it->ifa_addr->sa_family != AF_INET)
            continue;

        // Loopback already printed above under a friendlier name.
        if(it->ifa_flags & IFF_LOOPBACK)
            continue;

        char ip[INET_ADDRSTRLEN];
        auto* sin = (sockaddr_in*)it->ifa_addr;
        inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));

        std::cout << "  http://" << ip << ":" << port << "/  (" << it->ifa_name << ")\n";
    }

    freeifaddrs(addrs);
}

}



int main(int argc, char** argv)
{
    // don't leave zombies from the daemons we fork
    std::signal(SIGCHLD, SIG_IGN);

    int port = 8787;

    if(const char* envPort = std::getenv("OPENAULA_WEB_PORT"))
        port = std::atoi(envPort);

    if(argc > 1)
        port = std::atoi(argv[1]);

    gKeyCount = (int)buildF75Layout().size();

    HttpServer server;
    server.serveStatic(resolveWebRoot());


    server.get("/api/state", [](const HttpRequest&, HttpResponse& res)
    {
        AppState state;
        state.load(gKeyCount);
        res.body = stateToJson(state);
    });

    server.get("/api/layout", [](const HttpRequest&, HttpResponse& res)
    {
        res.body = layoutToJson();
    });

    server.get("/api/profiles", [](const HttpRequest&, HttpResponse& res)
    {
        ProfileStore profiles;
        profiles.load(gKeyCount);
        res.body = profilesToJson(profiles);
    });

    server.get("/api/daemon/status", [](const HttpRequest&, HttpResponse& res)
    {
        res.body = std::string("{\"running\":") + (isDaemonRunning() ? "true" : "false") + "}";
    });


    server.post("/api/mode", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);

        AppState state;
        state.load(gKeyCount);
        state.setMode(StateFormat::modeFromString(body["mode"].asString("custom")));
        state.setLayers(StateFormat::legacyLayers(state.mode(), state.activeColor(), state.speed()));
        state.save();
        nudgeDaemon();

        res.body = stateToJson(state);
    });

    server.post("/api/speed", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);

        AppState state;
        state.load(gKeyCount);
        state.setSpeed(std::max(0.1, std::min(3.0, body["speed"].asNumber(1.0))));
        state.save();
        nudgeDaemon();

        res.body = stateToJson(state);
    });

    server.post("/api/brightness", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);

        AppState state;
        state.load(gKeyCount);
        state.setBrightness(std::max(0.0, std::min(1.0, body["brightness"].asNumber(1.0))));
        state.save();
        nudgeDaemon();

        res.body = stateToJson(state);
    });

    server.post("/api/color", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);

        AppState state;
        state.load(gKeyCount);
        state.setActiveColor(colorFromJson(body));
        state.save();
        nudgeDaemon();

        res.body = stateToJson(state);
    });

    server.post("/api/layers", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);

        AppState state;
        state.load(gKeyCount);
        state.setLayers(layersFromJson(body["layers"]));

        // painting sends colours + canvas mask together
        if(body["customColors"].type() == json::Value::Type::Array)
        {
            std::vector<Color> colors = state.customColorsRef();
            const auto& items = body["customColors"].items();

            for(int i = 0; i < gKeyCount && i < (int)items.size() && i < (int)colors.size(); i++)
                colors[i] = colorFromJson(items[i]);

            state.setCustomColors(colors);
        }

        state.save();
        nudgeDaemon();

        res.body = stateToJson(state);
    });

    // metrics for the browser preview of the system effects
    server.get("/api/system", [](const HttpRequest&, HttpResponse& res)
    {
        static std::mutex monitorMutex;
        static SystemMonitor monitor;
        static SystemSignals signals;

        std::lock_guard<std::mutex> lock(monitorMutex);
        monitor.sample(signals);

        std::ostringstream ss;
        ss << "{\"cpu\":" << signals.cpu << ",\"memory\":" << signals.memory
           << ",\"temperature\":" << signals.temperature << ",\"network\":" << signals.network
           << ",\"hour\":" << signals.hour << ",\"minute\":" << signals.minute
           << ",\"second\":" << signals.second << "}";
        res.body = ss.str();
    });

    server.post("/api/custom-colors", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);

        AppState state;
        state.load(gKeyCount);

        std::vector<Color> colors(gKeyCount, Color{24, 24, 28});
        const auto& items = body["colors"].items();

        for(int i = 0; i < gKeyCount && i < (int)items.size(); i++)
            colors[i] = colorFromJson(items[i]);

        state.setCustomColors(colors);
        state.save();
        nudgeDaemon();

        res.body = stateToJson(state);
    });

    server.post("/api/custom-color", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);

        AppState state;
        state.load(gKeyCount);

        std::vector<Color> colors = state.customColorsRef();
        int index = (int)body["index"].asNumber(-1);

        if(index >= 0 && index < (int)colors.size())
            colors[index] = colorFromJson(body);

        state.setCustomColors(colors);
        state.save();
        nudgeDaemon();

        res.body = stateToJson(state);
    });


    server.post("/api/profiles/save", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);
        std::string name = body["name"].asString("Untitled");

        AppState state;
        state.load(gKeyCount);

        Profile p;
        p.name = name;
        p.mode = state.mode();
        p.speed = state.speed();
        p.brightness = state.brightness();
        p.activeColor = state.activeColor();
        p.customColors = state.customColorsRef();
        p.layers = state.layers();

        ProfileStore profiles;
        profiles.load(gKeyCount);

        int index = profiles.indexByName(name);
        if(index >= 0)
            profiles.updateProfile(index, p);
        else
            index = profiles.addProfile(p);

        profiles.setActiveProfileIndex(index);
        profiles.save();

        state.setActiveProfile(name);
        state.save();

        res.body = profilesToJson(profiles);
    });

    server.post("/api/profiles/apply", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);
        int index = (int)body["index"].asNumber(-1);

        ProfileStore profiles;
        profiles.load(gKeyCount);

        const auto& list = profiles.all();
        if(index < 0 || index >= (int)list.size())
        {
            res.status = 400;
            res.body = "{\"error\":\"invalid profile index\"}";
            return;
        }

        const Profile& p = list[index];

        AppState state;
        state.load(gKeyCount);
        state.setMode(p.mode);
        state.setSpeed(p.speed);
        state.setBrightness(p.brightness);
        state.setActiveColor(p.activeColor);
        state.setCustomColors(p.customColors);
        state.setLayers(p.layers);
        state.setActiveProfile(p.name);
        state.save();

        profiles.setActiveProfileIndex(index);
        profiles.save();

        nudgeDaemon();

        res.body = stateToJson(state);
    });

    server.post("/api/profiles/rename", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);
        int index = (int)body["index"].asNumber(-1);
        std::string name = body["name"].asString("Untitled");

        ProfileStore profiles;
        profiles.load(gKeyCount);
        profiles.renameProfile(index, name);
        profiles.save();

        res.body = profilesToJson(profiles);
    });

    server.post("/api/profiles/delete", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);
        int index = (int)body["index"].asNumber(-1);

        ProfileStore profiles;
        profiles.load(gKeyCount);
        profiles.removeProfile(index);
        profiles.save();

        res.body = profilesToJson(profiles);
    });


    server.get("/api/calibration", [](const HttpRequest&, HttpResponse& res)
    {
        CalibrationSession calib;
        calib.load();

        AppState state;
        state.load(gKeyCount);

        res.body = calibrationToJson(calib, state.isCalibrated());
    });

    server.post("/api/calibration/start", [](const HttpRequest&, HttpResponse& res)
    {
        // start from a clean mapping, keys you don't map shouldn't keep stale entries
        AppState state;
        state.load(gKeyCount);
        state.resetCalibration(gKeyCount);
        state.save();

        CalibrationSession calib;
        calib.start(TotalPhysicalLeds);
        calib.save();
        nudgeDaemon();

        res.body = calibrationToJson(calib, state.isCalibrated());
    });

    server.post("/api/calibration/map", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);

        CalibrationSession calib;
        calib.load();

        if(!calib.active())
        {
            res.status = 400;
            res.body = "{\"error\":\"no calibration session active\"}";
            return;
        }

        AppState state;
        state.load(gKeyCount);
        state.setCalibrationMapping((int)body["ledIndex"].asNumber(-1), calib.step());
        state.save();

        if(calib.step() + 1 >= calib.total())
            calib.stop();
        else
            calib.setStep(calib.step() + 1);

        calib.save();
        nudgeDaemon();

        res.body = calibrationToJson(calib, state.isCalibrated());
    });

    server.post("/api/calibration/skip", [](const HttpRequest&, HttpResponse& res)
    {
        CalibrationSession calib;
        calib.load();

        if(calib.active())
        {
            if(calib.step() + 1 >= calib.total())
                calib.stop();
            else
                calib.setStep(calib.step() + 1);

            calib.save();
            nudgeDaemon();
        }

        AppState state;
        state.load(gKeyCount);
        res.body = calibrationToJson(calib, state.isCalibrated());
    });

    server.post("/api/calibration/back", [](const HttpRequest&, HttpResponse& res)
    {
        CalibrationSession calib;
        calib.load();

        if(calib.active())
        {
            calib.setStep(calib.step() - 1);
            calib.save();
            nudgeDaemon();
        }

        AppState state;
        state.load(gKeyCount);
        res.body = calibrationToJson(calib, state.isCalibrated());
    });

    server.post("/api/calibration/finish", [](const HttpRequest&, HttpResponse& res)
    {
        CalibrationSession calib;
        calib.load();
        calib.stop();
        calib.save();
        nudgeDaemon();

        AppState state;
        state.load(gKeyCount);
        res.body = calibrationToJson(calib, state.isCalibrated());
    });


    server.post("/api/daemon/start", [](const HttpRequest&, HttpResponse& res)
    {
        bool ok = startDaemon();
        res.body = std::string("{\"ok\":") + (ok ? "true" : "false") + "}";
    });

    server.post("/api/daemon/stop", [](const HttpRequest&, HttpResponse& res)
    {
        stopDaemon();
        res.body = "{\"ok\":true}";
    });


    server.get("/api/remap", [](const HttpRequest&, HttpResponse& res)
    {
        RemapConfig config;
        config.load();
        res.body = remapConfigToJson(config);
    });

    server.post("/api/remap/enabled", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);

        RemapConfig config;
        config.load();
        config.setEnabled(body["enabled"].asBool(false));
        config.save();

        // enabling also starts the engine (no-op if it's running / not installed)
        if(config.isEnabled())
            startRemapd();

        res.body = remapConfigToJson(config);
    });

    server.post("/api/remap/binding", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);
        int key = (int)body["key"].asNumber(0);

        RemapConfig config;
        config.load();

        BindingType type = bindingTypeFromString(body["type"].asString("passthrough"));

        if(type == BindingType::Passthrough)
        {
            config.clearBinding(key);
        }
        else
        {
            KeyBinding binding;
            binding.physicalKeyCode = key;
            binding.type = type;
            binding.remapKeyCode = (int)body["remap"].asNumber(0);
            binding.macro = macroFromJson(body["macro"]);
            config.setBinding(binding);
        }

        config.save();
        res.body = remapConfigToJson(config);
    });

    server.post("/api/remap/binding/delete", [](const HttpRequest& req, HttpResponse& res)
    {
        json::Value body = json::Value::parse(req.body);

        RemapConfig config;
        config.load();
        config.clearBinding((int)body["key"].asNumber(0));
        config.save();

        res.body = remapConfigToJson(config);
    });

    server.get("/api/remapd/status", [](const HttpRequest&, HttpResponse& res)
    {
        res.body = std::string("{\"running\":") + (isRemapdRunning() ? "true" : "false")
                 + ",\"installed\":" + (isRemapdInstalled() ? "true" : "false") + "}";
    });

    server.post("/api/remapd/start", [](const HttpRequest&, HttpResponse& res)
    {
        bool ok = startRemapd();
        res.body = std::string("{\"ok\":") + (ok ? "true" : "false") + "}";
    });

    server.post("/api/remapd/stop", [](const HttpRequest&, HttpResponse& res)
    {
        stopRemapd();
        res.body = "{\"ok\":true}";
    });


    printAccessUrls(port);

    if(!server.listen("0.0.0.0", port))
        return 1;

    return 0;
}
