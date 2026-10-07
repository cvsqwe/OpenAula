#include "LightingEngine.h"

#include <cmath>
#include <algorithm>


namespace
{

Color hsvColor(float h)
{
    float sector = h * 6.0f;
    int i = (int)sector;
    float f = sector - i;

    float p = 0.0f;
    float q = 1.0f - f;
    float t = f;

    float rf, gf, bf;

    switch(i % 6)
    {
        case 0: rf = 1; gf = t; bf = p; break;
        case 1: rf = q; gf = 1; bf = p; break;
        case 2: rf = p; gf = 1; bf = t; break;
        case 3: rf = p; gf = q; bf = 1; break;
        case 4: rf = t; gf = p; bf = 1; break;
        default: rf = 1; gf = p; bf = q; break;
    }

    return Color{
        (unsigned char)(rf * 255),
        (unsigned char)(gf * 255),
        (unsigned char)(bf * 255)
    };
}

// Smooth 0..1..0 ping-pong wave with the given period (seconds).
double triangleWave(double t, double period)
{
    double half = period / 2.0;
    double phase = std::fmod(t, period);
    if(phase < 0) phase += period;

    double tri = half - std::fabs(phase - half);
    return tri / half;
}

Color scaleColor(const Color& c, double k)
{
    k = std::clamp(k, 0.0, 1.0);

    return Color{
        (unsigned char)(c.r * k),
        (unsigned char)(c.g * k),
        (unsigned char)(c.b * k)
    };
}

Color fromHsv(double h, double s, double v)
{
    h = std::fmod(h, 1.0);
    if(h < 0) h += 1.0;
    s = std::clamp(s, 0.0, 1.0);
    v = std::clamp(v, 0.0, 1.0);

    double sector = h * 6.0;
    int i = (int)sector;
    double f = sector - i;
    double p = v * (1 - s), q = v * (1 - s * f), u = v * (1 - s * (1 - f));
    double r, g, b;

    switch(i % 6)
    {
        case 0: r = v; g = u; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = u; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = u; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    return Color{ (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255) };
}

void toHsv(const Color& c, double& h, double& s, double& v)
{
    double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
    double mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;

    v = mx;
    s = mx > 0 ? d / mx : 0;
    h = 0;

    if(d > 0)
    {
        if(mx == r)      h = std::fmod((g - b) / d, 6.0);
        else if(mx == g) h = (b - r) / d + 2.0;
        else             h = (r - g) / d + 4.0;

        h /= 6.0;
        if(h < 0) h += 1.0;
    }
}

// Same colour with its hue rotated by `shift` turns (0..1).
Color shiftHue(const Color& c, double shift)
{
    double h, s, v;
    toHsv(c, h, s, v);
    return fromHsv(h + shift, s, v);
}

Color mixColor(const Color& a, const Color& b, double k)
{
    k = std::clamp(k, 0.0, 1.0);
    return Color{
        (unsigned char)(a.r + (b.r - a.r) * k),
        (unsigned char)(a.g + (b.g - a.g) * k),
        (unsigned char)(a.b + (b.b - a.b) * k)
    };
}

void boardExtent(const std::vector<KeyDef>& keys, double& maxX, double& maxY)
{
    maxX = 0; maxY = 0;
    for(const auto& k : keys)
    {
        maxX = std::max(maxX, k.x + k.w);
        maxY = std::max(maxY, k.y + k.h);
    }
}

int findKey(const std::vector<KeyDef>& keys, const char* label)
{
    for(size_t i = 0; i < keys.size(); i++)
        if(keys[i].label == label)
            return (int)i;

    return -1;
}

// Cheap, deterministic pseudo-random 0..1 from an integer key and a
// "salt" (a second axis - a different salt gives an independent stream
// for the same key, e.g. per-mode or per-flicker-frame). Deterministic
// means the GUI's live preview and the daemon's independently computed
// frame always agree without sharing any RNG state.
float hash01(int x, int salt)
{
    unsigned int h = (unsigned int)(x * 374761393 + salt * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return (h & 0xFFFFFFu) / (float)0xFFFFFFu;
}

}



namespace LightingEngine
{

std::vector<Color> computeFrame(
    LightingMode mode,
    double t,
    const std::vector<KeyDef>& keys,
    const std::vector<Color>& baseColors,
    const Color& activeColor,
    const SystemSignals& sys
)
{
    std::vector<Color> frame(keys.size(), Color{0, 0, 0});

    if(mode == LightingMode::Off)
        return frame;

    if(mode == LightingMode::Custom)
    {
        for(size_t i = 0; i < keys.size() && i < baseColors.size(); i++)
            frame[i] = baseColors[i];

        return frame;
    }

    if(mode == LightingMode::Breathing)
    {
        double k = 0.1 + 0.9 * (std::sin(t * 2.0) + 1.0) / 2.0;

        for(size_t i = 0; i < keys.size(); i++)
            frame[i] = scaleColor(activeColor, k);

        return frame;
    }

    if(mode == LightingMode::ColorCycle)
    {
        for(size_t i = 0; i < keys.size(); i++)
        {
            float hue = std::fmod((float)(t * 0.15 + keys[i].x * 0.03), 1.0f);
            if(hue < 0) hue += 1.0f;

            frame[i] = hsvColor(hue);
        }

        return frame;
    }

    if(mode == LightingMode::Bounce)
    {
        double maxX = 0;
        for(const auto& k : keys)
            maxX = std::max(maxX, k.x + k.w);

        double period = 4.0;
        double pos = triangleWave(t, period) * maxX;
        double falloff = 2.2;

        for(size_t i = 0; i < keys.size(); i++)
        {
            double center = keys[i].x + keys[i].w / 2.0;
            double dist = std::fabs(center - pos);
            double b = std::max(0.0, 1.0 - dist / falloff);

            frame[i] = scaleColor(activeColor, b);
        }

        return frame;
    }

    if(mode == LightingMode::Wave)
    {
        const double waveLength = 6.0;

        for(size_t i = 0; i < keys.size(); i++)
        {
            double center = keys[i].x + keys[i].w / 2.0;
            double phase = center / waveLength - t * 0.35;
            double b = 0.15 + 0.85 * (std::sin(2.0 * M_PI * phase) + 1.0) / 2.0;

            frame[i] = scaleColor(activeColor, b);
        }

        return frame;
    }

    if(mode == LightingMode::Starlight)
    {
        for(size_t i = 0; i < keys.size(); i++)
        {
            float seed = hash01(keys[i].ledIndex, 17);
            double period = 1.5 + seed * 3.0;
            double phase = seed * 11.0;
            double twinklePos = std::fmod(t + phase, period) / period;

            double dist = std::fabs(twinklePos - 0.5) * 2.0;
            double b = std::pow(std::max(0.0, 1.0 - dist), 6.0) * 0.9 + 0.04;

            frame[i] = scaleColor(activeColor, b);
        }

        return frame;
    }

    if(mode == LightingMode::Raindrop)
    {
        double maxY = 0;
        for(const auto& k : keys)
            maxY = std::max(maxY, k.y + k.h);

        for(size_t i = 0; i < keys.size(); i++)
        {
            double colSeed = hash01((int)std::lround(keys[i].x * 3.0), 41);
            double period = 1.2 + colSeed * 2.2;
            double phase = colSeed * 13.0;
            double localT = std::fmod(t + phase, period);
            double dropY = (localT / period) * (maxY + 2.5) - 1.5;

            double ky = keys[i].y + keys[i].h / 2.0;
            double dist = dropY - ky;

            double b = (dist >= 0 && dist < 1.4) ? (1.0 - dist / 1.4) : 0.0;

            frame[i] = scaleColor(activeColor, b);
        }

        return frame;
    }

    if(mode == LightingMode::Comet)
    {
        double maxX = 0, maxY = 0;
        for(const auto& k : keys)
        {
            maxX = std::max(maxX, k.x + k.w);
            maxY = std::max(maxY, k.y + k.h);
        }

        double pathLen = maxX + maxY * 1.3;
        double period = 3.0;
        double pos = std::fmod(t, period) / period * (pathLen + 6.0) - 3.0;

        for(size_t i = 0; i < keys.size(); i++)
        {
            double coord = keys[i].x + keys[i].y * 1.3;
            double dist = pos - coord;

            double b = (dist >= 0 && dist < 5.0) ? std::pow(1.0 - dist / 5.0, 2.0) : 0.0;

            frame[i] = scaleColor(activeColor, b);
        }

        return frame;
    }

    if(mode == LightingMode::Fire)
    {
        double maxY = 0;
        for(const auto& k : keys)
            maxY = std::max(maxY, k.y + k.h);

        int flickerStep = (int)(t * 6.0);

        for(size_t i = 0; i < keys.size(); i++)
        {
            double heightFactor = 1.0 - (keys[i].y / std::max(1.0, maxY)) * 0.5;

            float n1 = hash01(keys[i].ledIndex, flickerStep);
            float n2 = hash01(keys[i].ledIndex, flickerStep + 1000);
            double flicker = 0.55 + 0.45 * (n1 * 0.7 + n2 * 0.3);

            double heat = std::clamp(heightFactor * flicker, 0.0, 1.0);

            frame[i] = Color{
                (unsigned char)(255 * heat),
                (unsigned char)(90 * heat * heat),
                (unsigned char)(12 * heat * heat)
            };
        }

        return frame;
    }

    if(mode == LightingMode::RainbowWave)
    {
        for(size_t i = 0; i < keys.size(); i++)
        {
            float hue = std::fmod((float)((keys[i].x * 0.025 + keys[i].y * 0.07) + t * 0.2), 1.0f);
            if(hue < 0) hue += 1.0f;

            frame[i] = hsvColor(hue);
        }

        return frame;
    }

    if(mode == LightingMode::Heartbeat)
    {
        auto pulse = [](double x, double center, double width)
        {
            double d = std::fabs(x - center);
            return d < width ? std::pow(std::cos((d / width) * M_PI / 2.0), 2.0) : 0.0;
        };

        double period = 1.6;
        double localT = std::fmod(t, period);

        double b = 0.06
            + 0.55 * pulse(localT, 0.10, 0.10)
            + 0.92 * pulse(localT, 0.34, 0.14);

        b = std::clamp(b, 0.0, 1.0);

        for(size_t i = 0; i < keys.size(); i++)
            frame[i] = scaleColor(activeColor, b);

        return frame;
    }

    if(mode == LightingMode::Ripple)
    {
        double maxX = 0, maxY = 0;
        for(const auto& k : keys)
        {
            maxX = std::max(maxX, k.x + k.w);
            maxY = std::max(maxY, k.y + k.h);
        }

        double cx = maxX / 2.0;
        double cy = maxY / 2.0;
        const double ringSpacing = 1.6;

        for(size_t i = 0; i < keys.size(); i++)
        {
            double kx = keys[i].x + keys[i].w / 2.0;
            double ky = keys[i].y + keys[i].h / 2.0;

            double dist = std::sqrt((kx - cx) * (kx - cx) + (ky - cy) * (ky - cy));
            double phase = dist / ringSpacing - t * 0.6;
            double b = 0.15 + 0.85 * (std::sin(2.0 * M_PI * phase) + 1.0) / 2.0;

            frame[i] = scaleColor(activeColor, b);
        }

        return frame;
    }

    if(mode == LightingMode::Strobe)
    {
        double period = 0.6;
        double phase = std::fmod(t, period) / period;
        double b = phase < 0.5 ? 1.0 : 0.05;

        for(size_t i = 0; i < keys.size(); i++)
            frame[i] = scaleColor(activeColor, b);

        return frame;
    }

    if(mode == LightingMode::Alternating)
    {
        double period = 1.0;
        double phase = std::fmod(t, period) / period;
        bool onA = phase < 0.5;

        for(size_t i = 0; i < keys.size(); i++)
        {
            bool groupA = ((int)std::floor(keys[i].x) + (int)std::floor(keys[i].y)) % 2 == 0;
            double b = (groupA == onA) ? 1.0 : 0.08;

            frame[i] = scaleColor(activeColor, b);
        }

        return frame;
    }

    if(mode == LightingMode::Confetti)
    {
        for(size_t i = 0; i < keys.size(); i++)
        {
            float seed = hash01(keys[i].ledIndex, 71);
            double period = 0.6 + seed * 1.8;
            double phase = std::fmod(t, period) / period;
            int cycle = (int)std::floor(t / period);

            double b = std::pow(std::max(0.0, 1.0 - phase * 4.0), 3.0);
            float hue = hash01(keys[i].ledIndex + cycle * 977, 133);

            frame[i] = scaleColor(hsvColor(hue), b);
        }

        return frame;
    }

    if(mode == LightingMode::Snake)
    {
        int n = (int)keys.size();
        double period = 3.0;
        double pos = std::fmod(t, period) / period * n;
        double tailLen = 8.0;

        for(int i = 0; i < n; i++)
        {
            double d = pos - i;
            if(d < 0) d += n;

            double b = (d < tailLen) ? (1.0 - d / tailLen) : 0.0;
            frame[i] = scaleColor(activeColor, b);
        }

        return frame;
    }

    if(mode == LightingMode::Spiral)
    {
        double maxX = 0, maxY = 0;
        for(const auto& k : keys)
        {
            maxX = std::max(maxX, k.x + k.w);
            maxY = std::max(maxY, k.y + k.h);
        }

        double cx = maxX / 2.0, cy = maxY / 2.0;

        for(size_t i = 0; i < keys.size(); i++)
        {
            double kx = keys[i].x + keys[i].w / 2.0, ky = keys[i].y + keys[i].h / 2.0;
            double angle = std::atan2(ky - cy, kx - cx) / (2.0 * M_PI);
            double dist = std::sqrt((kx - cx) * (kx - cx) + (ky - cy) * (ky - cy));

            float hue = std::fmod((float)(angle + dist * 0.12 - t * 0.25), 1.0f);
            if(hue < 0) hue += 1.0f;

            frame[i] = hsvColor(hue);
        }

        return frame;
    }

    if(mode == LightingMode::Fireworks)
    {
        double maxX = 0, maxY = 0;
        for(const auto& k : keys)
        {
            maxX = std::max(maxX, k.x + k.w);
            maxY = std::max(maxY, k.y + k.h);
        }

        double boardR = std::sqrt(maxX * maxX + maxY * maxY) / 2.0;
        double period = 1.4;

        int w = (int)std::floor(t / period);
        double localT = t - w * period;

        double ox = hash01(w, 201) * maxX;
        double oy = hash01(w, 202) * maxY;
        float hue = hash01(w, 203);

        double ringR = (localT / period) * (boardR * 1.3);
        double thickness = 1.4;
        double fade = std::max(0.0, 1.0 - localT / period);
        Color burstColor = hsvColor(hue);

        for(size_t i = 0; i < keys.size(); i++)
        {
            double kx = keys[i].x + keys[i].w / 2.0, ky = keys[i].y + keys[i].h / 2.0;
            double dist = std::sqrt((kx - ox) * (kx - ox) + (ky - oy) * (ky - oy));
            double ringDist = std::fabs(dist - ringR);

            double b = std::max(0.0, 1.0 - ringDist / thickness) * fade;
            frame[i] = scaleColor(burstColor, b);
        }

        return frame;
    }

    if(mode == LightingMode::Aurora)
    {
        for(size_t i = 0; i < keys.size(); i++)
        {
            double x = keys[i].x + keys[i].w / 2.0, y = keys[i].y + keys[i].h / 2.0;
            double v = std::sin(x * 0.45 + t * 0.7) + std::sin(y * 0.9 - t * 0.5) + std::sin((x + y) * 0.3 + t * 0.35);
            double n = (v + 3.0) / 6.0;

            frame[i] = scaleColor(shiftHue(activeColor, (n - 0.5) * 0.35), 0.3 + 0.7 * n);
        }

        return frame;
    }

    if(mode == LightingMode::Matrix)
    {
        double maxX, maxY;
        boardExtent(keys, maxX, maxY);

        for(size_t i = 0; i < keys.size(); i++)
        {
            double colSeed = hash01((int)std::lround((keys[i].x + keys[i].w / 2.0) * 2.0), 59);
            double period = 1.6 + colSeed * 1.6;
            double localT = std::fmod(t + colSeed * 17.0, period);
            double headY = (localT / period) * (maxY + 5.0) - 1.0;
            double d = headY - (keys[i].y + keys[i].h / 2.0);

            if(d < 0 || d > 4.0)
                continue;

            double b = std::pow(1.0 - d / 4.0, 1.6);
            Color c = scaleColor(activeColor, b);

            frame[i] = d < 0.7 ? mixColor(c, Color{255, 255, 255}, 0.45) : c;
        }

        return frame;
    }

    if(mode == LightingMode::Gradient)
    {
        double maxX, maxY;
        boardExtent(keys, maxX, maxY);
        double drift = std::sin(t * 0.3) * 0.06;

        for(size_t i = 0; i < keys.size(); i++)
        {
            double pos = (keys[i].x + keys[i].w / 2.0) / std::max(1.0, maxX);
            frame[i] = shiftHue(activeColor, pos * 0.33 + drift);
        }

        return frame;
    }

    if(mode == LightingMode::Afterglow)
    {
        for(size_t i = 0; i < keys.size(); i++)
        {
            double age = i < sys.keyAge.size() ? sys.keyAge[i] : 1e9;
            double b = age < 1.4 ? std::pow(1.0 - age / 1.4, 2.0) : 0.0;

            frame[i] = scaleColor(activeColor, b);
        }

        return frame;
    }

    if(mode == LightingMode::Splash)
    {
        const double life = 1.1, speedUnits = 10.0, width = 1.3;

        for(size_t i = 0; i < keys.size(); i++)
        {
            double kx = keys[i].x + keys[i].w / 2.0, ky = keys[i].y + keys[i].h / 2.0;
            double best = 0.0;

            for(size_t j = 0; j < keys.size() && j < sys.keyAge.size(); j++)
            {
                double age = sys.keyAge[j];
                if(age >= life)
                    continue;

                double ox = keys[j].x + keys[j].w / 2.0, oy = keys[j].y + keys[j].h / 2.0;
                double dist = std::sqrt((kx - ox) * (kx - ox) + (ky - oy) * (ky - oy));
                double ring = std::max(0.0, 1.0 - std::fabs(dist - age * speedUnits) / width);

                best = std::max(best, ring * (1.0 - age / life));
            }

            frame[i] = scaleColor(activeColor, best);
        }

        return frame;
    }

    if(mode == LightingMode::CpuLoad)
    {
        double maxX, maxY;
        boardExtent(keys, maxX, maxY);

        for(size_t i = 0; i < keys.size(); i++)
        {
            double pos = (keys[i].x + keys[i].w / 2.0) / std::max(1.0, maxX);
            if(pos > sys.cpu)
                continue;

            // green -> amber -> red along the bar
            frame[i] = fromHsv(0.33 * (1.0 - pos), 1.0, 1.0);
        }

        return frame;
    }

    if(mode == LightingMode::Memory)
    {
        double maxX, maxY;
        boardExtent(keys, maxX, maxY);

        for(size_t i = 0; i < keys.size(); i++)
        {
            double fromBottom = 1.0 - (keys[i].y + keys[i].h / 2.0) / std::max(1.0, maxY);
            frame[i] = fromBottom <= sys.memory ? activeColor : Color{0, 0, 0};
        }

        return frame;
    }

    if(mode == LightingMode::Thermal)
    {
        double breathe = 0.8 + 0.2 * std::sin(t * (1.5 + sys.temperature * 4.0));
        Color c = fromHsv(0.62 * (1.0 - sys.temperature), 1.0, breathe);

        for(size_t i = 0; i < keys.size(); i++)
            frame[i] = c;

        return frame;
    }

    if(mode == LightingMode::Network)
    {
        int step = (int)std::floor(t * 8.0);
        double within = t * 8.0 - step;
        double chance = 0.03 + sys.network * 0.55;

        for(size_t i = 0; i < keys.size(); i++)
        {
            double b = hash01(keys[i].ledIndex, step * 31 + 7) < chance ? (1.0 - within) : 0.0;
            frame[i] = scaleColor(activeColor, 0.06 + 0.94 * b);
        }

        return frame;
    }

    if(mode == LightingMode::Clock)
    {
        static const char* fKeys[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"};
        static const char* digits[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

        Color dim = scaleColor(activeColor, 0.08);
        Color second = shiftHue(activeColor, 0.5);

        for(const char* label : fKeys)
        {
            int k = findKey(keys, label);
            if(k >= 0) frame[k] = dim;
        }

        int hour12 = sys.hour % 12 == 0 ? 12 : sys.hour % 12;
        int hk = findKey(keys, fKeys[hour12 - 1]);
        if(hk >= 0) frame[hk] = activeColor;

        int tens = findKey(keys, digits[(sys.minute / 10) % 10]);
        int units = findKey(keys, digits[sys.minute % 10]);

        if(tens >= 0) frame[tens] = activeColor;
        if(units >= 0) frame[units] = (units == tens) ? Color{255, 255, 255} : second;

        int esc = findKey(keys, "Esc");
        if(esc >= 0) frame[esc] = scaleColor(activeColor, sys.second % 2 == 0 ? 0.9 : 0.15);

        return frame;
    }

    if(mode == LightingMode::Indicators)
    {
        int caps = findKey(keys, "Caps");
        if(caps >= 0 && sys.capsLock) frame[caps] = activeColor;

        return frame;
    }

    if(mode == LightingMode::Sweep)
    {
        double maxX = 0, maxY = 0;
        for(const auto& k : keys)
        {
            maxX = std::max(maxX, k.x + k.w);
            maxY = std::max(maxY, k.y + k.h);
        }

        double diagMax = maxX + maxY;
        double period = 2.0;
        double phase = std::fmod(t, period) / period;
        double pos = phase * (diagMax + 4.0) - 2.0;

        for(size_t i = 0; i < keys.size(); i++)
        {
            double coord = keys[i].x + keys[i].y;
            double b = (coord < pos) ? 1.0 : 0.05;

            frame[i] = scaleColor(activeColor, b);
        }

        return frame;
    }

    return frame;
}



std::vector<Color> composite(
    const std::vector<Layer>& layers,
    const std::vector<double>& phases,
    const std::vector<KeyDef>& keys,
    const std::vector<Color>& baseColors,
    const SystemSignals& sys
)
{
    std::vector<double> acc(keys.size() * 3, 0.0);

    for(size_t li = 0; li < layers.size(); li++)
    {
        const Layer& layer = layers[li];
        if(!layer.enabled || layer.effect == LightingMode::Off)
            continue;

        double t = li < phases.size() ? phases[li] : 0.0;
        double o = std::clamp(layer.opacity, 0.0, 1.0);

        std::vector<Color> src = computeFrame(layer.effect, t, keys, baseColors, layer.color, sys);

        for(size_t i = 0; i < keys.size(); i++)
        {
            if(!layer.mask.empty() && (i >= layer.mask.size() || !layer.mask[i]))
                continue;

            double s[3] = { (double)src[i].r, (double)src[i].g, (double)src[i].b };

            for(int c = 0; c < 3; c++)
            {
                double& d = acc[i * 3 + c];

                switch(layer.blend)
                {
                    case BlendMode::Normal:   d = d * (1 - o) + s[c] * o; break;
                    case BlendMode::Add:      d = std::min(255.0, d + s[c] * o); break;
                    case BlendMode::Lighten:  d = d + (std::max(d, s[c]) - d) * o; break;
                    case BlendMode::Multiply: d = d * (1 - o) + d * (s[c] / 255.0) * o; break;
                }
            }
        }
    }

    std::vector<Color> frame(keys.size());

    for(size_t i = 0; i < keys.size(); i++)
        frame[i] = Color{
            (unsigned char)std::clamp(acc[i * 3], 0.0, 255.0),
            (unsigned char)std::clamp(acc[i * 3 + 1], 0.0, 255.0),
            (unsigned char)std::clamp(acc[i * 3 + 2], 0.0, 255.0)
        };

    return frame;
}



bool isAnimated(LightingMode mode)
{
    return mode != LightingMode::Custom && mode != LightingMode::Off;
}

bool usesSystemMetrics(LightingMode mode)
{
    return mode == LightingMode::CpuLoad || mode == LightingMode::Memory
        || mode == LightingMode::Thermal || mode == LightingMode::Network
        || mode == LightingMode::Clock;
}

bool usesKeystrokes(LightingMode mode)
{
    return mode == LightingMode::Afterglow || mode == LightingMode::Splash
        || mode == LightingMode::Indicators;
}



void applyBrightness(std::vector<Color>& frame, double brightness)
{
    brightness = std::clamp(brightness, 0.0, 1.0);

    if(brightness >= 0.999)
        return;

    for(Color& c : frame)
        c = scaleColor(c, brightness);
}

}
