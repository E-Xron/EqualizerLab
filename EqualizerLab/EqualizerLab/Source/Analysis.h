#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>
#include "FilterDesign.h"

// Lock-free tap the audio thread writes (mono sum) and the UI reads the newest samples from.
class ScopeTap
{
public:
    static constexpr int kSize = 1 << 15;
    void push (const float* const* ch, int numCh, int n)
    {
        if (numCh <= 0) return;
        int w = wpos.load (std::memory_order_relaxed);
        const float k = 1.f / (float) numCh;
        for (int i = 0; i < n; ++i)
        {
            float v = 0.f;
            for (int c = 0; c < numCh; ++c) v += ch[c][i];
            buf[(size_t) w] = v * k;
            w = (w + 1) & (kSize - 1);
        }
        wpos.store (w, std::memory_order_release);
        written.fetch_add (n, std::memory_order_release);
    }
    void read (float* dst, int n) const
    {
        const int end = wpos.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i) dst[i] = buf[(size_t) ((end - n + i) & (kSize - 1))];
    }
    juce::int64 count() const { return written.load (std::memory_order_acquire); }
private:
    std::array<float, kSize> buf {};
    std::atomic<int> wpos { 0 };
    std::atomic<juce::int64> written { 0 };
};

// FFT analyser run on the UI thread: Blackman-Harris window, power spectrum, attack/release smoothing in dB,
// a slope tilt (dB/octave around 1 kHz) and an optional peak-hold (Freeze).
class SpectrumAnalyser
{
public:
    void setOrder (int o)
    {
        if (o == order && fft != nullptr) return;
        order = o; N = 1 << o;
        fft = std::make_unique<juce::dsp::FFT> (o);
        window.resize ((size_t) N);
        double sum = 0;
        for (int i = 0; i < N; ++i)
        {
            const double x = 2.0 * eq::kPi * i / (N - 1);
            window[(size_t) i] = (float) (0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2 * x) - 0.01168 * std::cos (3 * x));
            sum += window[(size_t) i];
        }
        norm = (float) (2.0 / sum);
        data.assign ((size_t) N * 2, 0.f);
        db.assign ((size_t) N / 2 + 1, -150.f);
        peak.assign ((size_t) N / 2 + 1, -150.f);
        raw.assign ((size_t) N / 2 + 1, 0.f);
    }
    int size() const { return N; }

    // newData false: transport stopped / no audio, the curve falls away
    void update (const ScopeTap& tap, bool newData, double fs, float releaseDbPerFrame, float tiltDbOct, bool freeze)
    {
        if (fft == nullptr) setOrder (12);
        if (newData)
        {
            tap.read (data.data(), N);
            for (int i = 0; i < N; ++i) data[(size_t) i] *= window[(size_t) i];
            std::fill (data.begin() + N, data.end(), 0.f);
            fft->performFrequencyOnlyForwardTransform (data.data(), true);
        }
        for (int k = 0; k <= N / 2; ++k)
        {
            const double f = std::max (1.0, k * fs / N);
            float v = newData ? juce::Decibels::gainToDecibels (data[(size_t) k] * norm, -150.f) : -150.f;
            if (newData) raw[(size_t) k] = data[(size_t) k] * norm;
            v += (float) (tiltDbOct * std::log2 (f / 1000.0));
            auto& d = db[(size_t) k];
            d = v > d ? d + (v - d) * 0.6f : std::max (v, d - releaseDbPerFrame);
            if (freeze) peak[(size_t) k] = std::max (peak[(size_t) k], d);
        }
        if (! freeze) std::fill (peak.begin(), peak.end(), -150.f);
    }

    // value at frequency f (log-interpolated between bins, max over the bins a pixel spans)
    float at (double f0, double f1, double fs, bool usePeak = false) const
    {
        const auto& v = usePeak ? peak : db;
        if (v.empty()) return -150.f;
        const double binHz = fs / N;
        int k0 = (int) std::floor (f0 / binHz), k1 = (int) std::ceil (f1 / binHz);
        k0 = juce::jlimit (0, N / 2, k0); k1 = juce::jlimit (0, N / 2, k1);
        if (k1 - k0 <= 1)
        {
            const double fk = juce::jlimit (0.0, (double) N / 2 - 1.0, f0 / binHz);
            const int i = (int) fk; const float t = (float) (fk - i);
            return v[(size_t) i] + (v[(size_t) i + 1] - v[(size_t) i]) * t;
        }
        float m = -150.f;
        for (int k = k0; k <= k1; ++k) m = std::max (m, v[(size_t) k]);
        return m;
    }
    const std::vector<float>& rawMagnitudes() const { return raw; }   // linear, untilted, newest frame

private:
    int order = 0, N = 0;
    float norm = 1.f;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, data, db, peak, raw;
};

// EQ Match: long-term average spectra on a log grid, and a greedy fit of bell/shelf bands to their difference.
namespace match
{
    constexpr int kGrid = 240;
    inline double gridFreq (int i) { return 20.0 * std::pow (1000.0, (double) i / (kGrid - 1)); }   // 20 Hz .. 20 kHz

    struct Average
    {
        std::array<double, kGrid> sum {};
        int frames = 0;
        void clear() { sum.fill (0.0); frames = 0; }
        void add (const std::vector<float>& mags, double fs)          // mags: linear, N/2+1 bins
        {
            if (mags.size() < 2) return;
            const int N = (int) (mags.size() - 1) * 2;
            double total = 0;
            std::array<double, kGrid> frame {};
            for (int i = 0; i < kGrid; ++i)                         // 1/6 octave power average around each grid point
            {
                const double f = gridFreq (i), lo = f / std::pow (2.0, 1.0 / 12.0), hi = f * std::pow (2.0, 1.0 / 12.0);
                const int k0 = std::max (1, (int) std::floor (lo * N / fs)), k1 = std::min ((int) mags.size() - 1, std::max (k0, (int) std::ceil (hi * N / fs)));
                double p = 0; for (int k = k0; k <= k1; ++k) p += (double) mags[(size_t) k] * mags[(size_t) k];
                frame[(size_t) i] = p / (k1 - k0 + 1);
                total += frame[(size_t) i];
            }
            if (total < 1e-12) return;                               // skip silence
            for (int i = 0; i < kGrid; ++i) sum[(size_t) i] += frame[(size_t) i];
            ++frames;
        }
        bool valid() const { return frames > 4; }
        std::array<double, kGrid> db() const                        // normalised to 0 dB mean
        {
            std::array<double, kGrid> d {};
            double mean = 0;
            for (int i = 0; i < kGrid; ++i) { d[(size_t) i] = 10.0 * std::log10 (std::max (1e-20, sum[(size_t) i] / std::max (1, frames))); mean += d[(size_t) i]; }
            mean /= kGrid;
            for (auto& v : d) v -= mean;
            return d;
        }
    };

    inline std::vector<eq::BandParams> fit (const Average& reference, const Average& current, int maxBands, double amount)
    {
        const auto r = reference.db(), c = current.db();
        std::array<double, kGrid> target {};
        for (int i = 0; i < kGrid; ++i) target[(size_t) i] = juce::jlimit (-18.0, 18.0, (r[(size_t) i] - c[(size_t) i]) * amount);
        // light smoothing (about 1/3 octave)
        std::array<double, kGrid> sm {};
        for (int i = 0; i < kGrid; ++i)
        {
            double s = 0; int n = 0;
            for (int j = std::max (0, i - 4); j <= std::min (kGrid - 1, i + 4); ++j) { s += target[(size_t) j]; ++n; }
            sm[(size_t) i] = s / n;
        }

        std::vector<eq::BandParams> out;
        std::array<double, kGrid> res = sm;
        auto bandDb = [] (const eq::BandParams& b, int i) { return eq::gainToDb (std::abs ([&] { auto a = eq::design (b); eq::Cplx h (1.0); for (int s = 0; s < a.n; ++s) h *= a.s[(size_t) s].at (gridFreq (i)); return h; }())); };
        auto subtract = [&] (const eq::BandParams& b) { for (int i = 0; i < kGrid; ++i) res[(size_t) i] -= bandDb (b, i); };

        // broad tilt at the ends first: low and high shelves
        for (int side = 0; side < 2 && (int) out.size() < maxBands; ++side)
        {
            double m = 0; int n = 0;
            for (int i = side == 0 ? 0 : kGrid - 25; i < (side == 0 ? 25 : kGrid); ++i) { m += res[(size_t) i]; ++n; }
            m /= n;
            if (std::abs (m) < 1.0) continue;
            eq::BandParams b; b.used = true; b.type = side == 0 ? eq::Type::LowShelf : eq::Type::HighShelf;
            b.freq = side == 0 ? 120.0 : 7000.0; b.gain = juce::jlimit (-18.0, 18.0, m); b.q = 0.6;
            out.push_back (b); subtract (b);
        }
        while ((int) out.size() < maxBands)
        {
            int best = -1; double bv = 0;
            for (int i = 3; i < kGrid - 3; ++i) if (std::abs (res[(size_t) i]) > std::abs (bv)) { bv = res[(size_t) i]; best = i; }
            if (best < 0 || std::abs (bv) < 0.75) break;
            int lo = best, hi = best;
            while (lo > 0 && res[(size_t) lo] * bv > 0 && std::abs (res[(size_t) lo]) > std::abs (bv) * 0.5) --lo;
            while (hi < kGrid - 1 && res[(size_t) hi] * bv > 0 && std::abs (res[(size_t) hi]) > std::abs (bv) * 0.5) ++hi;
            const double bw = std::max (0.15, std::log2 (gridFreq (hi) / gridFreq (lo)));
            const double p = std::pow (2.0, bw);
            eq::BandParams b; b.used = true; b.type = eq::Type::Bell;
            b.freq = gridFreq (best); b.gain = juce::jlimit (-18.0, 18.0, bv); b.q = juce::jlimit (0.3, 8.0, std::sqrt (p) / (p - 1.0));
            out.push_back (b); subtract (b);
        }
        // refine gains a few times against the remaining error
        for (int it = 0; it < 8; ++it)
            for (auto& b : out)
            {
                const double f = b.type == eq::Type::LowShelf ? 40.0 : b.type == eq::Type::HighShelf ? 14000.0 : b.freq;
                const int i = juce::jlimit (0, kGrid - 1, (int) std::lround (std::log (f / 20.0) / std::log (1000.0) * (kGrid - 1)));
                const double before = bandDb (b, i);
                eq::BandParams nb = b; nb.gain = juce::jlimit (-18.0, 18.0, b.gain + res[(size_t) i] * 0.5);
                for (int k = 0; k < kGrid; ++k) res[(size_t) k] += bandDb (b, k) - bandDb (nb, k);
                juce::ignoreUnused (before);
                b = nb;
            }
        return out;
    }
}
