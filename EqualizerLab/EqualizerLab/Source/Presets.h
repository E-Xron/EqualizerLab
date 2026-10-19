#pragma once
#include <vector>
#include "FilterDesign.h"

// Factory presets: a name and the bands it sets (every other band is cleared).
namespace presets
{
    struct Preset { const char* category; const char* name; std::vector<eq::BandParams> bands; };

    inline eq::BandParams B (eq::Type t, double f, double g, double q, int slope = 1, eq::Place p = eq::Place::Stereo)
    {
        eq::BandParams b; b.used = true; b.on = true; b.type = t; b.freq = f; b.gain = g; b.q = q; b.slope = slope; b.place = p; return b;
    }

    inline const std::vector<Preset>& factory()
    {
        using T = eq::Type; using P = eq::Place;
        static const std::vector<Preset> list {
            { "General",   "Default (flat)", {} },
            { "General",   "Smiley Curve",       { B (T::LowShelf, 90, 4, 0.7), B (T::Bell, 700, -2, 0.8), B (T::HighShelf, 9000, 4, 0.7) } },
            { "General",   "Gentle Air",         { B (T::HighShelf, 12000, 3, 0.6) } },
            { "General",   "Telephone",          { B (T::LowCut, 400, 0, 0.71, 3), B (T::Bell, 1500, 6, 1.2), B (T::HighCut, 3400, 0, 0.71, 3) } },
            { "General",   "Lo-Fi Radio",        { B (T::LowCut, 250, 0, 1.4, 1), B (T::Bell, 1100, 4, 1.0), B (T::HighCut, 5000, 0, 1.2, 1) } },
            { "Vocals",    "Vocal Presence",     { B (T::LowCut, 90, 0, 0.71, 3), B (T::Bell, 300, -2.5, 1.2), B (T::Bell, 3200, 3, 0.9), B (T::HighShelf, 11000, 2.5, 0.7) } },
            { "Vocals",    "Vocal De-Mud",       { B (T::LowCut, 100, 0, 0.71, 3), B (T::Bell, 250, -4, 1.5), B (T::Bell, 500, -2, 2.0) } },
            { "Vocals",    "Bright Pop Vocal",   { B (T::LowCut, 120, 0, 0.71, 5), B (T::Bell, 400, -2, 1.0), B (T::Bell, 5000, 2.5, 0.8), B (T::HighShelf, 14000, 4, 0.6) } },
            { "Vocals",    "Rap Vocal",          { B (T::LowCut, 80, 0, 0.71, 3), B (T::Bell, 180, 2, 1.0), B (T::Bell, 650, -2.5, 1.5), B (T::Bell, 4500, 3.5, 1.0), B (T::HighShelf, 12000, 2, 0.7) } },
            { "Drums",     "Kick Punch",         { B (T::LowCut, 28, 0, 0.71, 5), B (T::Bell, 60, 4, 1.4), B (T::Bell, 350, -5, 1.6), B (T::Bell, 4000, 3, 1.4) } },
            { "Drums",     "Snare Crack",        { B (T::LowCut, 90, 0, 0.71, 3), B (T::Bell, 200, 3, 1.4), B (T::Bell, 900, -3, 2.0), B (T::Bell, 5500, 3, 1.0) } },
            { "Drums",     "Overheads Clean",    { B (T::LowCut, 180, 0, 0.71, 3), B (T::Bell, 450, -2, 1.0), B (T::HighShelf, 10000, 2, 0.7) } },
            { "Drums",     "808 Tighten",        { B (T::LowCut, 25, 0, 0.71, 6), B (T::Bell, 50, 2, 1.0), B (T::Bell, 250, -3, 1.2) } },
            { "Bass",      "Bass Definition",    { B (T::LowCut, 30, 0, 0.71, 3), B (T::Bell, 80, 2, 1.0), B (T::Bell, 250, -3, 1.4), B (T::Bell, 900, 2.5, 1.2) } },
            { "Guitars",   "Acoustic Sparkle",   { B (T::LowCut, 80, 0, 0.71, 3), B (T::Bell, 220, -3, 1.2), B (T::HighShelf, 8000, 3, 0.7) } },
            { "Guitars",   "Electric Mid Focus", { B (T::LowCut, 100, 0, 0.71, 3), B (T::Bell, 1200, 2.5, 0.8), B (T::HighCut, 10000, 0, 0.71, 3) } },
            { "Keys",      "Piano Warmth",       { B (T::LowShelf, 150, 2, 0.7), B (T::Bell, 2500, -1.5, 1.0), B (T::HighShelf, 9000, 1.5, 0.7) } },
            { "Mix Bus",   "Master Polish",      { B (T::LowCut, 22, 0, 0.71, 3), B (T::LowShelf, 70, 1, 0.7), B (T::Bell, 300, -1, 0.7), B (T::HighShelf, 12000, 1.5, 0.6) } },
            { "Mix Bus",   "Mono Lows (Side Cut)", { B (T::LowCut, 120, 0, 0.71, 3, P::Side) } },
            { "Mix Bus",   "Wider Top (Side Air)", { B (T::HighShelf, 8000, 3, 0.7, 1, P::Side), B (T::LowCut, 150, 0, 0.71, 3, P::Side) } },
            { "Mix Bus",   "Tilt Brighter",      { B (T::TiltShelf, 1000, 3, 1.0) } },
            { "Mix Bus",   "Tilt Darker",        { B (T::TiltShelf, 1000, -3, 1.0) } },
            { "Utility",   "Rumble Filter",      { B (T::LowCut, 35, 0, 0.71, 6) } },
            { "Utility",   "Hum Notch 60 Hz",    { B (T::Notch, 60, 0, 12), B (T::Notch, 120, 0, 12), B (T::Notch, 180, 0, 12) } },
            { "Utility",   "Hum Notch 50 Hz",    { B (T::Notch, 50, 0, 12), B (T::Notch, 100, 0, 12), B (T::Notch, 150, 0, 12) } },
        };
        return list;
    }
}
