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
    const Color& activeColor
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

        for(size_t i = 0; i < keys.size() && i < baseColors.size(); i++)
            frame[i] = scaleColor(baseColors[i], k);

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



void applyBrightness(std::vector<Color>& frame, double brightness)
{
    brightness = std::clamp(brightness, 0.0, 1.0);

    if(brightness >= 0.999)
        return;

    for(Color& c : frame)
        c = scaleColor(c, brightness);
}

}
