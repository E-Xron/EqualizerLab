#pragma once
#include "PluginProcessor.h"
#include "EqDisplay.h"
#include "Look.h"

// Knob with its caption painted above.
class Knob : public juce::Slider
{
public:
    Knob (const juce::String& caption, juce::Colour c) : name (caption)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
        setColour (juce::Slider::rotarySliderFillColourId, c);
    }
    juce::String name;
};

// Controls for the focused band: on/off, shape, slope, stereo placement, frequency / gain / Q, solo, delete.
class BandPanel : public juce::Component, private juce::Timer
{
public:
    BandPanel (EqProcessor&, EqDisplay&);
    void setBand (int band);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void step (int dir);

    EqProcessor& proc;
    EqDisplay& display;
    int band = -1;
    juce::ToggleButton power;
    juce::ComboBox type, slope, place;
    Knob freq { "FREQ", Theme::accent }, gain { "GAIN", Theme::warm }, q { "Q", Theme::accent2 };
    juce::TextButton prev { "<" }, next { ">" }, solo { "Solo" }, del { "Delete" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aPower;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aType, aSlope, aPlace;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aFreq, aGain, aQ;
};

// Stereo output peak meter with a held peak.
class OutMeter : public juce::Component
{
public:
    void set (float l, float r);
    void paint (juce::Graphics&) override;
private:
    float lv[2] {}, hold[2] {}; int holdT[2] {};
};

class EqEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EqEditor (EqProcessor&);
    ~EqEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void presetMenu();
    void stepPreset (int dir);
    void savePreset();
    void loadPresetFile (const juce::File&);
    void loadFactory (int index);
    static juce::File presetFolder();

    EqProcessor& proc;
    KnobLAF laf;
    EqDisplay display;
    BandPanel panel;

    juce::TextButton presetBtn, presetPrev { "<" }, presetNext { ">" }, saveBtn { "Save" };
    juce::TextButton slotA { "A" }, slotB { "B" }, copyBtn { "Copy" }, undoBtn { "Undo" }, redoBtn { "Redo" }, matchBtn { "EQ Match" };
    juce::ToggleButton bypass;
    juce::ComboBox mode, res, range;
    juce::TextButton analyser { "Analyzer" };
    juce::ToggleButton gainQ { "Gain-Q" }, autoGain { "Auto Gain" }, phase { "Polarity" };
    Knob scale { "GAIN SCALE", Theme::accent }, out { "OUTPUT", Theme::accent2 }, pan { "PAN", Theme::dim };
    OutMeter meter;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aMode, aRes;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aScale, aOut, aPan;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> aBypass, aGainQ, aAuto, aPhase;
    std::unique_ptr<juce::FileChooser> chooser;
    static constexpr int kTop = 48, kBottom = 124;
};
