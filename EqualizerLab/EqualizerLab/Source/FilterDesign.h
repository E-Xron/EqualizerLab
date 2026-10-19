#pragma once
#include <array>
#include <cmath>
#include <complex>

// Filter maths shared by the audio engine and the display.
// Every band is described as a cascade of analog sections (normalised to the band frequency), which are then
// turned into digital biquads: bilinear (Zero Latency), magnitude-matched (Natural Phase), or used directly as a
// magnitude target for the FIR (Linear Phase).
namespace eq
{
    constexpr int    kMaxBands = 24;
    constexpr int    kMaxSections = 8;
    constexpr double kPi = 3.14159265358979323846;

    enum class Type { Bell = 0, LowShelf, LowCut, HighShelf, HighCut, Notch, BandPass, TiltShelf };
    enum class Place { Stereo = 0, Left, Right, Mid, Side };
    enum class Mode { ZeroLatency = 0, Natural, Linear };

    inline const char* const* typeNames()  { static const char* n[] { "Bell", "Low Shelf", "Low Cut", "High Shelf", "High Cut", "Notch", "Band Pass", "Tilt Shelf" }; return n; }
    constexpr int kNumTypes = 8;
    inline const char* const* placeNames() { static const char* n[] { "Stereo", "Left", "Right", "Mid", "Side" }; return n; }
    constexpr int kNumPlaces = 5;
    inline const int* slopeDb()            { static const int s[] { 6, 12, 18, 24, 30, 36, 48, 72, 96 }; return s; }
    constexpr int kNumSlopes = 9;
    inline const char* const* modeNames()  { static const char* n[] { "Zero Latency", "Natural Phase", "Linear Phase" }; return n; }

    inline bool typeHasGain (Type t)  { return t == Type::Bell || t == Type::LowShelf || t == Type::HighShelf || t == Type::TiltShelf; }
    inline bool typeHasSlope (Type t) { return t == Type::LowCut || t == Type::HighCut || t == Type::BandPass; }

    struct BandParams
    {
        bool   used = false, on = true;
        Type   type = Type::Bell;
        double freq = 1000.0, gain = 0.0, q = 1.0;
        int    slope = 1;                 // index into slopeDb()
        Place  place = Place::Stereo;
        bool active() const { return used && on; }
    };

    using Cplx = std::complex<double>;

    // H(s) = (b2 s^2 + b1 s + b0) / (a2 s^2 + a1 s + a0), s normalised to 2*pi*f0. a2 == b2 == 0 is first order.
    struct Analog
    {
        double f0 = 1000.0, b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
        bool firstOrder() const { return a2 == 0.0 && b2 == 0.0; }
        Cplx at (double f) const
        {
            const Cplx s (0.0, f / f0);
            return (b2 * s * s + b1 * s + b0) / (a2 * s * s + a1 * s + a0);
        }
        double magAt (double f) const { return std::abs (at (f)); }
    };

    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        Cplx at (double w) const
        {
            const Cplx z1 = std::polar (1.0, -w), z2 = z1 * z1;
            return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
        }
    };

    struct AnalogChain { std::array<Analog, kMaxSections> s; int n = 0; void add (const Analog& a) { if (n < kMaxSections) s[(size_t) n++] = a; } };
    struct BiquadChain { std::array<Biquad, kMaxSections> s; int n = 0; };

    // Gain-Q interaction: bells narrow as their gain grows (like a classic analog console EQ).
    inline double effectiveQ (const BandParams& b, double gainDb, bool gainQ)
    {
        double q = b.q;
        if (gainQ && b.type == Type::Bell) q *= std::pow (10.0, std::abs (gainDb) / 80.0);
        return q;
    }

    inline AnalogChain design (const BandParams& b, double gainScale = 1.0, bool gainQ = false)
    {
        AnalogChain c;
        if (! b.active()) return c;
        const double g  = b.gain * gainScale;
        const double A  = std::pow (10.0, g / 40.0), sA = std::sqrt (A);
        const double q  = std::max (0.01, effectiveQ (b, g, gainQ));
        const double f0 = b.freq;
        auto sec = [&] (double b0, double b1, double b2, double a0, double a1, double a2)
        {
            Analog a; a.f0 = f0; a.b0 = b0; a.b1 = b1; a.b2 = b2; a.a0 = a0; a.a1 = a1; a.a2 = a2; c.add (a);
        };

        switch (b.type)
        {
            case Type::Bell:      if (g != 0.0) sec (1, A / q, 1, 1, 1 / (A * q), 1); break;
            case Type::LowShelf:  if (g != 0.0) sec (A * A, A * sA / q, A, 1, sA / q, A); break;
            case Type::HighShelf: if (g != 0.0) sec (A, A * sA / q, A * A, A, sA / q, 1); break;
            case Type::TiltShelf: if (g != 0.0) sec (1, A, 0, A, 1, 0); break;
            case Type::Notch:     sec (1, 0, 1, 1, 1 / q, 1); break;
            case Type::BandPass:
            {
                const int n = std::max (1, std::min (kMaxSections, slopeDb()[b.slope] / 12));
                for (int i = 0; i < n; ++i) sec (0, 1 / q, 0, 1, 1 / q, 1);
                break;
            }
            case Type::LowCut:
            case Type::HighCut:
            {
                const bool hp = b.type == Type::LowCut;
                const int order = slopeDb()[b.slope] / 6;
                if (order % 2 == 1)
                {
                    if (hp) sec (0, 1, 0, 1, 1, 0); else sec (1, 0, 0, 1, 1, 0);
                }
                const int pairs = order / 2;
                const double res = q / std::sqrt (0.5);       // Q 0.71 = Butterworth; higher adds a resonant bump
                for (int k = 0; k < pairs; ++k)
                {
                    double qk = (order % 2 == 0) ? 1.0 / (2.0 * std::cos ((2 * k + 1) * kPi / (2.0 * order)))
                                                 : 1.0 / (2.0 * std::cos ((k + 1) * kPi / order));
                    if (k == 0) qk *= res;                    // the sharpest pole pair carries the resonance
                    if (hp) sec (0, 0, 1, 1, 1 / qk, 1); else sec (1, 0, 0, 1, 1 / qk, 1);
                }
                break;
            }
        }
        return c;
    }

    // ---- digital versions
    inline Biquad bilinear (const Analog& s, double fs)
    {
        const double f0 = std::min (s.f0, 0.4995 * fs);
        const double K = std::tan (kPi * f0 / fs), K2 = K * K;
        Biquad d;
        if (s.firstOrder())
        {
            const double n0 = s.b1 + s.b0 * K, n1 = s.b0 * K - s.b1, m0 = s.a1 + s.a0 * K, m1 = s.a0 * K - s.a1;
            d.b0 = n0 / m0; d.b1 = n1 / m0; d.a1 = m1 / m0;
            return d;
        }
        const double n0 = s.b2 + s.b1 * K + s.b0 * K2, n1 = 2.0 * (s.b0 * K2 - s.b2), n2 = s.b2 - s.b1 * K + s.b0 * K2;
        const double m0 = s.a2 + s.a1 * K + s.a0 * K2, m1 = 2.0 * (s.a0 * K2 - s.a2), m2 = s.a2 - s.a1 * K + s.a0 * K2;
        d.b0 = n0 / m0; d.b1 = n1 / m0; d.b2 = n2 / m0; d.a1 = m1 / m0; d.a2 = m2 / m0;
        return d;
    }

    // Natural Phase: poles mapped exactly (impulse invariance), zeros chosen so the magnitude matches the analog
    // filter at DC, at Nyquist and at the band frequency. No cramping near Nyquist, near-analog phase, no latency.
    inline Biquad matched (const Analog& s, double fs)
    {
        const double w0T = 2.0 * kPi * s.f0 / fs;
        if (w0T < 0.02 * kPi) return bilinear (s, fs);                 // bilinear is already exact down here
        Biquad d;
        if (s.firstOrder())
        {
            const double p = -s.a0 / s.a1 * w0T;
            d.a1 = -std::exp (p);
            const double h0 = s.magAt (0.0), hn = s.magAt (0.5 * fs);
            const double sum = h0 * (1.0 + d.a1), diff = hn * (1.0 - d.a1);
            d.b0 = 0.5 * (sum + diff); d.b1 = 0.5 * (sum - diff);
            return d;
        }
        const double disc = s.a1 * s.a1 - 4.0 * s.a2 * s.a0;
        if (disc < 0.0)
        {
            const double re = -s.a1 / (2.0 * s.a2) * w0T, im = std::sqrt (-disc) / (2.0 * s.a2) * w0T;
            if (im > 0.95 * kPi) return bilinear (s, fs);
            d.a1 = -2.0 * std::exp (re) * std::cos (im);
            d.a2 = std::exp (2.0 * re);
        }
        else
        {
            const double r1 = (-s.a1 + std::sqrt (disc)) / (2.0 * s.a2) * w0T, r2 = (-s.a1 - std::sqrt (disc)) / (2.0 * s.a2) * w0T;
            d.a1 = -(std::exp (r1) + std::exp (r2));
            d.a2 = std::exp (r1 + r2);
        }
        auto phi = [] (double w, double& p0, double& p1, double& p2)
        {
            p1 = std::sin (0.5 * w); p1 *= p1; p0 = 1.0 - p1; p2 = 4.0 * p0 * p1;
        };
        const double A0 = (1.0 + d.a1 + d.a2) * (1.0 + d.a1 + d.a2), A1 = (1.0 - d.a1 + d.a2) * (1.0 - d.a1 + d.a2), A2 = -4.0 * d.a2;
        const double wm = std::min (w0T, 0.75 * kPi);
        double p0, p1, p2; phi (wm, p0, p1, p2);
        const double h0 = s.magAt (0.0), hn = s.magAt (0.5 * fs), hm = s.magAt (wm * fs / (2.0 * kPi));
        const double B0 = h0 * h0 * A0, B1 = hn * hn * A1;
        const double B2 = (hm * hm * (A0 * p0 + A1 * p1 + A2 * p2) - B0 * p0 - B1 * p1) / p2;
        const double s0 = std::sqrt (B0), s1 = std::sqrt (B1);
        const double W = 0.5 * (s0 + s1);
        d.b1 = 0.5 * (s0 - s1);
        d.b0 = 0.5 * (W + std::sqrt (std::max (0.0, W * W + B2)));
        d.b2 = W - d.b0;
        return d;
    }

    inline BiquadChain digital (const AnalogChain& a, double fs, Mode m)
    {
        BiquadChain c;
        c.n = a.n;
        for (int i = 0; i < a.n; ++i)
            c.s[(size_t) i] = (m == Mode::Natural) ? matched (a.s[(size_t) i], fs) : bilinear (a.s[(size_t) i], fs);
        return c;
    }

    // magnitude of one band in the given mode (Linear Phase uses the analog magnitude, which the FIR reproduces)
    inline double bandMag (const AnalogChain& a, const BiquadChain& d, double f, double fs, Mode m)
    {
        double mag = 1.0;
        if (m == Mode::Linear) { for (int i = 0; i < a.n; ++i) mag *= a.s[(size_t) i].magAt (f); }
        else { const double w = 2.0 * kPi * std::min (f, 0.5 * fs) / fs; for (int i = 0; i < d.n; ++i) mag *= std::abs (d.s[(size_t) i].at (w)); }
        return mag;
    }

    inline double gainToDb (double g) { return 20.0 * std::log10 (std::max (g, 1.0e-9)); }
}
