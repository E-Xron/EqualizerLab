#include "EqEngine.h"
#include <cmath>

using namespace eq;

namespace
{
    constexpr int kSub = 32;                         // coefficient update interval while gliding
}

// ===================================================================== IIR
void IirEq::prepare (double newFs)
{
    fs = newFs;
    for (auto& b : bands) { b.live = false; b.dirty = true; b.clear(); }
    gainScale = gainScaleTarget;
    reset();
}

void IirEq::reset()
{
    for (auto& b : bands) b.clear();
    for (auto& c : soloSt) for (auto& s : c) s[0] = s[1] = 0.0;
}

void IirEq::setTargets (const BandSet& t, double gs, bool gq, Mode m)
{
    target = t;
    gainScaleTarget = gs;
    if (gq != gainQ) { gainQ = gq; for (auto& b : bands) b.dirty = true; }
    if (m != mode)
    {
        mode = m;
        for (auto& b : bands) { b.dirty = true; b.clear(); }
    }

    for (int i = 0; i < kMaxBands; ++i)
    {
        auto& b = bands[(size_t) i];
        const auto& T = target[(size_t) i];
        const bool gainType = typeHasGain (T.type);
        const bool structural = T.type != b.cur.type || T.slope != b.cur.slope || T.place != b.cur.place
                                || (! gainType && T.active() != b.live);

        if (structural || (! b.live && T.active()))
        {
            const bool wasLive = b.live;
            b.cur = T;
            b.cur.used = b.cur.on = true;
            if (gainType) b.cur.gain = (wasLive && ! structural) ? b.cur.gain : 0.0;   // fade in from flat
            b.live = T.active();
            b.dirty = true;
            if (structural) b.clear();
        }
    }
}

void IirEq::stepSmoothing (double a)
{
    if (std::abs (gainScale - gainScaleTarget) > 1e-6)
    {
        gainScale += (gainScaleTarget - gainScale) * a;
        if (std::abs (gainScale - gainScaleTarget) < 1e-4) gainScale = gainScaleTarget;
        for (auto& b : bands) b.dirty = true;
    }

    for (int i = 0; i < kMaxBands; ++i)
    {
        auto& b = bands[(size_t) i];
        if (! b.live) continue;
        const auto& T = target[(size_t) i];
        const bool gainType = typeHasGain (T.type);
        const double tg = T.active() ? T.gain : 0.0;

        auto glideLog = [a] (double& v, double t)
        {
            if (std::abs (v / t - 1.0) < 1e-5) { v = t; return false; }
            v = std::exp (std::log (v) + (std::log (t) - std::log (v)) * a);
            return true;
        };
        bool moved = glideLog (b.cur.freq, T.freq) | glideLog (b.cur.q, T.q);
        if (gainType && std::abs (b.cur.gain - tg) > 1e-5)
        {
            b.cur.gain += (tg - b.cur.gain) * a;
            if (std::abs (b.cur.gain - tg) < 1e-3) b.cur.gain = tg;
            moved = true;
        }
        if (moved) b.dirty = true;
        if (gainType && ! T.active() && b.cur.gain == 0.0) { b.live = false; b.clear(); }
    }
}

void IirEq::updateCoeffs (Band& b)
{
    b.coeffs = digital (design (b.cur, gainScale, gainQ), fs, mode);
    b.dirty = false;
}

void IirEq::run (const BiquadChain& c, double (*st)[2], float* x, int n)
{
    for (int k = 0; k < c.n; ++k)
    {
        const auto& q = c.s[(size_t) k];
        double s1 = st[k][0], s2 = st[k][1];
        for (int i = 0; i < n; ++i)
        {
            const double in = x[i];
            const double y = q.b0 * in + s1;
            s1 = q.b1 * in - q.a1 * y + s2;
            s2 = q.b2 * in - q.a2 * y;
            x[i] = (float) y;
        }
        st[k][0] = s1; st[k][1] = s2;
    }
}

void IirEq::process (float* const* ch, int numCh, int numSamples, bool runBands)
{
    const double a = 1.0 - std::exp (-(double) kSub / (fs * 0.025));
    const bool stereo = numCh >= 2;

    for (int pos = 0; pos < numSamples; pos += kSub)
    {
        const int n = std::min (kSub, numSamples - pos);
        stepSmoothing (a);
        float* x[2] = { ch[0] + pos, stereo ? ch[1] + pos : nullptr };

        bool anyMS = false;
        for (auto& b : bands)
        {
            if (! b.live) continue;
            if (! runBands) continue;
            if (b.dirty) updateCoeffs (b);
            const Place p = b.cur.place;
            if (! stereo)
            {
                if (p != Place::Side) run (b.coeffs, b.st[0], x[0], n);
                continue;
            }
            if (p == Place::Mid || p == Place::Side) { anyMS = true; continue; }
            if (p != Place::Right) run (b.coeffs, b.st[0], x[0], n);
            if (p != Place::Left)  run (b.coeffs, b.st[1], x[1], n);
        }

        if (anyMS)
        {
            for (int i = 0; i < n; ++i)
            {
                const float l = x[0][i], r = x[1][i];
                x[0][i] = 0.5f * (l + r); x[1][i] = 0.5f * (l - r);
            }
            for (auto& b : bands)
            {
                if (! b.live) continue;
                if (b.cur.place == Place::Mid)  run (b.coeffs, b.st[0], x[0], n);
                if (b.cur.place == Place::Side) run (b.coeffs, b.st[1], x[1], n);
            }
            for (int i = 0; i < n; ++i)
            {
                const float m = x[0][i], s = x[1][i];
                x[0][i] = m + s; x[1][i] = m - s;
            }
        }

        processSolo (x, stereo ? 2 : 1, n);
    }
}

void IirEq::processSolo (float* const* ch, int numCh, int n)
{
    const int s = soloBand;
    if (s < 0 || s >= kMaxBands || ! target[(size_t) s].used) { lastSolo = -1; return; }
    if (s != lastSolo) { for (auto& c : soloSt) for (auto& v : c) v[0] = v[1] = 0.0; lastSolo = s; }

    // audition: only the region the band acts on
    const auto& b = target[(size_t) s];
    Analog an; an.f0 = b.freq;
    const double q = std::max (0.1, b.q);
    switch (b.type)
    {
        case Type::LowShelf: case Type::HighCut:  an.b0 = 1; an.b1 = 0; an.b2 = 0; an.a0 = 1; an.a1 = 1.414; an.a2 = 1; break;
        case Type::HighShelf: case Type::LowCut:  an.b0 = 0; an.b1 = 0; an.b2 = 1; an.a0 = 1; an.a1 = 1.414; an.a2 = 1; break;
        case Type::Bell: case Type::Notch: case Type::BandPass: case Type::TiltShelf:
        default:                                  an.b0 = 0; an.b1 = 1 / q; an.b2 = 0; an.a0 = 1; an.a1 = 1 / q; an.a2 = 1; break;
    }
    soloCoeffs.n = 1;
    soloCoeffs.s[0] = bilinear (an, fs);
    for (int c = 0; c < numCh; ++c) run (soloCoeffs, soloSt[c], ch[c], n);
}

// ===================================================================== linear phase
LinearPhaseEq::LinearPhaseEq()
{
    using C = juce::dsp::Convolution;
    ll = std::make_unique<C> (C::NonUniform { 1024 }, queue);
    rr = std::make_unique<C> (C::NonUniform { 1024 }, queue);
    lr = std::make_unique<C> (C::NonUniform { 1024 }, queue);
    rl = std::make_unique<C> (C::NonUniform { 1024 }, queue);
}

void LinearPhaseEq::prepare (double fs, int maxBlock)
{
    const std::lock_guard<std::mutex> guard (lock);
    sampleRate = fs;
    juce::dsp::ProcessSpec spec { fs, (juce::uint32) maxBlock, 1 };
    for (auto* c : { ll.get(), rr.get(), lr.get(), rl.get() }) c->prepare (spec);
    scratch.setSize (4, maxBlock);
    for (auto* c : { lr.get(), rl.get() })                    // cross paths start silent (default would be a pass-through)
    {
        juce::AudioBuffer<float> zero (1, 1);
        zero.clear();
        c->loadImpulseResponse (std::move (zero), fs, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    }
    isReady = false;
}

void LinearPhaseEq::reset()
{
    for (auto* c : { ll.get(), rr.get(), lr.get(), rl.get() }) c->reset();
}

void LinearPhaseEq::design (const BandSet& bandsIn, double gainScale, bool gainQ, int order, bool stereo)
{
    const std::lock_guard<std::mutex> guard (lock);
    const double fs = sampleRate.load();
    const int N = 1 << order, half = N / 2;

    if (fft == nullptr || fft->getSize() != N)
    {
        fft = std::make_unique<juce::dsp::FFT> (order);
        std::vector<float> t ((size_t) N * 2, 0.f);              // calibrate the inverse transform's scaling
        for (int k = 0; k <= half; ++k) t[(size_t) (2 * k)] = 1.f;
        fft->performRealOnlyInverseTransform (t.data());
        fftScale = std::abs (t[0]) > 1e-9f ? 1.f / t[0] : 1.f;
    }

    // magnitude of each placement group at every bin
    std::vector<double> g[5];
    for (auto& v : g) v.assign ((size_t) half + 1, 1.0);
    bool hasMS = false;
    for (const auto& b : bandsIn)
    {
        if (! b.active()) continue;
        const auto an = eq::design (b, gainScale, gainQ);
        if (an.n == 0) continue;
        int grp = (int) b.place;
        if (! stereo) { if (b.place == Place::Side) continue; grp = 0; }
        if (grp >= 3) hasMS = true;
        auto& v = g[grp];
        for (int k = 1; k <= half; ++k)
        {
            const double f = (double) k * fs / N;
            double m = 1.0;
            for (int s = 0; s < an.n; ++s) m *= an.s[(size_t) s].magAt (f);
            v[(size_t) k] *= m;
        }
        double m0 = 1.0;
        for (int s = 0; s < an.n; ++s) m0 *= an.s[(size_t) s].magAt (0.0);
        v[0] *= m0;
    }

    auto makeIr = [&] (auto magFn)
    {
        std::vector<float> d ((size_t) N * 2, 0.f);
        for (int k = 0; k <= half; ++k) d[(size_t) (2 * k)] = (float) magFn (k);
        fft->performRealOnlyInverseTransform (d.data());
        juce::AudioBuffer<float> ir (1, N);
        auto* w = ir.getWritePointer (0);
        for (int n = 0; n < N; ++n)
        {
            const int src = (n + half) % N;                       // centre the zero-phase response
            const double x = (double) n / (double) N;
            const double win = 0.42 - 0.5 * std::cos (2.0 * kPi * x) + 0.08 * std::cos (4.0 * kPi * x);
            w[n] = (float) (d[(size_t) src] * fftScale * win);
        }
        return ir;
    };
    using C = juce::dsp::Convolution;
    auto load = [&] (C& c, juce::AudioBuffer<float>&& ir) { c.loadImpulseResponse (std::move (ir), fs, C::Stereo::no, C::Trim::no, C::Normalise::no); };

    const auto& S = g[0]; const auto& L = g[1]; const auto& R = g[2]; const auto& M = g[3]; const auto& Sd = g[4];
    if (! hasMS)
    {
        load (*ll, makeIr ([&] (int k) { return S[(size_t) k] * L[(size_t) k]; }));
        load (*rr, makeIr ([&] (int k) { return S[(size_t) k] * R[(size_t) k]; }));
        cross = false;
    }
    else
    {
        load (*ll, makeIr ([&] (int k) { return 0.5 * (M[(size_t) k] + Sd[(size_t) k]) * S[(size_t) k] * L[(size_t) k]; }));
        load (*rr, makeIr ([&] (int k) { return 0.5 * (M[(size_t) k] + Sd[(size_t) k]) * S[(size_t) k] * R[(size_t) k]; }));
        load (*lr, makeIr ([&] (int k) { return 0.5 * (M[(size_t) k] - Sd[(size_t) k]) * S[(size_t) k] * R[(size_t) k]; }));
        load (*rl, makeIr ([&] (int k) { return 0.5 * (M[(size_t) k] - Sd[(size_t) k]) * S[(size_t) k] * L[(size_t) k]; }));
        cross = true;
    }
    latencySamples = half;
    isReady = true;
}

void LinearPhaseEq::process (float* const* ch, int numCh, int n)
{
    using namespace juce::dsp;
    auto conv = [n] (Convolution& c, const float* in, float* out)
    {
        const float* ins[] = { in };
        float* outs[] = { out };
        AudioBlock<const float> bi (ins, 1, (size_t) n);
        AudioBlock<float> bo (outs, 1, (size_t) n);
        c.process (ProcessContextNonReplacing<float> (bi, bo));
    };

    if (numCh < 2)
    {
        auto* t = scratch.getWritePointer (0);
        juce::FloatVectorOperations::copy (t, ch[0], n);
        conv (*ll, t, ch[0]);
        return;
    }

    auto* inL = scratch.getWritePointer (0); auto* inR = scratch.getWritePointer (1);
    auto* x1 = scratch.getWritePointer (2);  auto* x2 = scratch.getWritePointer (3);
    juce::FloatVectorOperations::copy (inL, ch[0], n);
    juce::FloatVectorOperations::copy (inR, ch[1], n);
    conv (*ll, inL, ch[0]);
    conv (*rr, inR, ch[1]);
    if (cross.load())
    {
        conv (*lr, inR, x1);
        conv (*rl, inL, x2);
        juce::FloatVectorOperations::add (ch[0], x1, n);
        juce::FloatVectorOperations::add (ch[1], x2, n);
    }
}
