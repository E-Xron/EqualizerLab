#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <mutex>
#include "FilterDesign.h"

using BandSet = std::array<eq::BandParams, eq::kMaxBands>;

// Zero Latency / Natural Phase engine: cascaded biquads in double precision. Frequency, gain and Q glide to their
// targets (no zipper noise), bands fade in and out, and Mid/Side bands run on an M/S-encoded copy of the signal.
class IirEq
{
public:
    void prepare (double fs);
    void reset();
    void setTargets (const BandSet&, double gainScale, bool gainQ, eq::Mode);
    void process (float* const* ch, int numCh, int numSamples, bool runBands = true);   // runBands false: solo only
    void setSolo (int band) { soloBand = band; }

private:
    struct Band
    {
        eq::BandParams cur;
        bool live = false, dirty = true;
        eq::BiquadChain coeffs;
        double st[2][eq::kMaxSections][2] {};
        void clear() { for (auto& c : st) for (auto& s : c) s[0] = s[1] = 0.0; }
    };

    void stepSmoothing (double amount);
    void updateCoeffs (Band&);
    static void run (const eq::BiquadChain&, double (*st)[2], float* x, int n);
    void processSolo (float* const* ch, int numCh, int n);

    double fs = 48000.0;
    BandSet target;
    std::array<Band, eq::kMaxBands> bands;
    double gainScale = 1.0, gainScaleTarget = 1.0;
    bool gainQ = false;
    eq::Mode mode = eq::Mode::ZeroLatency;
    int soloBand = -1;

    // solo (audition) filter
    eq::BiquadChain soloCoeffs;
    double soloSt[2][eq::kMaxSections][2] {};
    int lastSolo = -1;
};

// Linear Phase engine: the whole EQ curve becomes one FIR per channel path (L->L, R->R, and L->R / R->L when Mid/Side
// bands are used), designed on a background thread and run through JUCE's zero-latency partitioned convolution.
// The FIR is symmetric, so phase is untouched and the plug-in reports half its length as latency.
class LinearPhaseEq
{
public:
    LinearPhaseEq();
    void prepare (double fs, int maxBlock);
    void reset();
    void design (const BandSet&, double gainScale, bool gainQ, int fftOrder, bool stereo);   // worker thread
    void process (float* const* ch, int numCh, int numSamples);                              // audio thread
    int  latency() const { return latencySamples.load(); }
    bool ready() const { return isReady.load(); }

private:
    std::mutex lock;                                  // prepare vs. background design
    juce::dsp::ConvolutionMessageQueue queue;
    std::unique_ptr<juce::dsp::Convolution> ll, rr, lr, rl;
    std::atomic<bool> cross { false }, isReady { false };
    std::atomic<int> latencySamples { 0 };
    std::atomic<double> sampleRate { 48000.0 };
    juce::AudioBuffer<float> scratch;
    std::unique_ptr<juce::dsp::FFT> fft;
    float fftScale = 1.f;
};
