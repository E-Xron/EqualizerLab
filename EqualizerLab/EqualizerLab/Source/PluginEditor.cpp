#include "PluginEditor.h"
#include "Presets.h"
#include <cmath>
#include <map>

using namespace eq;

namespace
{
    void caption (juce::Graphics& g, const Knob& k)
    {
        if (! k.isVisible()) return;
        g.setColour (Theme::dim.withAlpha (k.isEnabled() ? 1.f : 0.4f));
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (k.name, k.getX() - 10, k.getY() - 14, k.getWidth() + 20, 13, juce::Justification::centred);
    }

    void panelBox (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setGradientFill (juce::ColourGradient (Theme::panel2, 0.f, r.getY(), Theme::panel, 0.f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (Theme::border); g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);
    }
}

// ===================================================================== band panel
BandPanel::BandPanel (EqProcessor& p, EqDisplay& d) : proc (p), display (d)
{
    for (auto* c : std::initializer_list<juce::Component*> { &power, &type, &slope, &place, &freq, &gain, &q, &prev, &next, &solo, &del })
        addAndMakeVisible (c);
    power.getProperties().set ("power", true);
    power.setColour (juce::ToggleButton::tickColourId, Theme::accent2);
    for (int i = 0; i < kNumTypes; ++i) type.addItem (typeNames()[i], i + 1);
    for (int i = 0; i < kNumSlopes; ++i) slope.addItem (juce::String (slopeDb()[i]) + " dB/oct", i + 1);
    for (int i = 0; i < kNumPlaces; ++i) place.addItem (placeNames()[i], i + 1);
    freq.setDoubleClickReturnValue (true, 1000.0);
    gain.setDoubleClickReturnValue (true, 0.0);
    q.setDoubleClickReturnValue (true, 1.0);
    solo.setClickingTogglesState (true);
    solo.setColour (juce::TextButton::buttonOnColourId, Theme::warm.darker (0.2f));
    solo.setTooltip ("Listen to just the part of the sound this band affects");
    solo.onClick = [this] { proc.soloBand = solo.getToggleState() ? band : -1; };
    del.onClick = [this] { if (band >= 0) display.deleteBands ({ band }); };
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    setBand (-1);
    startTimerHz (10);
}

void BandPanel::step (int dir)
{
    const auto b = proc.readBands();
    for (int k = 1; k <= kMaxBands; ++k)
    {
        const int i = ((band < 0 ? (dir > 0 ? -1 : 0) : band) + dir * k + kMaxBands * 2) % kMaxBands;
        if (b[(size_t) i].used) { display.setFocus (i); return; }
    }
}

void BandPanel::setBand (int b)
{
    aPower.reset(); aType.reset(); aSlope.reset(); aPlace.reset(); aFreq.reset(); aGain.reset(); aQ.reset();
    band = b;
    const bool has = b >= 0;
    for (auto* c : std::initializer_list<juce::Component*> { &power, &type, &slope, &place, &freq, &gain, &q, &solo, &del })
        c->setVisible (has);
    if (has)
    {
        auto& s = proc.apvts;
        using A = juce::AudioProcessorValueTreeState;
        aPower = std::make_unique<A::ButtonAttachment> (s, pid::band (b, "on"), power);
        aType  = std::make_unique<A::ComboBoxAttachment> (s, pid::band (b, "type"), type);
        aSlope = std::make_unique<A::ComboBoxAttachment> (s, pid::band (b, "slope"), slope);
        aPlace = std::make_unique<A::ComboBoxAttachment> (s, pid::band (b, "place"), place);
        aFreq  = std::make_unique<A::SliderAttachment> (s, pid::band (b, "freq"), freq);
        aGain  = std::make_unique<A::SliderAttachment> (s, pid::band (b, "gain"), gain);
        aQ     = std::make_unique<A::SliderAttachment> (s, pid::band (b, "q"), q);
        freq.setColour (juce::Slider::rotarySliderFillColourId, EqDisplay::bandColour (b));
        solo.setToggleState (proc.soloBand.load() == b, juce::dontSendNotification);
    }
    timerCallback();
    resized();
    repaint();
}

void BandPanel::timerCallback()
{
    if (band < 0) return;
    const auto t = (Type) juce::jlimit (0, kNumTypes - 1, (int) proc.pv (pid::band (band, "type")));
    gain.setEnabled (typeHasGain (t));
    slope.setEnabled (typeHasSlope (t));
    place.setEnabled (proc.isStereo());
    solo.setToggleState (proc.soloBand.load() == band, juce::dontSendNotification);
}

void BandPanel::paint (juce::Graphics& g)
{
    panelBox (g, getLocalBounds().toFloat());
    if (band < 0)
    {
        g.setColour (Theme::dim); g.setFont (juce::FontOptions (13.f));
        g.drawText ("No band selected. Double-click the display to add one.", getLocalBounds().withTrimmedLeft (60), juce::Justification::centred);
        return;
    }
    g.setColour (EqDisplay::bandColour (band));
    g.setFont (juce::FontOptions (15.f, juce::Font::bold));
    g.drawText ("BAND " + juce::String (band + 1), 42, 8, 76, 20, juce::Justification::centredLeft);
    const double f = proc.pv (pid::band (band, "freq"));
    g.setColour (Theme::dim); g.setFont (juce::FontOptions (11.f));
    g.drawText (EqDisplay::noteName (f), 42, 26, 76, 14, juce::Justification::centredLeft);
    for (auto* k : { &freq, &gain, &q }) caption (g, *k);
}

void BandPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    prev.setBounds (r.getX(), r.getY(), 26, 22);
    next.setBounds (r.getX(), r.getY() + 26, 26, 22);
    power.setBounds (r.getX() + 30, r.getY() + 46, 28, 28);
    r.removeFromLeft (112);
    auto left = r.removeFromLeft (124);
    type.setBounds (left.removeFromTop (26)); left.removeFromTop (6);
    slope.setBounds (left.removeFromTop (26)); left.removeFromTop (6);
    place.setBounds (left.removeFromTop (26));
    r.removeFromLeft (8);
    auto right = r.removeFromRight (70);
    solo.setBounds (right.removeFromTop (30)); right.removeFromTop (8);
    del.setBounds (right.removeFromTop (30));
    r.removeFromRight (8);
    const int kw = std::min (100, r.getWidth() / 3);
    r.removeFromTop (14);
    r = r.withSizeKeepingCentre (kw * 3, r.getHeight());
    for (auto* k : { &freq, &gain, &q }) k->setBounds (r.removeFromLeft (kw).withTrimmedBottom (0));
}

// ===================================================================== meter
void OutMeter::set (float l, float r)
{
    const float v[2] { l, r };
    for (int c = 0; c < 2; ++c)
    {
        const float db = juce::Decibels::gainToDecibels (v[c], -60.f);
        lv[c] = std::max (db, lv[c] - 1.5f);
        if (db >= hold[c]) { hold[c] = db; holdT[c] = 40; } else if (--holdT[c] < 0) hold[c] = std::max (-60.f, hold[c] - 1.f);
    }
    repaint();
}

void OutMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (r, 4.f);
    const float w = (r.getWidth() - 6.f) / 2.f;
    for (int c = 0; c < 2; ++c)
    {
        auto bar = juce::Rectangle<float> (r.getX() + 2.f + (float) c * (w + 2.f), r.getY() + 2.f, w, r.getHeight() - 4.f);
        auto toY = [&] (float db) { return bar.getBottom() - bar.getHeight() * juce::jlimit (0.f, 1.f, (db + 60.f) / 66.f); };
        const float y = toY (lv[c]);
        g.setGradientFill (juce::ColourGradient (Theme::red, 0.f, toY (6.f), Theme::accent2, 0.f, toY (-24.f), false));
        g.fillRect (bar.withTop (y));
        g.setColour (hold[c] > 0.f ? Theme::red : Theme::text);
        g.fillRect (bar.getX(), toY (hold[c]), bar.getWidth(), 1.5f);
    }
}

// ===================================================================== editor
EqEditor::EqEditor (EqProcessor& p)
    : AudioProcessorEditor (p), proc (p), display (p), panel (p, display)
{
    addAndMakeVisible (display);
    addAndMakeVisible (panel);
    display.onFocusChanged = [this] (int b) { panel.setBand (b); };

    for (auto* c : std::initializer_list<juce::Component*> { &presetBtn, &presetPrev, &presetNext, &saveBtn, &slotA, &slotB, &copyBtn, &undoBtn, &redoBtn,
                                                             &matchBtn, &bypass, &mode, &res, &range, &analyser, &gainQ, &autoGain, &phase, &scale, &out, &pan, &meter })
        addAndMakeVisible (c);

    presetBtn.onClick = [this] { presetMenu(); };
    presetPrev.onClick = [this] { stepPreset (-1); };
    presetNext.onClick = [this] { stepPreset (1); };
    saveBtn.onClick = [this] { savePreset(); };
    slotA.setRadioGroupId (7); slotB.setRadioGroupId (7);
    slotA.setClickingTogglesState (true); slotB.setClickingTogglesState (true);
    slotA.onClick = [this] { if (slotA.getToggleState()) proc.selectSlot (0); };
    slotB.onClick = [this] { if (slotB.getToggleState()) proc.selectSlot (1); };
    copyBtn.setTooltip ("Copy this setting to the other slot");
    copyBtn.onClick = [this] { proc.copyToOtherSlot(); };
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    matchBtn.setClickingTogglesState (true);
    matchBtn.onClick = [this] { display.toggleMatchPanel(); };
    bypass.getProperties().set ("power", true);
    bypass.setColour (juce::ToggleButton::tickColourId, Theme::red);
    bypass.setTooltip ("Bypass");

    for (int i = 0; i < 3; ++i) mode.addItem (modeNames()[i], i + 1);
    for (int i = 0; i < pid::lpResNames().size(); ++i) res.addItem ("LP " + pid::lpResNames()[i], i + 1);
    for (size_t i = 0; i < EqDisplay::rangeDb().size(); ++i) range.addItem ("+/- " + juce::String ((int) EqDisplay::rangeDb()[i]) + " dB", (int) i + 1);
    range.setSelectedId (display.setting (EqDisplay::Range) + 1, juce::dontSendNotification);
    range.onChange = [this] { display.setSetting (EqDisplay::Range, range.getSelectedId() - 1); };
    analyser.onClick = [this] { display.showAnalyserMenu (&analyser); };

    scale.setDoubleClickReturnValue (true, 1.0);
    out.setDoubleClickReturnValue (true, 0.0);
    pan.setDoubleClickReturnValue (true, 0.0);

    using A = juce::AudioProcessorValueTreeState;
    aMode = std::make_unique<A::ComboBoxAttachment> (p.apvts, "mode", mode);
    aRes = std::make_unique<A::ComboBoxAttachment> (p.apvts, "lpRes", res);
    aScale = std::make_unique<A::SliderAttachment> (p.apvts, "gainScale", scale);
    aOut = std::make_unique<A::SliderAttachment> (p.apvts, "outGain", out);
    aPan = std::make_unique<A::SliderAttachment> (p.apvts, "outPan", pan);
    aBypass = std::make_unique<A::ButtonAttachment> (p.apvts, "bypass", bypass);
    aGainQ = std::make_unique<A::ButtonAttachment> (p.apvts, "gainQ", gainQ);
    aAuto = std::make_unique<A::ButtonAttachment> (p.apvts, "autoGain", autoGain);
    aPhase = std::make_unique<A::ButtonAttachment> (p.apvts, "phaseInv", phase);
    gainQ.setTooltip ("Bells get narrower as you boost or cut them, like an analog console");
    phase.setTooltip ("Flip the output polarity (phase invert)");
    autoGain.setTooltip ("Keeps the overall level the same so you can compare with bypass fairly");

    setLookAndFeel (&laf);                                   // after the children exist, so they all pick it up
    setResizable (true, true);
    setResizeLimits (980, 560, 2400, 1500);
    setSize ((int) proc.uiState.getProperty ("w", 1100), (int) proc.uiState.getProperty ("h", 680));
    startTimerHz (30);
}

EqEditor::~EqEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EqEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
    // header
    auto top = getLocalBounds().removeFromTop (kTop).toFloat();
    g.setGradientFill (juce::ColourGradient (Theme::panel2, 0.f, 0.f, Theme::panel, 0.f, top.getBottom(), false));
    g.fillRect (top);
    g.setColour (Theme::border); g.drawHorizontalLine (kTop - 1, 0.f, (float) getWidth());
    g.setFont (juce::FontOptions (20.f, juce::Font::bold));
    g.setColour (Theme::text);
    g.drawText ("Equalizer", 16, 0, 100, kTop, juce::Justification::centredLeft);
    g.setColour (Theme::accent2);
    g.drawText ("Lab", 108, 0, 50, kTop, juce::Justification::centredLeft);

    // bottom bar boxes
    auto b = getLocalBounds().removeFromBottom (kBottom).reduced (8, 6).toFloat();
    panelBox (g, b.removeFromLeft (200.f));
    panelBox (g, b.removeFromRight (310.f));
    for (auto* k : { &scale, &out, &pan }) caption (g, *k);

    const int lat = proc.getLatencySamples();
    g.setColour (Theme::dim); g.setFont (juce::FontOptions (11.f));
    if (lat > 0)
        g.drawText ("Latency " + juce::String (lat / proc.getSr() * 1000.0, 1) + " ms", 18, getHeight() - 26, 180, 14, juce::Justification::centredLeft);
}

void EqEditor::resized()
{
    proc.uiState.setProperty ("w", getWidth(), nullptr);
    proc.uiState.setProperty ("h", getHeight(), nullptr);

    auto top = getLocalBounds().removeFromTop (kTop).reduced (10, 9);
    top.removeFromLeft (150);
    bypass.setBounds (top.removeFromRight (30));
    top.removeFromRight (10);
    matchBtn.setBounds (top.removeFromRight (90)); top.removeFromRight (14);
    redoBtn.setBounds (top.removeFromRight (56)); top.removeFromRight (4);
    undoBtn.setBounds (top.removeFromRight (56)); top.removeFromRight (14);
    copyBtn.setBounds (top.removeFromRight (56)); top.removeFromRight (4);
    slotB.setBounds (top.removeFromRight (30));
    slotA.setBounds (top.removeFromRight (30));
    slotA.setConnectedEdges (juce::Button::ConnectedOnRight); slotB.setConnectedEdges (juce::Button::ConnectedOnLeft);
    top.removeFromRight (14);
    presetPrev.setBounds (top.removeFromLeft (28)); top.removeFromLeft (2);
    saveBtn.setBounds (top.removeFromRight (56)); top.removeFromRight (2);
    presetNext.setBounds (top.removeFromRight (28)); top.removeFromRight (2);
    presetBtn.setBounds (top);

    auto r = getLocalBounds();
    r.removeFromTop (kTop);
    auto bottom = r.removeFromBottom (kBottom).reduced (8, 6);
    display.setBounds (r.reduced (8, 6));

    auto left = bottom.removeFromLeft (200).reduced (10, 8);
    auto row = left.removeFromTop (24);
    mode.setBounds (row); left.removeFromTop (5);
    row = left.removeFromTop (24);
    res.setBounds (row.removeFromLeft (row.getWidth() / 2 - 2)); row.removeFromLeft (4); range.setBounds (row);
    left.removeFromTop (5);
    row = left.removeFromTop (24);
    analyser.setBounds (row.removeFromLeft (row.getWidth() / 2 - 2)); row.removeFromLeft (4); gainQ.setBounds (row);

    auto right = bottom.removeFromRight (310).reduced (10, 8);
    meter.setBounds (right.removeFromRight (18)); right.removeFromRight (8);
    auto toggles = right.removeFromRight (96);
    autoGain.setBounds (toggles.removeFromTop (30)); toggles.removeFromTop (4);
    phase.setBounds (toggles.removeFromTop (30));
    right.removeFromTop (14);
    const int kw = right.getWidth() / 3;
    for (auto* k : { &scale, &out, &pan }) k->setBounds (right.removeFromLeft (kw));

    bottom.removeFromLeft (8); bottom.removeFromRight (8);
    panel.setBounds (bottom);
}

void EqEditor::timerCallback()
{
    undoBtn.setEnabled (proc.undo.canUndo());
    redoBtn.setEnabled (proc.undo.canRedo());
    slotA.setToggleState (proc.slot() == 0, juce::dontSendNotification);
    slotB.setToggleState (proc.slot() == 1, juce::dontSendNotification);
    res.setEnabled (proc.mode() == Mode::Linear);
    matchBtn.setToggleState (display.matchPanelVisible(), juce::dontSendNotification);
    if (presetBtn.getButtonText() != proc.presetName) presetBtn.setButtonText (proc.presetName);
    meter.set (proc.outPeak[0].exchange (0.f), proc.isStereo() ? proc.outPeak[1].exchange (0.f) : proc.outPeak[0].load());
    static int slow = 0;
    if (++slow % 10 == 0) repaint (0, getHeight() - kBottom, 200, kBottom);
}

bool EqEditor::keyPressed (const juce::KeyPress& k) { return display.keyPressed (k); }

// ---- presets
juce::File EqEditor::presetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("EqualizerLab Presets");
}

void EqEditor::loadFactory (int index)
{
    const auto& list = presets::factory();
    if (index < 0 || index >= (int) list.size()) return;
    proc.beginEdit();
    proc.applyBands (list[(size_t) index].bands, true);
    proc.presetName = list[(size_t) index].name;
    display.setFocus (-1);
}

void EqEditor::loadPresetFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return;
    proc.beginEdit();
    for (int i = 0; i < kMaxBands; ++i) proc.clearBand (i);
    proc.loadParamTree (juce::ValueTree::fromXml (*xml));
    proc.presetName = f.getFileNameWithoutExtension();
    display.setFocus (-1);
}

void EqEditor::presetMenu()
{
    juce::PopupMenu m;
    const auto& list = presets::factory();
    std::map<juce::String, juce::PopupMenu> cats;
    juce::StringArray order;
    for (int i = 0; i < (int) list.size(); ++i)
    {
        const juce::String c = list[(size_t) i].category;
        if (! order.contains (c)) order.add (c);
        cats[c].addItem (1 + i, list[(size_t) i].name, true, proc.presetName == list[(size_t) i].name);
    }
    for (auto& c : order) m.addSubMenu (c, cats[c]);
    const auto files = presetFolder().findChildFiles (juce::File::findFiles, false, "*.eqlpreset");
    juce::PopupMenu user;
    for (int i = 0; i < files.size(); ++i) user.addItem (1000 + i, files[i].getFileNameWithoutExtension());
    m.addSeparator();
    m.addSubMenu ("User presets", user, ! files.isEmpty());
    m.addItem (999, "Save preset...");
    m.addItem (998, "Load preset file...");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [this, files] (int r)
    {
        if (r >= 1000) loadPresetFile (files[r - 1000]);
        else if (r == 999) savePreset();
        else if (r == 998)
        {
            chooser = std::make_unique<juce::FileChooser> ("Load EqualizerLab preset", presetFolder(), "*.eqlpreset");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) loadPresetFile (fc.getResult()); });
        }
        else if (r >= 1) loadFactory (r - 1);
    });
}

void EqEditor::stepPreset (int dir)
{
    const auto& list = presets::factory();
    const auto files = presetFolder().findChildFiles (juce::File::findFiles, false, "*.eqlpreset");
    const int total = (int) list.size() + files.size();
    int cur = -1;
    for (int i = 0; i < (int) list.size(); ++i) if (proc.presetName == list[(size_t) i].name) cur = i;
    for (int i = 0; i < files.size(); ++i) if (proc.presetName == files[i].getFileNameWithoutExtension()) cur = (int) list.size() + i;
    const int nx = ((cur < 0 ? (dir > 0 ? -1 : 0) : cur) + dir + total) % total;
    if (nx < (int) list.size()) loadFactory (nx); else loadPresetFile (files[nx - (int) list.size()]);
}

void EqEditor::savePreset()
{
    presetFolder().createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save EqualizerLab preset", presetFolder().getChildFile (proc.presetName + ".eqlpreset"), "*.eqlpreset");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension ("eqlpreset");
        if (auto xml = proc.paramTree().createXml())
            if (xml->writeTo (f)) proc.presetName = f.getFileNameWithoutExtension();
    });
}
