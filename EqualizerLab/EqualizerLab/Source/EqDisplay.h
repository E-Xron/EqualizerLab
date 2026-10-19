#pragma once
#include "PluginProcessor.h"
#include "Look.h"

// The big interactive EQ display: grid, spectrum analyser (pre / post / sidechain, freeze), the EQ curve(s) and a
// handle per band that you drag, scroll and right-click. Also hosts the EQ Match overlay.
class EqDisplay : public juce::Component, private juce::Timer
{
public:
    explicit EqDisplay (EqProcessor&);
    ~EqDisplay() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    int  focus() const { return focusBand; }
    void setFocus (int band);                     // selects only this band (-1: none)
    std::function<void (int)> onFocusChanged;
    void deleteBands (juce::Array<int>);
    void selectAll();

    // settings (stored in the processor's UI state)
    enum Setting { Range, AnaMode, AnaSidechain, AnaRes, AnaSpeed, AnaTilt, AnaFreeze, AnaRangeDb, Piano, Grab, NumSettings };
    int  setting (Setting) const;
    void setSetting (Setting, int);
    static const std::vector<float>& rangeDb()   { static const std::vector<float> v { 3.f, 6.f, 12.f, 30.f }; return v; }
    void showAnalyserMenu (juce::Component* target);
    void toggleMatchPanel();
    bool matchPanelVisible() const;

    static juce::Colour bandColour (int i);
    static juce::String noteName (double freq);
    static juce::String freqString (double f);

private:
    class MatchPanel;
    void timerCallback() override;

    juce::Rectangle<float> graph() const;
    float  freqToX (double f) const;
    double xToFreq (float x) const;
    float  dbToY (double db) const;
    double yToDb (float y) const;
    float  specToY (float db) const;
    juce::Point<float> handlePos (int i, const eq::BandParams&) const;
    int    hitBand (juce::Point<float>) const;
    void   rebuildCurves();
    double curveDbAt (double f) const;
    void   bandMenu (int band);
    void   emptyMenu (juce::Point<float>);
    void   addBandAt (juce::Point<float>, eq::Type, bool startDrag);
    void   changeFocus (int b);
    void   updateGrab (juce::Point<float>);
    void   drawSpectrum (juce::Graphics&, const SpectrumAnalyser&, juce::Colour, bool fill, bool peak);
    void   drawPiano (juce::Graphics&, juce::Rectangle<float>);

    EqProcessor& proc;
    SpectrumAnalyser pre, post, sc;
    juce::int64 lastPre = 0, lastPost = 0, lastSc = 0;

    // curve cache
    std::vector<double> key;
    int cols = 0;
    std::vector<float> colFreq;
    std::array<std::vector<float>, eq::kMaxBands> bandDb;
    std::array<std::vector<float>, 5> groupDb;
    std::array<bool, 5> groupUsed {};
    BandSet bands;

    // interaction
    juce::Array<int> selected;
    int focusBand = -1, hover = -1;
    struct DragStart { int band; double freq, gain, q; };
    std::vector<DragStart> dragStart;
    bool dragging = false, rubber = false;
    juce::Point<float> downPos, mousePos { -1.f, -1.f };
    juce::Rectangle<float> rubberRect;
    double grabFreq = 0.0; float grabDb = 0.f; bool grabValid = false;

    std::unique_ptr<MatchPanel> matchPanel;
    match::Average refAvg, curAvg;
    bool capRef = false, capCur = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqDisplay)
};
