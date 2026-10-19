#include "EqDisplay.h"
#include <cmath>

using namespace eq;

namespace
{
    constexpr float kHandle = 8.f;
    constexpr double kFMin = 10.0, kFMax = 30000.0;
    const char* const kSettingIds[] { "range", "anaMode", "anaSc", "anaRes", "anaSpeed", "anaTilt", "anaFreeze", "anaRangeDb", "piano", "grab" };
    const int kSettingDefaults[] { 2, 3, 1, 1, 2, 3, 0, 1, 0, 1 };
    const float kSpeeds[] { 0.4f, 0.8f, 1.5f, 3.f, 6.f };
    const float kTilts[]  { 0.f, 1.5f, 3.f, 4.5f, 6.f };
    const float kAnaRanges[] { 60.f, 90.f, 120.f };

    const juce::Colour kCurve { 0xfffcd34d }, kCurveL { 0xff60a5fa }, kCurveR { 0xfff87171 }, kCurveM { 0xff4ade80 }, kCurveS { 0xfff472b6 };
}

juce::Colour EqDisplay::bandColour (int i)
{
    static const juce::uint32 c[] { 0xffff6b6b, 0xffffa94d, 0xffffd43b, 0xff8ce99a, 0xff38d9a9, 0xff4dabf7, 0xff748ffc, 0xffb197fc, 0xfff783ac, 0xff66d9e8, 0xffc0eb75, 0xffffc078 };
    return juce::Colour (c[(size_t) i % 12]);
}

juce::String EqDisplay::freqString (double f)
{
    return f >= 1000.0 ? juce::String (f / 1000.0, f >= 10000.0 ? 1 : 2) + " kHz" : juce::String (f, f < 100.0 ? 1 : 0) + " Hz";
}

juce::String EqDisplay::noteName (double f)
{
    static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double midi = 69.0 + 12.0 * std::log2 (f / 440.0);
    const int m = (int) std::lround (midi);
    const int cents = (int) std::lround ((midi - m) * 100.0);
    juce::String s = juce::String (n[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1);
    if (cents != 0) s << (cents > 0 ? " +" : " ") << cents << "c";
    return s;
}

// ===================================================================== EQ Match overlay
class EqDisplay::MatchPanel : public juce::Component, private juce::Timer
{
public:
    explicit MatchPanel (EqDisplay& d) : disp (d)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &source, &refBtn, &curBtn, &bands, &amount, &apply, &clear, &close, &replace })
            addAndMakeVisible (c);
        source.addItem ("Reference: Sidechain input", 1);
        source.addItem ("Reference: Capture this track", 2);
        source.setSelectedId (1, juce::dontSendNotification);
        refBtn.setClickingTogglesState (true); curBtn.setClickingTogglesState (true);
        refBtn.onClick = [this] { disp.capRef = refBtn.getToggleState(); if (disp.capRef) disp.refAvg.clear(); };
        curBtn.onClick = [this] { disp.capCur = curBtn.getToggleState(); if (disp.capCur) disp.curAvg.clear(); };
        for (auto* s : { &bands, &amount })
        {
            s->setSliderStyle (juce::Slider::LinearHorizontal);
            s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
            s->setColour (juce::Slider::trackColourId, Theme::accent);
        }
        bands.setRange (1, 24, 1); bands.setValue (8); bands.setTextValueSuffix (" bands");
        amount.setRange (0, 100, 1); amount.setValue (80); amount.setTextValueSuffix ("%");
        replace.setToggleState (true, juce::dontSendNotification);
        apply.onClick = [this] { doMatch(); };
        clear.onClick = [this] { disp.refAvg.clear(); disp.curAvg.clear(); stopCapture(); };
        close.onClick = [this] { stopCapture(); setVisible (false); disp.repaint(); };
        startTimerHz (5);
    }

    bool useSidechain() const { return source.getSelectedId() == 1; }
    void stopCapture()
    {
        refBtn.setToggleState (false, juce::dontSendNotification); curBtn.setToggleState (false, juce::dontSendNotification);
        disp.capRef = disp.capCur = false;
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Theme::panel.withAlpha (0.95f)); g.fillRoundedRectangle (r, 8.f);
        g.setColour (Theme::border); g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);
        g.setColour (Theme::text); g.setFont (juce::FontOptions (14.f, juce::Font::bold));
        g.drawText ("EQ Match", 12, 6, 200, 20, juce::Justification::centredLeft);
        g.setColour (Theme::dim); g.setFont (juce::FontOptions (11.5f));
        auto secs = [] (const match::Average& a) { return juce::String (a.frames / 30.0, 1) + " s"; };     // one frame per display refresh
        g.drawText ("Reference " + secs (disp.refAvg) + "   Current " + secs (disp.curAvg),
                    12, getHeight() - 24, getWidth() - 24, 18, juce::Justification::centredLeft);
        if (useSidechain() && ! disp.proc.sidechainActive.load())
        {
            g.setColour (Theme::warm);
            g.drawText ("Route a reference to the sidechain input", 100, 6, getWidth() - 140, 20, juce::Justification::centredRight);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        r.removeFromTop (24);
        close.setBounds (getWidth() - 30, 6, 22, 20);
        source.setBounds (r.removeFromTop (24)); r.removeFromTop (6);
        auto row = r.removeFromTop (26);
        refBtn.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3)); row.removeFromLeft (6); curBtn.setBounds (row);
        r.removeFromTop (6);
        bands.setBounds (r.removeFromTop (22)); r.removeFromTop (4);
        amount.setBounds (r.removeFromTop (22)); r.removeFromTop (6);
        row = r.removeFromTop (26);
        replace.setBounds (row.removeFromLeft (110));
        clear.setBounds (row.removeFromRight (60)); row.removeFromRight (6);
        apply.setBounds (row.removeFromRight (70));
    }

private:
    void timerCallback() override { repaint(); }

    void doMatch()
    {
        if (! disp.refAvg.valid() || ! disp.curAvg.valid())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "EQ Match",
                "Capture a reference and the current track first: switch on each Capture button while audio plays, then switch it off.");
            return;
        }
        stopCapture();
        auto list = match::fit (disp.refAvg, disp.curAvg, (int) bands.getValue(), amount.getValue() / 100.0);
        disp.proc.beginEdit();
        disp.proc.applyBands (list, replace.getToggleState());
        disp.setFocus (-1);
    }

    EqDisplay& disp;
    juce::ComboBox source;
    juce::TextButton refBtn { "Capture Reference" }, curBtn { "Capture Current" }, apply { "Match" }, clear { "Clear" }, close { "x" };
    juce::Slider bands, amount;
    juce::ToggleButton replace { "Replace bands" };
};

// ===================================================================== display
EqDisplay::EqDisplay (EqProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    matchPanel = std::make_unique<MatchPanel> (*this);
    addChildComponent (*matchPanel);
    startTimerHz (30);
}

EqDisplay::~EqDisplay() { stopTimer(); }

int EqDisplay::setting (Setting s) const
{
    return (int) proc.uiState.getProperty (kSettingIds[s], kSettingDefaults[s]);
}

void EqDisplay::setSetting (Setting s, int v)
{
    proc.uiState.setProperty (kSettingIds[s], v, nullptr);
    key.clear();
    repaint();
}

void EqDisplay::toggleMatchPanel()
{
    matchPanel->setVisible (! matchPanel->isVisible());
    if (! matchPanel->isVisible()) matchPanel->stopCapture();
    repaint();
}

bool EqDisplay::matchPanelVisible() const { return matchPanel->isVisible(); }

void EqDisplay::resized()
{
    matchPanel->setBounds (14, 12, 300, 200);
    key.clear();
}

// ---- coordinates
juce::Rectangle<float> EqDisplay::graph() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromRight (38.f);
    r.removeFromBottom (setting (Piano) ? 34.f : 18.f);
    return r;
}
float EqDisplay::freqToX (double f) const { const auto g = graph(); return g.getX() + (float) (std::log (f / kFMin) / std::log (kFMax / kFMin)) * g.getWidth(); }
double EqDisplay::xToFreq (float x) const { const auto g = graph(); return kFMin * std::pow (kFMax / kFMin, (double) ((x - g.getX()) / g.getWidth())); }
float EqDisplay::dbToY (double db) const { const auto g = graph(); const double R = rangeDb()[(size_t) setting (Range)]; return g.getY() + (float) ((R - db) / (2.0 * R)) * g.getHeight(); }
double EqDisplay::yToDb (float y) const { const auto g = graph(); const double R = rangeDb()[(size_t) setting (Range)]; return R - (double) ((y - g.getY()) / g.getHeight()) * 2.0 * R; }
float EqDisplay::specToY (float db) const
{
    const auto g = graph();
    const float R = kAnaRanges[setting (AnaRangeDb)];
    return g.getY() + (-db / R) * g.getHeight();
}

juce::Point<float> EqDisplay::handlePos (int, const BandParams& b) const
{
    const double gs = proc.pv ("gainScale");
    double db = 0.0;
    if (typeHasGain (b.type)) db = b.gain * gs;
    else if (b.type == Type::LowCut || b.type == Type::HighCut) db = gainToDb (b.q / std::sqrt (0.5));
    const double R = rangeDb()[(size_t) setting (Range)];
    return { freqToX (b.freq), dbToY (juce::jlimit (-R, R, db)) };
}

int EqDisplay::hitBand (juce::Point<float> p) const
{
    int best = -1; float bd = kHandle + 5.f;
    for (int i = kMaxBands; --i >= 0;)
    {
        const auto& b = bands[(size_t) i];
        if (! b.used) continue;
        const float d = handlePos (i, b).getDistanceFrom (p);
        if (d < bd || (i == focusBand && d < kHandle + 5.f && best < 0)) { bd = d; best = i; }
    }
    return best;
}

// ---- curves
void EqDisplay::rebuildCurves()
{
    bands = proc.readBands();
    const double fs = proc.getSr(), gs = proc.pv ("gainScale");
    const bool gq = proc.pv ("gainQ") > 0.5f;
    const Mode m = proc.mode();
    const auto g = graph();
    const int n = std::max (2, (int) g.getWidth() / 2);

    std::vector<double> k { fs, gs, gq ? 1.0 : 0.0, (double) m, (double) n, (double) g.getHeight(), (double) g.getX() };
    for (const auto& b : bands) k.insert (k.end(), { b.used ? 1.0 : 0.0, b.on ? 1.0 : 0.0, (double) b.type, b.freq, b.gain, b.q, (double) b.slope, (double) b.place });
    if (k == key) return;
    key = k;

    cols = n;
    colFreq.resize ((size_t) n);
    for (int i = 0; i < n; ++i) colFreq[(size_t) i] = (float) xToFreq (g.getX() + g.getWidth() * (float) i / (float) (n - 1));
    for (auto& v : groupDb) v.assign ((size_t) n, 0.f);
    groupUsed.fill (false);
    const bool stereo = proc.isStereo();

    for (int bi = 0; bi < kMaxBands; ++bi)
    {
        auto& v = bandDb[(size_t) bi];
        const auto& b = bands[(size_t) bi];
        if (! b.used) { v.clear(); continue; }
        BandParams on = b; on.on = true;                          // disabled bands still show their (dimmed) shape
        const auto an = design (on, gs, gq);
        const auto dg = digital (an, fs, m);
        v.resize ((size_t) n);
        for (int i = 0; i < n; ++i) v[(size_t) i] = (float) gainToDb (bandMag (an, dg, colFreq[(size_t) i], fs, m));
        if (! b.on) continue;
        int grp = (int) b.place;
        if (! stereo) { if (b.place == Place::Side) continue; grp = 0; }
        groupUsed[(size_t) grp] = true;
        for (int i = 0; i < n; ++i) groupDb[(size_t) grp][(size_t) i] += v[(size_t) i];
    }
}

double EqDisplay::curveDbAt (double f) const
{
    if (cols < 2) return 0.0;
    const double t = std::log (f / colFreq[0]) / std::log ((double) colFreq[(size_t) cols - 1] / colFreq[0]) * (cols - 1);
    const int i = juce::jlimit (0, cols - 1, (int) std::lround (t));
    return groupDb[0][(size_t) i];
}

// ---- timer
void EqDisplay::timerCallback()
{
    const double fs = proc.getSr();
    const int order = 11 + juce::jlimit (0, 3, setting (AnaRes));
    for (auto* a : { &pre, &post, &sc }) a->setOrder (order);
    const float rel = kSpeeds[juce::jlimit (0, 4, setting (AnaSpeed))], tilt = kTilts[juce::jlimit (0, 4, setting (AnaTilt))];
    const bool freeze = setting (AnaFreeze) != 0;

    auto upd = [&] (SpectrumAnalyser& a, const ScopeTap& t, juce::int64& last)
    {
        const auto c = t.count();
        const bool fresh = c != last; last = c;
        a.update (t, fresh, fs, rel, tilt, freeze);
        return fresh;
    };
    const bool preFresh = upd (pre, proc.preTap, lastPre);
    upd (post, proc.postTap, lastPost);
    const bool scFresh = proc.sidechainActive.load() && upd (sc, proc.scTap, lastSc);

    if (capCur && preFresh) curAvg.add (pre.rawMagnitudes(), fs);
    if (capRef)
    {
        if (matchPanel->useSidechain()) { if (scFresh) refAvg.add (sc.rawMagnitudes(), fs); }
        else if (preFresh) refAvg.add (pre.rawMagnitudes(), fs);
    }

    rebuildCurves();
    if (! selected.isEmpty())
        for (int i = selected.size(); --i >= 0;)
            if (! bands[(size_t) selected[i]].used) selected.remove (i);
    if (focusBand >= 0 && ! bands[(size_t) focusBand].used) changeFocus (selected.isEmpty() ? -1 : selected.getLast());
    repaint();
}

// ---- painting
void EqDisplay::drawSpectrum (juce::Graphics& g, const SpectrumAnalyser& a, juce::Colour c, bool fill, bool peak)
{
    const auto gr = graph();
    const double fs = proc.getSr();
    juce::Path p;
    const int step = 2;
    bool started = false;
    for (int x = (int) gr.getX(); x <= (int) gr.getRight(); x += step)
    {
        const double f0 = xToFreq ((float) x), f1 = xToFreq ((float) (x + step));
        if (f0 > fs * 0.5) break;
        const float y = juce::jlimit (gr.getY(), gr.getBottom() + 2.f, specToY (a.at (f0, f1, fs, peak)));
        if (! started) { p.startNewSubPath ((float) x, y); started = true; } else p.lineTo ((float) x, y);
    }
    if (! started) return;
    if (fill)
    {
        juce::Path f (p);
        f.lineTo (p.getCurrentPosition().x, gr.getBottom()); f.lineTo (gr.getX(), gr.getBottom()); f.closeSubPath();
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.30f), 0.f, gr.getY(), c.withAlpha (0.04f), 0.f, gr.getBottom(), false));
        g.fillPath (f);
    }
    g.setColour (c.withAlpha (fill ? 0.55f : 0.5f));
    g.strokePath (p, juce::PathStrokeType (peak ? 1.f : 1.2f));
}

void EqDisplay::drawPiano (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xffd9dde4));
    g.fillRect (r);
    for (int m = 12; m <= 127; ++m)
    {
        const double f = 440.0 * std::pow (2.0, (m - 69) / 12.0);
        const float x0 = freqToX (f * std::pow (2.0, -1.0 / 24.0)), x1 = freqToX (f * std::pow (2.0, 1.0 / 24.0));
        if (x1 < r.getX() || x0 > r.getRight()) continue;
        const int pc = m % 12;
        const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        if (black) { g.setColour (juce::Colour (0xff1b1e24)); g.fillRect (x0, r.getY(), x1 - x0, r.getHeight() * 0.62f); }
        if (pc == 0)
        {
            g.setColour (juce::Colour (0xff6b7280)); g.drawVerticalLine ((int) x0, r.getY(), r.getBottom());
            if (x1 - x0 > 3.f) { g.setFont (juce::FontOptions (9.f)); g.drawText ("C" + juce::String (m / 12 - 1), (int) x0 + 1, (int) r.getBottom() - 10, 22, 10, juce::Justification::left); }
        }
    }
    // band markers on the keys
    for (int i = 0; i < kMaxBands; ++i)
        if (bands[(size_t) i].used)
        {
            const float x = freqToX (bands[(size_t) i].freq);
            g.setColour (bandColour (i)); g.fillRect (x - 1.f, r.getY(), 2.f, r.getHeight());
        }
}

void EqDisplay::paint (juce::Graphics& g)
{
    const auto gr = graph();
    const double R = rangeDb()[(size_t) setting (Range)];
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141821), 0.f, 0.f, juce::Colour (0xff0b0d12), 0.f, (float) getHeight(), false));
    g.fillAll();

    // grid
    g.setFont (juce::FontOptions (10.5f));
    const double fl[] { 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000 };
    for (double f : fl)
    {
        const float x = freqToX (f);
        const bool major = f == 20 || f == 50 || f == 100 || f == 200 || f == 500 || f == 1000 || f == 2000 || f == 5000 || f == 10000 || f == 20000;
        g.setColour (juce::Colours::white.withAlpha (major ? 0.08f : 0.035f));
        g.drawVerticalLine ((int) x, gr.getY(), gr.getBottom());
        if (major)
        {
            g.setColour (Theme::dim);
            g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x - 20, (int) gr.getBottom() + 2, 40, 14, juce::Justification::centred);
        }
    }
    const double step = R <= 3 ? 1 : R <= 6 ? 2 : R <= 12 ? 3 : 6;
    for (double d = -R; d <= R + 1e-6; d += step)
    {
        const float y = dbToY (d);
        g.setColour (juce::Colours::white.withAlpha (d == 0 ? 0.16f : 0.05f));
        g.drawHorizontalLine ((int) y, gr.getX(), gr.getRight());
        g.setColour (d == 0 ? Theme::text.withAlpha (0.8f) : kCurve.withAlpha (0.6f));
        const int ly = juce::jlimit (0, getHeight() - 14, (int) y - 7);
        g.drawText ((d > 0 ? "+" : "") + juce::String ((int) d), (int) gr.getRight() + 2, ly, 34, 14, juce::Justification::centredLeft);
    }
    // spectrum scale on the left (small, dim)
    g.setColour (Theme::dim.withAlpha (0.5f));
    g.setFont (juce::FontOptions (9.5f));
    const float AR = kAnaRanges[setting (AnaRangeDb)];
    for (float d = -20.f; d > -AR; d -= 20.f) g.drawText (juce::String ((int) d), (int) gr.getX() + 3, (int) specToY (d) - 6, 30, 12, juce::Justification::left);

    // spectrum
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (gr.toNearestInt());
        const int am = setting (AnaMode);
        const bool freeze = setting (AnaFreeze) != 0;
        if (am == 1 || am == 3) drawSpectrum (g, pre, juce::Colour (0xff9aa3b5), am == 1, false);
        if (am == 2 || am == 3) drawSpectrum (g, post, Theme::accent, true, false);
        if (freeze && am != 0) drawSpectrum (g, am == 1 ? pre : post, juce::Colours::white, false, true);
        if (setting (AnaSidechain) && proc.sidechainActive.load()) drawSpectrum (g, sc, Theme::warm, false, false);

        // EQ Match: show the captured difference as a dashed guide
        if (matchPanel->isVisible() && refAvg.valid() && curAvg.valid())
        {
            const auto r = refAvg.db(), c = curAvg.db();
            juce::Path p;
            for (int i = 0; i < match::kGrid; ++i)
            {
                const float x = freqToX (match::gridFreq (i)), y = dbToY (juce::jlimit (-R, R, r[(size_t) i] - c[(size_t) i]));
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            juce::Path dashed; const float dl[] { 5.f, 4.f };
            juce::PathStrokeType (1.5f).createDashedStroke (dashed, p, dl, 2);
            g.setColour (Theme::accent2.withAlpha (0.8f)); g.fillPath (dashed);
        }

        // individual band curves, selected band filled
        const float y0 = dbToY (0.0);
        for (int i = 0; i < kMaxBands; ++i)
        {
            const auto& v = bandDb[(size_t) i];
            if (v.empty() || cols < 2) continue;
            juce::Path p;
            for (int k = 0; k < cols; ++k)
            {
                const float x = gr.getX() + gr.getWidth() * (float) k / (float) (cols - 1);
                const float y = juce::jlimit (gr.getY() - 4.f, gr.getBottom() + 4.f, dbToY (v[(size_t) k]));
                if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            const auto c = bands[(size_t) i].on ? bandColour (i) : juce::Colour (0xff6b7280);
            if (selected.contains (i) || i == hover)
            {
                juce::Path f (p);
                f.lineTo (gr.getRight(), y0); f.lineTo (gr.getX(), y0); f.closeSubPath();
                g.setColour (c.withAlpha (i == focusBand ? 0.20f : 0.12f)); g.fillPath (f);
            }
            g.setColour (c.withAlpha (selected.contains (i) ? 0.75f : 0.30f));
            g.strokePath (p, juce::PathStrokeType (1.f));
        }

        // composite curve(s)
        auto drawCurve = [&] (const std::vector<float>& a, const std::vector<float>* b, juce::Colour c, float w)
        {
            if (cols < 2) return;
            juce::Path p;
            for (int k = 0; k < cols; ++k)
            {
                const float x = gr.getX() + gr.getWidth() * (float) k / (float) (cols - 1);
                const float d = a[(size_t) k] + (b != nullptr ? (*b)[(size_t) k] : 0.f);
                const float y = juce::jlimit (gr.getY() - 4.f, gr.getBottom() + 4.f, dbToY (d));
                if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (c.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (w + 3.f));
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (w));
        };
        const bool lr = groupUsed[1] || groupUsed[2], ms = groupUsed[3] || groupUsed[4];
        if (! lr && ! ms) drawCurve (groupDb[0], nullptr, kCurve, 2.f);
        if (lr) { drawCurve (groupDb[0], &groupDb[1], kCurveL, 1.8f); drawCurve (groupDb[0], &groupDb[2], kCurveR, 1.8f); }
        if (ms) { drawCurve (groupDb[0], &groupDb[3], kCurveM, 1.8f); drawCurve (groupDb[0], &groupDb[4], kCurveS, 1.8f); }
        if (lr || ms)
        {
            g.setFont (juce::FontOptions (11.f, juce::Font::bold));
            int lx = (int) gr.getRight() - 150;
            auto tag = [&] (const char* t, juce::Colour c) { g.setColour (c); g.drawText (t, lx, (int) gr.getY() + 6, 34, 14, juce::Justification::centred); lx += 36; };
            if (lr) { tag ("L", kCurveL); tag ("R", kCurveR); }
            if (ms) { tag ("MID", kCurveM); tag ("SIDE", kCurveS); }
        }

        // spectrum grab marker
        if (grabValid && hover < 0 && ! dragging)
        {
            const float x = freqToX (grabFreq), y = specToY (grabDb);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawEllipse (x - 5.f, y - 5.f, 10.f, 10.f, 1.5f);
            g.setFont (juce::FontOptions (11.f));
            g.drawText (freqString (grabFreq) + "  " + noteName (grabFreq), (int) x - 60, (int) y - 22, 120, 14, juce::Justification::centred);
        }
    }

    // handles
    for (int i = 0; i < kMaxBands; ++i)
    {
        const auto& b = bands[(size_t) i];
        if (! b.used) continue;
        const auto p = handlePos (i, b);
        const bool sel = selected.contains (i), hv = i == hover;
        const auto c = b.on ? bandColour (i) : juce::Colour (0xff6b7280);
        const float r = kHandle + (hv ? 1.5f : 0.f);
        if (sel) { g.setColour (c.withAlpha (0.25f)); g.fillEllipse (p.x - r - 4.f, p.y - r - 4.f, 2.f * r + 8.f, 2.f * r + 8.f); }
        g.setColour (sel ? c : c.withAlpha (0.85f).darker (0.2f));
        g.fillEllipse (p.x - r, p.y - r, 2.f * r, 2.f * r);
        g.setColour (sel ? juce::Colours::white : juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (p.x - r, p.y - r, 2.f * r, 2.f * r, sel ? 1.6f : 1.f);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.setFont (juce::FontOptions (10.f, juce::Font::bold));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (p.x - r, p.y - r, 2.f * r, 2.f * r), juce::Justification::centred);
        if (b.place != Place::Stereo)
        {
            g.setColour (c); g.setFont (juce::FontOptions (9.f, juce::Font::bold));
            g.drawText (juce::String (placeNames()[(int) b.place]).substring (0, 1), (int) p.x - 8, (int) (p.y + r + 1.f), 16, 10, juce::Justification::centred);
        }
        if (proc.soloBand.load() == i)
        {
            g.setColour (Theme::warm); g.drawEllipse (p.x - r - 3.f, p.y - r - 3.f, 2.f * r + 6.f, 2.f * r + 6.f, 1.5f);
        }
    }

    // readout for the hovered / dragged band
    const int info = dragging && focusBand >= 0 ? focusBand : hover;
    if (info >= 0 && bands[(size_t) info].used)
    {
        const auto& b = bands[(size_t) info];
        const auto p = handlePos (info, b);
        juce::String t = freqString (b.freq) + "   " + noteName (b.freq);
        juce::String t2 = juce::String (typeNames()[(int) b.type]);
        if (typeHasGain (b.type)) t2 << "   " << (b.gain > 0 ? "+" : "") << juce::String (b.gain, 1) << " dB";
        t2 << "   Q " << juce::String (b.q, 2);
        if (typeHasSlope (b.type)) t2 << "   " << slopeDb()[b.slope] << " dB/oct";
        const float w = 190.f, h = 36.f;
        float bx = juce::jlimit (gr.getX() + 4.f, gr.getRight() - w - 4.f, p.x - w * 0.5f);
        float by = p.y - h - 16.f; if (by < gr.getY() + 2.f) by = p.y + 16.f;
        g.setColour (juce::Colour (0xee10131a)); g.fillRoundedRectangle (bx, by, w, h, 6.f);
        g.setColour (bandColour (info).withAlpha (0.8f)); g.drawRoundedRectangle (bx, by, w, h, 6.f, 1.f);
        g.setColour (Theme::text); g.setFont (juce::FontOptions (12.f, juce::Font::bold));
        g.drawText (t, (int) bx + 8, (int) by + 3, (int) w - 16, 15, juce::Justification::centredLeft);
        g.setColour (Theme::dim); g.setFont (juce::FontOptions (11.f));
        g.drawText (t2, (int) bx + 8, (int) by + 18, (int) w - 16, 15, juce::Justification::centredLeft);
    }

    if (rubber)
    {
        g.setColour (Theme::accent.withAlpha (0.12f)); g.fillRect (rubberRect);
        g.setColour (Theme::accent.withAlpha (0.6f)); g.drawRect (rubberRect, 1.f);
    }

    if (setting (Piano)) drawPiano (g, { gr.getX(), gr.getBottom() + 17.f, gr.getWidth(), 16.f });

    bool any = false;
    for (const auto& b : bands) any |= b.used;
    if (! any)
    {
        g.setColour (Theme::dim.withAlpha (0.8f)); g.setFont (juce::FontOptions (13.f));
        g.drawText ("Double-click to add a band, or grab a peak in the spectrum and drag it", gr.toNearestInt().removeFromBottom (60), juce::Justification::centred);
    }
}

// ---- interaction
void EqDisplay::changeFocus (int b)
{
    if (b == focusBand) return;
    focusBand = b;
    if (onFocusChanged) onFocusChanged (b);
}

void EqDisplay::setFocus (int b)
{
    selected.clear();
    if (b >= 0) selected.add (b);
    changeFocus (b);
    repaint();
}

void EqDisplay::selectAll()
{
    selected.clear();
    for (int i = 0; i < kMaxBands; ++i) if (bands[(size_t) i].used) selected.add (i);
    if (! selected.isEmpty() && ! selected.contains (focusBand)) changeFocus (selected.getFirst());
    repaint();
}

void EqDisplay::deleteBands (juce::Array<int> list)
{
    proc.beginEdit();
    for (int i : list) proc.clearBand (i);
    selected.clear();
    changeFocus (-1);
    key.clear();
}

void EqDisplay::updateGrab (juce::Point<float> p)
{
    grabValid = false;
    if (! setting (Grab) || setting (AnaMode) == 0) return;
    const auto& a = setting (AnaMode) == 1 ? pre : post;
    const double fs = proc.getSr();
    float best = -200.f; float bx = 0.f;
    for (float x = p.x - 30.f; x <= p.x + 30.f; x += 1.f)
    {
        const double f = xToFreq (x);
        if (f < 15.0 || f > fs * 0.48) continue;
        const float v = a.at (f, xToFreq (x + 1.f), fs);
        if (v > best) { best = v; bx = x; }
    }
    if (best < -100.f) return;
    const float y = specToY (best);
    if (std::abs (y - p.y) > 40.f) return;
    grabValid = true; grabFreq = xToFreq (bx); grabDb = best;
}

void EqDisplay::mouseMove (const juce::MouseEvent& e)
{
    mousePos = e.position;
    const int h = hitBand (e.position);
    if (h != hover) { hover = h; repaint(); }
    if (h < 0) updateGrab (e.position); else grabValid = false;
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : grabValid ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void EqDisplay::mouseExit (const juce::MouseEvent&) { hover = -1; grabValid = false; repaint(); }

void EqDisplay::addBandAt (juce::Point<float> p, Type t, bool startDrag)
{
    const int i = proc.firstFreeBand();
    if (i < 0) return;
    BandParams b; b.used = true; b.type = t;
    b.freq = juce::jlimit (10.0, 30000.0, xToFreq (p.x));
    b.q = (t == Type::LowCut || t == Type::HighCut || typeHasGain (t)) ? (t == Type::Bell ? 1.0 : 0.71) : (t == Type::Notch ? 4.0 : 1.0);
    const double gs = proc.pv ("gainScale");
    b.gain = typeHasGain (t) ? juce::jlimit (-30.0, 30.0, yToDb (p.y) / (std::abs (gs) > 0.01 ? gs : 1.0)) : 0.0;
    b.slope = (t == Type::LowCut || t == Type::HighCut) ? 3 : 1;
    proc.beginEdit();
    proc.writeBand (i, b);
    bands[(size_t) i] = b;
    setFocus (i);
    if (startDrag)
    {
        dragging = true;
        dragStart = { { i, b.freq, b.gain, b.q } };
        downPos = p;
    }
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    downPos = e.position;
    const int h = hitBand (e.position);
    rubber = false; dragging = false;

    if (h >= 0)
    {
        if (e.mods.isPopupMenu())
        {
            if (! selected.contains (h)) setFocus (h);
            bandMenu (h);
            return;
        }
        if (e.mods.isAltDown())
        {
            proc.beginEdit();
            proc.setParam (pid::band (h, "on"), bands[(size_t) h].on ? 0.f : 1.f);
            return;
        }
        if (e.mods.isCommandDown() || e.mods.isShiftDown())
        {
            if (selected.contains (h)) selected.removeFirstMatchingValue (h); else selected.add (h);
            changeFocus (selected.contains (h) ? h : (selected.isEmpty() ? -1 : selected.getLast()));
            repaint();
            return;
        }
        if (! selected.contains (h)) setFocus (h); else changeFocus (h);
        proc.beginEdit();
        dragging = true;
        dragStart.clear();
        for (int s : selected) dragStart.push_back ({ s, bands[(size_t) s].freq, bands[(size_t) s].gain, bands[(size_t) s].q });
        return;
    }

    if (e.mods.isPopupMenu()) { emptyMenu (e.position); return; }

    if (grabValid && graph().contains (e.position))
    {
        const auto at = juce::Point<float> (freqToX (grabFreq), dbToY (curveDbAt (grabFreq)));
        dragStart.clear();
        addBandAt (at, Type::Bell, true);
        if (dragging && ! dragStart.empty())
        {
            proc.setParam (pid::band (dragStart[0].band, "q"), 4.f);         // grabbed peaks start narrow
            dragStart[0].q = 4.0;
        }
        downPos = e.position;
        grabValid = false;
        return;
    }

    if (! (e.mods.isCommandDown() || e.mods.isShiftDown())) setFocus (-1);
    rubber = true;
    rubberRect = { e.position, e.position };
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    mousePos = e.position;
    if (rubber)
    {
        rubberRect = juce::Rectangle<float> (downPos, e.position);
        for (int i = 0; i < kMaxBands; ++i)
            if (bands[(size_t) i].used && rubberRect.contains (handlePos (i, bands[(size_t) i])))
            {
                if (! selected.contains (i)) { selected.add (i); changeFocus (i); }
            }
        repaint();
        return;
    }
    if (! dragging || dragStart.empty()) return;

    const float fine = e.mods.isShiftDown() ? 0.15f : 1.f;
    float dx = (e.position.x - downPos.x) * fine, dy = (e.position.y - downPos.y) * fine;
    if (e.mods.isCommandDown()) { if (std::abs (dx) > std::abs (dy)) dy = 0.f; else dx = 0.f; }
    const auto g = graph();
    const double octPerPx = std::log2 (kFMax / kFMin) / g.getWidth();
    const double R = rangeDb()[(size_t) setting (Range)];
    const double dbPerPx = 2.0 * R / g.getHeight();
    const double gs = proc.pv ("gainScale");

    for (const auto& s : dragStart)
    {
        const auto& b = bands[(size_t) s.band];
        const double f = juce::jlimit (10.0, 30000.0, s.freq * std::pow (2.0, dx * octPerPx));
        proc.setParam (pid::band (s.band, "freq"), (float) f);
        if (typeHasGain (b.type))
            proc.setParam (pid::band (s.band, "gain"), (float) juce::jlimit (-30.0, 30.0, s.gain - dy * dbPerPx / (std::abs (gs) > 0.01 ? std::abs (gs) : 1.0)));
        else if (b.type == Type::LowCut || b.type == Type::HighCut)
            proc.setParam (pid::band (s.band, "q"), (float) juce::jlimit (0.1, 40.0, s.q * std::pow (10.0, -dy * dbPerPx / 20.0)));
        else
            proc.setParam (pid::band (s.band, "q"), (float) juce::jlimit (0.025, 40.0, s.q * std::pow (2.0, dy / 60.0)));
        bands[(size_t) s.band].freq = f;
    }
    repaint();
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    if (rubber) { rubber = false; repaint(); }
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitBand (e.position);
    if (h >= 0) { deleteBands ({ h }); return; }
    if (! graph().contains (e.position)) return;
    const auto g = graph();
    const float rel = (e.position.x - g.getX()) / g.getWidth();
    addBandAt (e.position, rel < 0.06f ? Type::LowCut : rel > 0.95f ? Type::HighCut : Type::Bell, false);
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    juce::Array<int> targets;
    const int h = hitBand (e.position);
    if (h >= 0 && ! selected.contains (h)) targets.add (h); else targets = selected;
    if (targets.isEmpty()) return;
    proc.beginEdit();
    const float d = (w.deltaY != 0.f ? w.deltaY : w.deltaX) * (w.isReversed ? -1.f : 1.f);
    for (int i : targets)
    {
        const auto& b = bands[(size_t) i];
        if (e.mods.isShiftDown() && typeHasGain (b.type))
            proc.setParam (pid::band (i, "gain"), (float) juce::jlimit (-30.0, 30.0, b.gain + d * 3.0));
        else
            proc.setParam (pid::band (i, "q"), (float) juce::jlimit (0.025, 40.0, b.q * std::pow (2.0, d * 1.5)));
    }
}

bool EqDisplay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { if (! selected.isEmpty()) deleteBands (selected); return true; }
    if (k == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0)) { selectAll(); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.undo.undo(); return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || k == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0)) { proc.undo.redo(); return true; }
    if (k == juce::KeyPress::escapeKey) { setFocus (-1); return true; }
    return false;
}

void EqDisplay::bandMenu (int band)
{
    juce::Array<int> targets = selected;
    if (! targets.contains (band)) targets = { band };
    const auto& b = bands[(size_t) band];

    juce::PopupMenu m, types, slopes, places;
    for (int i = 0; i < kNumTypes; ++i) types.addItem (100 + i, typeNames()[i], true, (int) b.type == i);
    for (int i = 0; i < kNumSlopes; ++i) slopes.addItem (200 + i, juce::String (slopeDb()[i]) + " dB/oct", true, b.slope == i);
    for (int i = 0; i < kNumPlaces; ++i) places.addItem (300 + i, placeNames()[i], proc.isStereo() || i == 0, (int) b.place == i);
    m.addSectionHeader ("Band " + juce::String (band + 1) + (targets.size() > 1 ? " (+" + juce::String (targets.size() - 1) + " selected)" : juce::String()));
    m.addSubMenu ("Shape", types);
    m.addSubMenu ("Slope", slopes, typeHasSlope (b.type));
    m.addSubMenu ("Stereo Placement", places);
    m.addSeparator();
    m.addItem (1, "Enabled", true, b.on);
    m.addItem (2, "Solo (audition)", true, proc.soloBand.load() == band);
    m.addItem (3, "Reset gain to 0 dB", typeHasGain (b.type));
    m.addItem (4, "Invert gain", typeHasGain (b.type));
    m.addItem (5, "Reset Q");
    m.addSeparator();
    m.addItem (6, targets.size() > 1 ? "Delete selected bands" : "Delete band");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, targets, band] (int r)
    {
        if (r == 0) return;
        proc.beginEdit();
        for (int i : targets)
        {
            const auto& bb = bands[(size_t) i];
            if (r >= 100 && r < 100 + kNumTypes) proc.setParam (pid::band (i, "type"), (float) (r - 100));
            else if (r >= 200 && r < 200 + kNumSlopes) proc.setParam (pid::band (i, "slope"), (float) (r - 200));
            else if (r >= 300 && r < 300 + kNumPlaces) proc.setParam (pid::band (i, "place"), (float) (r - 300));
            else if (r == 1) proc.setParam (pid::band (i, "on"), bands[(size_t) band].on ? 0.f : 1.f);
            else if (r == 3) proc.setParam (pid::band (i, "gain"), 0.f);
            else if (r == 4) proc.setParam (pid::band (i, "gain"), (float) -bb.gain);
            else if (r == 5) proc.setParam (pid::band (i, "q"), (bb.type == Type::LowCut || bb.type == Type::HighCut) ? 0.71f : 1.f);
        }
        if (r == 2) proc.soloBand = proc.soloBand.load() == band ? -1 : band;
        if (r == 6) deleteBands (targets);
        if (onFocusChanged) onFocusChanged (focusBand);
    });
}

void EqDisplay::emptyMenu (juce::Point<float> p)
{
    juce::PopupMenu m, add;
    for (int i = 0; i < kNumTypes; ++i) add.addItem (100 + i, typeNames()[i]);
    m.addSubMenu ("Add band here", add, proc.firstFreeBand() >= 0);
    m.addItem (1, "Select all bands");
    m.addItem (2, "Invert all gains");
    m.addItem (3, "Swap Left/Right and Mid/Side placements");
    m.addSeparator();
    m.addItem (4, "Delete all bands");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, p] (int r)
    {
        if (r >= 100) { addBandAt (p, (Type) (r - 100), false); return; }
        if (r == 1) selectAll();
        if (r == 2 || r == 3)
        {
            proc.beginEdit();
            for (int i = 0; i < kMaxBands; ++i)
            {
                const auto& b = bands[(size_t) i];
                if (! b.used) continue;
                if (r == 2) proc.setParam (pid::band (i, "gain"), (float) -b.gain);
                else
                {
                    const int sw[] { 0, 2, 1, 4, 3 };
                    proc.setParam (pid::band (i, "place"), (float) sw[(int) b.place]);
                }
            }
        }
        if (r == 4) { juce::Array<int> all; for (int i = 0; i < kMaxBands; ++i) all.add (i); deleteBands (all); }
    });
}

void EqDisplay::showAnalyserMenu (juce::Component* target)
{
    juce::PopupMenu m, mode, res, speed, tilt, range, disp;
    const char* modes[] { "Off", "Pre-EQ", "Post-EQ", "Pre + Post" };
    for (int i = 0; i < 4; ++i) mode.addItem (100 + i, modes[i], true, setting (AnaMode) == i);
    const char* ress[] { "Low", "Medium", "High", "Maximum" };
    for (int i = 0; i < 4; ++i) res.addItem (200 + i, ress[i], true, setting (AnaRes) == i);
    const char* speeds[] { "Very Slow", "Slow", "Medium", "Fast", "Very Fast" };
    for (int i = 0; i < 5; ++i) speed.addItem (300 + i, speeds[i], true, setting (AnaSpeed) == i);
    for (int i = 0; i < 5; ++i) tilt.addItem (400 + i, juce::String (kTilts[i], 1) + " dB/oct", true, setting (AnaTilt) == i);
    for (int i = 0; i < 3; ++i) range.addItem (500 + i, juce::String ((int) kAnaRanges[i]) + " dB", true, setting (AnaRangeDb) == i);
    m.addSubMenu ("Analyzer", mode);
    m.addItem (1, "Show sidechain spectrum", true, setting (AnaSidechain) != 0);
    m.addItem (2, "Freeze (peak hold)", true, setting (AnaFreeze) != 0);
    m.addItem (3, "Spectrum Grab", true, setting (Grab) != 0);
    m.addSubMenu ("Resolution", res);
    m.addSubMenu ("Speed", speed);
    m.addSubMenu ("Tilt", tilt);
    m.addSubMenu ("Range", range);
    m.addSeparator();
    m.addItem (4, "Piano roll", true, setting (Piano) != 0);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [this] (int r)
    {
        if (r >= 500) setSetting (AnaRangeDb, r - 500);
        else if (r >= 400) setSetting (AnaTilt, r - 400);
        else if (r >= 300) setSetting (AnaSpeed, r - 300);
        else if (r >= 200) setSetting (AnaRes, r - 200);
        else if (r >= 100) setSetting (AnaMode, r - 100);
        else if (r == 1) setSetting (AnaSidechain, setting (AnaSidechain) ? 0 : 1);
        else if (r == 2) setSetting (AnaFreeze, setting (AnaFreeze) ? 0 : 1);
        else if (r == 3) setSetting (Grab, setting (Grab) ? 0 : 1);
        else if (r == 4) setSetting (Piano, setting (Piano) ? 0 : 1);
    });
}
