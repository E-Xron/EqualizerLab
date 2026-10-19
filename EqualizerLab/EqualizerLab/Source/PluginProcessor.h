#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <memory>
#include "FilterDesign.h"
#include "EqEngine.h"
#include "Analysis.h"

namespace pid
{
    inline juce::String band (int i, const char* what) { return "b" + juce::String (i + 1) + "_" + what; }
    // linear-phase FIR length per resolution at 44.1/48 kHz (doubled at 88.2/96, quadrupled at 176.4/192)
    inline int lpOrder (int res, double fs) { return 12 + juce::jlimit (0, 4, res) + (fs > 50000.0 ? 1 : 0) + (fs > 100000.0 ? 1 : 0); }
    inline const juce::StringArray& lpResNames() { static juce::StringArray n { "Low", "Medium", "High", "Very High", "Maximum" }; return n; }
}

class EqProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    EqProcessor();
    ~EqProcessor() override;

    void prepareToPlay (double sampleRate, int blockSize) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "EqualizerLab"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;

    // ---- band helpers (message thread)
    BandSet readBands() const;                                  // current parameter values (any thread)
    eq::Mode mode() const { return (eq::Mode) (int) pv ("mode"); }
    float pv (const juce::String& id) const { return const_cast<juce::AudioProcessorValueTreeState&> (apvts).getRawParameterValue (id)->load(); }
    void setParam (const juce::String& id, float value);        // plain value, notifies host
    void writeBand (int i, const eq::BandParams&);
    void clearBand (int i);
    int  firstFreeBand() const;
    void applyBands (const std::vector<eq::BandParams>&, bool clearOthers);
    void beginEdit() { undo.beginNewTransaction(); }

    // presets / compare
    void loadParamTree (const juce::ValueTree&);                // sets every parameter in the tree (undoable)
    juce::ValueTree paramTree() { return apvts.copyState(); }
    void selectSlot (int s);                                    // A = 0, B = 1
    void copyToOtherSlot();
    int  slot() const { return activeSlot; }
    juce::String presetName { "Default (flat)" };

    // ---- state read by the UI
    ScopeTap preTap, postTap, scTap;
    std::atomic<float> outPeak[2] { { 0.f }, { 0.f } };
    std::atomic<int> soloBand { -1 };
    std::atomic<float> autoGainDb { 0.f };
    std::atomic<bool> sidechainActive { false };
    double getSr() const { return sr.load(); }
    bool isStereo() const { return getTotalNumOutputChannels() >= 2; }
    juce::ValueTree uiState { "UI" };                           // analyser + display settings, saved with the session

private:
    class Worker;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;

    IirEq iir;
    LinearPhaseEq lin;
    std::unique_ptr<Worker> worker;
    std::atomic<double> sr { 48000.0 };
    std::atomic<int> wantLatency { 0 };

    struct BandPtrs { std::atomic<float>* used, * on, * freq, * gain, * q, * type, * slope, * place; };
    std::array<BandPtrs, eq::kMaxBands> bp {};
    std::atomic<float>* pMode, * pRes, * pScale, * pOut, * pPan, * pAuto, * pInv, * pGainQ, * pBypass;

    // audio thread
    std::array<std::vector<float>, 2> dryRing;
    int dryPos = 0, lastMode = -1;
    juce::SmoothedValue<float> outGainSm, panSm, wetSm;
    juce::AudioBuffer<float> dryBuf;

    juce::ValueTree slots[2];
    int activeSlot = 0;
};
