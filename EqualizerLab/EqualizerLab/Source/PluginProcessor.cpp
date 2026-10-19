#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

using namespace eq;

namespace
{
    constexpr int kRing = 1 << 18;                          // dry delay for bypass in Linear Phase (up to 2^17 samples)

    juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        return { lo, hi,
                 [] (float s, float e, float v) { return s * std::pow (e / s, v); },
                 [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                 [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
    }

    juce::String freqText (float v, int)
    {
        return v >= 1000.f ? juce::String (v / 1000.f, v >= 10000.f ? 1 : 2) + " kHz" : juce::String (v, v < 100.f ? 1 : 0) + " Hz";
    }
    float freqValue (const juce::String& t)
    {
        const float v = t.retainCharacters ("0123456789.").getFloatValue();
        return t.containsIgnoreCase ("k") ? v * 1000.f : v;
    }
}

class EqProcessor::Worker : public juce::Thread
{
public:
    Worker (EqProcessor& p, LinearPhaseEq& l) : juce::Thread ("EqualizerLab FIR designer"), proc (p), lin (l) {}
    std::atomic<bool> force { true };

    void run() override
    {
        std::vector<double> last;
        int lastOrder = -1;
        while (! threadShouldExit())
        {
            const auto bands = proc.readBands();
            const double gs = proc.pScale->load();
            const bool gq = proc.pGainQ->load() > 0.5f;
            const int order = pid::lpOrder ((int) proc.pRes->load(), proc.getSr());
            const bool stereo = proc.isStereo();

            std::vector<double> key { gs, gq ? 1.0 : 0.0, stereo ? 1.0 : 0.0, proc.getSr() };
            for (const auto& b : bands)
                if (b.active())
                    key.insert (key.end(), { (double) b.type, b.freq, b.gain, b.q, (double) b.slope, (double) b.place });
                else key.push_back (-1.0);

            const bool changed = key != last;
            if (changed) proc.autoGainDb = computeAutoGain (bands, gs, gq);

            if (proc.mode() == Mode::Linear)
            {
                if (changed || order != lastOrder || force.exchange (false))
                {
                    lin.design (bands, gs, gq, order, stereo);
                    last = key; lastOrder = order;
                    wait (30);                                         // keep redesigns under ~30 per second
                    continue;
                }
            }
            else
            {
                last = key;
                lastOrder = -1;                                        // design again when Linear Phase is switched on
            }
            wait (20);
        }
    }

private:
    // loudness compensation: the average change across the spectrum, so bypassing compares at equal level
    static float computeAutoGain (const BandSet& bands, double gs, bool gq)
    {
        double sum = 0; int n = 0;
        for (int i = 0; i < 64; ++i)
        {
            const double f = 40.0 * std::pow (16000.0 / 40.0, i / 63.0);
            double db = 0;
            for (const auto& b : bands)
            {
                if (! b.active()) continue;
                const auto a = eq::design (b, gs, gq);
                double m = 1.0;
                for (int s = 0; s < a.n; ++s) m *= a.s[(size_t) s].magAt (f);
                db += gainToDb (m) * (b.place == Place::Stereo ? 1.0 : 0.5);
            }
            sum += juce::jlimit (-36.0, 36.0, db); ++n;
        }
        return (float) juce::jlimit (-24.0, 24.0, -sum / n);
    }

    EqProcessor& proc;
    LinearPhaseEq& lin;
};

EqProcessor::EqProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "STATE", createLayout())
{
    for (int i = 0; i < kMaxBands; ++i)
    {
        auto g = [&] (const char* w) { return apvts.getRawParameterValue (pid::band (i, w)); };
        bp[(size_t) i] = { g ("used"), g ("on"), g ("freq"), g ("gain"), g ("q"), g ("type"), g ("slope"), g ("place") };
    }
    pMode = apvts.getRawParameterValue ("mode");     pRes = apvts.getRawParameterValue ("lpRes");
    pScale = apvts.getRawParameterValue ("gainScale"); pOut = apvts.getRawParameterValue ("outGain");
    pPan = apvts.getRawParameterValue ("outPan");    pAuto = apvts.getRawParameterValue ("autoGain");
    pInv = apvts.getRawParameterValue ("phaseInv");  pGainQ = apvts.getRawParameterValue ("gainQ");
    pBypass = apvts.getRawParameterValue ("bypass");

    for (auto& r : dryRing) r.assign ((size_t) kRing, 0.f);
    worker = std::make_unique<Worker> (*this, lin);
    worker->startThread();
    startTimerHz (20);
}

EqProcessor::~EqProcessor()
{
    stopTimer();
    worker->stopThread (4000);
}

juce::AudioProcessorValueTreeState::ParameterLayout EqProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    juce::StringArray types, slopes, places;
    for (int i = 0; i < kNumTypes; ++i) types.add (typeNames()[i]);
    for (int i = 0; i < kNumSlopes; ++i) slopes.add (juce::String (slopeDb()[i]) + " dB/oct");
    for (int i = 0; i < kNumPlaces; ++i) places.add (placeNames()[i]);

    for (int i = 0; i < kMaxBands; ++i)
    {
        const juce::String n = "Band " + juce::String (i + 1) + " ";
        auto id = [&] (const char* w) { return juce::ParameterID { pid::band (i, w), 1 }; };
        p.push_back (std::make_unique<juce::AudioParameterBool> (id ("used"), n + "Used", false));
        p.push_back (std::make_unique<juce::AudioParameterBool> (id ("on"), n + "Enabled", true));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (id ("freq"), n + "Frequency", logRange (10.f, 30000.f), 1000.f,
                     juce::AudioParameterFloatAttributes().withStringFromValueFunction (freqText).withValueFromStringFunction (freqValue)));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (id ("gain"), n + "Gain", juce::NormalisableRange<float> (-30.f, 30.f), 0.f,
                     juce::AudioParameterFloatAttributes().withLabel ("dB")
                         .withStringFromValueFunction ([] (float v, int) { return (v > 0.f ? "+" : "") + juce::String (v, 1) + " dB"; })));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (id ("q"), n + "Q", logRange (0.025f, 40.f), 1.f,
                     juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, v < 10.f ? 2 : 1); })));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (id ("type"), n + "Shape", types, 0));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (id ("slope"), n + "Slope", slopes, 1));
        p.push_back (std::make_unique<juce::AudioParameterChoice> (id ("place"), n + "Stereo Placement", places, 0));
    }

    juce::StringArray modes;
    for (int i = 0; i < 3; ++i) modes.add (modeNames()[i]);
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Processing Mode", modes, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "lpRes", 1 }, "Linear Phase Resolution", pid::lpResNames(), 1));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gainScale", 1 }, "Gain Scale", juce::NormalisableRange<float> (-1.f, 2.f), 1.f,
                 juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String ((int) std::lround (v * 100.f)) + "%"; })
                     .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.f; })));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "outGain", 1 }, "Output Gain", juce::NormalisableRange<float> (-36.f, 36.f), 0.f,
                 juce::AudioParameterFloatAttributes().withLabel ("dB")
                     .withStringFromValueFunction ([] (float v, int) { return (v > 0.f ? "+" : "") + juce::String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "outPan", 1 }, "Output Pan", juce::NormalisableRange<float> (-1.f, 1.f), 0.f,
                 juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                 { const int pc = (int) std::lround (std::abs (v) * 100.f); return pc == 0 ? juce::String ("C") : juce::String (pc) + (v < 0 ? "L" : "R"); })));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "autoGain", 1 }, "Auto Gain", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "phaseInv", 1 }, "Phase Invert", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "gainQ", 1 }, "Gain-Q Interaction", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
    return { p.begin(), p.end() };
}

bool EqProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != out) return false;
    if (l.inputBuses.size() > 1)
    {
        const auto sc = l.inputBuses[1];
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void EqProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    iir.prepare (sampleRate);
    lin.prepare (sampleRate, maxBlock);
    for (auto& r : dryRing) std::fill (r.begin(), r.end(), 0.f);
    dryPos = 0; lastMode = -1;
    outGainSm.reset (sampleRate, 0.03); panSm.reset (sampleRate, 0.03); wetSm.reset (sampleRate, 0.02);
    outGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    panSm.setCurrentAndTargetValue (pPan->load());
    wetSm.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 0.f : 1.f);
    worker->force = true;
    worker->notify();
}

BandSet EqProcessor::readBands() const
{
    BandSet s;
    for (int i = 0; i < kMaxBands; ++i)
    {
        const auto& p = bp[(size_t) i];
        auto& b = s[(size_t) i];
        b.used = p.used->load() > 0.5f;
        b.on = p.on->load() > 0.5f;
        b.freq = p.freq->load();
        b.gain = p.gain->load();
        b.q = p.q->load();
        b.type = (Type) juce::jlimit (0, kNumTypes - 1, (int) p.type->load());
        b.slope = juce::jlimit (0, kNumSlopes - 1, (int) p.slope->load());
        b.place = (Place) juce::jlimit (0, kNumPlaces - 1, (int) p.place->load());
    }
    return s;
}

void EqProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    auto main = getBusBuffer (buffer, false, 0);
    const int numCh = std::min (2, main.getNumChannels());
    if (numCh == 0 || n == 0) return;

    bool scOn = false;
    if (getBusCount (true) > 1)
        if (auto* b = getBus (true, 1); b != nullptr && b->isEnabled() && b->getNumberOfChannels() > 0)
        {
            auto sc = getBusBuffer (buffer, true, 1);
            scTap.push (sc.getArrayOfReadPointers(), sc.getNumChannels(), n);
            scOn = true;
        }
    sidechainActive = scOn;

    float* ch[2] = { main.getWritePointer (0), numCh > 1 ? main.getWritePointer (1) : nullptr };
    preTap.push (ch, numCh, n);

    for (int c = 0; c < numCh; ++c)
        for (int i = 0; i < n; ++i) dryRing[(size_t) c][(size_t) ((dryPos + i) & (kRing - 1))] = ch[c][i];

    const auto bands = readBands();
    const Mode m = (Mode) juce::jlimit (0, 2, (int) pMode->load());
    if ((int) m != lastMode) { iir.reset(); lin.reset(); lastMode = (int) m; }
    iir.setTargets (bands, pScale->load(), pGainQ->load() > 0.5f, m);
    iir.setSolo (soloBand.load());

    int latency = 0;
    if (m == Mode::Linear)
    {
        latency = (1 << pid::lpOrder ((int) pRes->load(), sr.load())) / 2;
        if (lin.ready())
        {
            latency = lin.latency();
            lin.process (ch, numCh, n);
        }
        else                                                   // FIR not ready yet: stay time-aligned
            for (int c = 0; c < numCh; ++c)
                for (int i = 0; i < n; ++i) ch[c][i] = dryRing[(size_t) c][(size_t) ((dryPos + i - latency) & (kRing - 1))];
        iir.process (ch, numCh, n, false);
    }
    else
        iir.process (ch, numCh, n, true);
    wantLatency = latency;

    // output: gain (+ auto gain), balance, phase, bypass crossfade against the latency-matched dry signal
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (pOut->load() + (pAuto->load() > 0.5f ? autoGainDb.load() : 0.f)));
    panSm.setTargetValue (pPan->load());
    wetSm.setTargetValue (pBypass->load() > 0.5f ? 0.f : 1.f);
    const float inv = pInv->load() > 0.5f ? -1.f : 1.f;
    float pk[2] = { 0.f, 0.f };
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainSm.getNextValue() * inv, p = panSm.getNextValue(), w = wetSm.getNextValue();
        const float gl = numCh > 1 && p > 0.f ? 1.f - p : 1.f, gr = p < 0.f ? 1.f + p : 1.f;
        for (int c = 0; c < numCh; ++c)
        {
            const float wet = ch[c][i] * g * (c == 0 ? gl : gr);
            float y = wet;
            if (w < 1.f) { const float dry = dryRing[(size_t) c][(size_t) ((dryPos + i - latency) & (kRing - 1))]; y = dry + (wet - dry) * w; }
            ch[c][i] = y;
            pk[c] = std::max (pk[c], std::abs (y));
        }
    }
    dryPos = (dryPos + n) & (kRing - 1);
    postTap.push (ch, numCh, n);
    for (int c = 0; c < numCh; ++c)
        if (pk[c] > outPeak[c].load()) outPeak[c] = pk[c];
    for (int c = numCh; c < main.getNumChannels(); ++c) main.clear (c, 0, n);
}

void EqProcessor::timerCallback()
{
    if (wantLatency.load() != getLatencySamples()) setLatencySamples (wantLatency.load());
}

// ===================================================================== band helpers
void EqProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        const float v = p->convertTo0to1 (value);
        if (std::abs (p->getValue() - v) < 1e-7f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (v);
        p->endChangeGesture();
    }
}

void EqProcessor::writeBand (int i, const BandParams& b)
{
    setParam (pid::band (i, "type"), (float) b.type);
    setParam (pid::band (i, "slope"), (float) b.slope);
    setParam (pid::band (i, "place"), (float) b.place);
    setParam (pid::band (i, "freq"), (float) b.freq);
    setParam (pid::band (i, "gain"), (float) b.gain);
    setParam (pid::band (i, "q"), (float) b.q);
    setParam (pid::band (i, "on"), b.on ? 1.f : 0.f);
    setParam (pid::band (i, "used"), b.used ? 1.f : 0.f);
}

void EqProcessor::clearBand (int i)
{
    BandParams b; b.used = false;
    writeBand (i, b);
    if (soloBand.load() == i) soloBand = -1;
}

int EqProcessor::firstFreeBand() const
{
    for (int i = 0; i < kMaxBands; ++i) if (bp[(size_t) i].used->load() < 0.5f) return i;
    return -1;
}

void EqProcessor::applyBands (const std::vector<BandParams>& list, bool clearOthers)
{
    if (clearOthers) for (int i = 0; i < kMaxBands; ++i) clearBand (i);
    for (const auto& b : list)
    {
        const int i = firstFreeBand();
        if (i < 0) break;
        writeBand (i, b);
    }
}

void EqProcessor::loadParamTree (const juce::ValueTree& t)
{
    for (const auto& c : t)
        if (c.hasType ("PARAM") && c.hasProperty ("id") && c.getProperty ("id").toString() != "bypass")
            setParam (c.getProperty ("id").toString(), (float) c.getProperty ("value"));
}

void EqProcessor::selectSlot (int s)
{
    if (s == activeSlot) return;
    slots[activeSlot] = paramTree();
    activeSlot = s;
    if (slots[s].isValid()) { beginEdit(); loadParamTree (slots[s]); }
}

void EqProcessor::copyToOtherSlot() { slots[1 - activeSlot] = paramTree(); }

// ===================================================================== state
void EqProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("preset", presetName, nullptr);
    st.setProperty ("slot", activeSlot, nullptr);
    st.appendChild (uiState.createCopy(), nullptr);
    for (int s = 0; s < 2; ++s)
        if (slots[s].isValid() && s != activeSlot)
        {
            juce::ValueTree w (s == 0 ? "SlotA" : "SlotB");
            w.appendChild (slots[s].createCopy(), nullptr);
            st.appendChild (w, nullptr);
        }
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void EqProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto st = juce::ValueTree::fromXml (*xml);
    if (! st.hasType (apvts.state.getType())) return;

    if (auto ui = st.getChildWithName ("UI"); ui.isValid()) { uiState = ui.createCopy(); st.removeChild (ui, nullptr); }
    for (int s = 0; s < 2; ++s)
        if (auto w = st.getChildWithName (s == 0 ? "SlotA" : "SlotB"); w.isValid())
        {
            if (w.getNumChildren() > 0) slots[s] = w.getChild (0).createCopy();
            st.removeChild (w, nullptr);
        }
    presetName = st.getProperty ("preset", "Default (flat)").toString();
    activeSlot = juce::jlimit (0, 1, (int) st.getProperty ("slot", 0));
    apvts.replaceState (st);
    undo.clearUndoHistory();
}

juce::AudioProcessorEditor* EqProcessor::createEditor() { return new EqEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EqProcessor(); }
