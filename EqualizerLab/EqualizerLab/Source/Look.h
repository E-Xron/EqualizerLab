#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace Theme { constexpr float kPi = 3.14159265358979f; }

namespace Theme
{
    const juce::Colour bg      { 0xff0b0d12 };
    const juce::Colour panel   { 0xff12151c };
    const juce::Colour panel2  { 0xff1b1f28 };
    const juce::Colour border  { 0xff262b36 };
    const juce::Colour text    { 0xffeef1f6 };
    const juce::Colour dim     { 0xff8a93a3 };
    const juce::Colour accent  { 0xff6c8cff };
    const juce::Colour accent2 { 0xff38d6c4 };
    const juce::Colour warm    { 0xffffa24d };
    const juce::Colour red     { 0xffff4f5e };
}

class ModernLAF : public juce::LookAndFeel_V4
{
public:
    ModernLAF()
    {
        using namespace Theme;
        setColour (juce::ResizableWindow::backgroundColourId, panel);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::backgroundColourId, panel2);
        setColour (juce::PopupMenu::backgroundColourId, panel2);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, accent);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (13.f, juce::Font::bold)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.f, juce::Font::bold)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.f)); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hl, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const float rad = 7.f;
        const bool cl = b.isConnectedOnLeft(), cr = b.isConnectedOnRight();
        juce::Path shape;
        shape.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), rad, rad, ! cl, ! cr, ! cl, ! cr);

        if (b.getProperties()["record"])                          // record: dark pill with a red lamp
        {
            g.setColour (on ? Theme::red.withAlpha (0.18f) : Theme::panel2.brighter (hl ? 0.08f : 0.f));
            g.fillPath (shape);
            g.setColour (on ? Theme::red.withAlpha (0.7f) : Theme::border);
            g.strokePath (shape, juce::PathStrokeType (1.f));
            const float d = 10.f, cx = r.getX() + 16.f, cy = r.getCentreY();
            if (on)
                for (int i = 3; i >= 1; --i)
                {
                    g.setColour (Theme::red.withAlpha (0.12f * (float) i));
                    g.fillEllipse (cx - d * 0.5f - (float) (4 - i) * 2.f, cy - d * 0.5f - (float) (4 - i) * 2.f, d + (float) (4 - i) * 4.f, d + (float) (4 - i) * 4.f);
                }
            g.setColour (on ? Theme::red : Theme::red.withAlpha (0.65f));
            g.fillEllipse (cx - d * 0.5f, cy - d * 0.5f, d, d);
            return;
        }

        juce::Colour top = on ? b.findColour (juce::TextButton::buttonOnColourId) : Theme::panel2;
        if (hl)   top = top.brighter (0.10f);
        if (down) top = top.darker (0.15f);
        g.setGradientFill (juce::ColourGradient (top.brighter (0.06f), 0.f, r.getY(), top.darker (0.08f), 0.f, r.getBottom(), false));
        g.fillPath (shape);
        g.setColour (on ? top.brighter (0.35f).withAlpha (0.8f) : Theme::border);
        g.strokePath (shape, juce::PathStrokeType (1.f));
        g.setColour (juce::Colours::white.withAlpha (on ? 0.18f : 0.05f));      // top sheen
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + (cl ? 0.f : rad), r.getRight() - (cr ? 0.f : rad));
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds();
        if (b.getProperties()["record"]) r.removeFromLeft (22);
        g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                      .withMultipliedAlpha (b.isEnabled() ? 1.f : 0.5f));
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawText (b.getButtonText(), r, juce::Justification::centred);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
        g.setGradientFill (juce::ColourGradient (Theme::panel2.brighter (0.05f), 0.f, 0.f, Theme::panel2.darker (0.08f), 0.f, (float) h, false));
        g.fillRoundedRectangle (r, 7.f);
        g.setColour (Theme::border);
        g.drawRoundedRectangle (r, 7.f, 1.f);
        juce::Path p;
        const float cx = (float) w - 14.f, cy = (float) h * 0.5f;
        p.addTriangle (cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy + 3.f);
        g.setColour (Theme::dim);
        g.fillPath (p);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider&) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.f);
        const float rad = std::min (b.getWidth(), b.getHeight()) * 0.5f, lw = 6.f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0), r2 = rad - lw * 0.5f;

        juce::Path track, val;
        track.addCentredArc (c.x, c.y, r2, r2, 0.f, a0, a1, true);
        val.addCentredArc   (c.x, c.y, r2, r2, 0.f, a0, ang, true);
        g.setColour (Theme::border);
        g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Theme::accent);
        g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white);
        g.fillEllipse (c.x + r2 * std::sin (ang) - 4.f, c.y - r2 * std::cos (ang) - 4.f, 8.f, 8.f);
    }
};

// Machined aluminium knob with a knurled skirt and a value ring in the slider's colour (rotarySliderFillColourId),
// illuminated power buttons (property "power") and slide switches for toggles.
class KnobLAF : public ModernLAF
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hl, bool down) override;
};

inline void KnobLAF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const juce::Colour c = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f - 2.f;
    const auto ctr = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);
    const bool enabled = s.isEnabled();

    // printed scale
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / 10.f;
        const float r0 = r - 1.f, r1 = r + (i % 5 == 0 ? 2.5f : 1.f);
        g.setColour (juce::Colours::white.withAlpha (i % 5 == 0 ? 0.35f : 0.18f));
        g.drawLine (ctr.x + r0 * std::sin (a), ctr.y - r0 * std::cos (a), ctr.x + r1 * std::sin (a), ctr.y - r1 * std::cos (a), 1.f);
    }

    // value ring
    const float rr = r - 5.f;
    juce::Path track, val;
    track.addCentredArc (ctr.x, ctr.y, rr, rr, 0.f, a0, a1, true);
    val.addCentredArc (ctr.x, ctr.y, rr, rr, 0.f, a0, ang, true);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (track, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (enabled)
    {
        g.setColour (c.withAlpha (0.22f));
        g.strokePath (val, juce::PathStrokeType (7.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (enabled ? c : c.withSaturation (0.1f).withAlpha (0.5f));
    g.strokePath (val, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body: drop shadow, knurled skirt, brushed cap
    const float kr = rr - 6.f;
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillEllipse (ctr.x - kr - (float) i * 0.5f, ctr.y - kr + (float) i * 0.9f, 2.f * kr + (float) i, 2.f * kr + (float) i);
    }
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3f49), ctr.x, ctr.y - kr, juce::Colour (0xff121419), ctr.x, ctr.y + kr, false));
    g.fillEllipse (ctr.x - kr, ctr.y - kr, 2.f * kr, 2.f * kr);
    const int grips = 40;
    for (int i = 0; i < grips; ++i)
    {
        const float a = ang + 2.f * Theme::kPi * (float) i / (float) grips;
        const float g0 = kr - 2.6f, g1 = kr - 0.4f;
        g.setColour (i % 2 == 0 ? juce::Colours::white.withAlpha (0.10f) : juce::Colours::black.withAlpha (0.35f));
        g.drawLine (ctr.x + g0 * std::sin (a), ctr.y - g0 * std::cos (a), ctr.x + g1 * std::sin (a), ctr.y - g1 * std::cos (a), 1.f);
    }
    const float cr = kr * 0.74f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe6e9ee), ctr.x - cr * 0.6f, ctr.y - cr * 0.8f,
                                             juce::Colour (0xff6d7480), ctr.x + cr * 0.7f, ctr.y + cr * 0.9f, true));
    g.fillEllipse (ctr.x - cr, ctr.y - cr, 2.f * cr, 2.f * cr);
    for (int i = 1; i <= 5; ++i)                                   // brushed rings
    {
        const float q = cr * (float) i / 5.5f;
        g.setColour (juce::Colours::white.withAlpha (i % 2 ? 0.07f : 0.0f));
        g.drawEllipse (ctr.x - q, ctr.y - q, 2.f * q, 2.f * q, 0.6f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.55f));          // top rim highlight
    juce::Path rim;
    rim.addCentredArc (ctr.x, ctr.y, cr - 0.5f, cr - 0.5f, 0.f, -2.2f, -0.4f, true);
    g.strokePath (rim, juce::PathStrokeType (1.f));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (ctr.x - cr, ctr.y - cr, 2.f * cr, 2.f * cr, 0.8f);

    // pointer
    const float p0 = cr * 0.25f, p1 = cr * 0.92f;
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (ctr.x + p0 * std::sin (ang), ctr.y - p0 * std::cos (ang) + 0.8f, ctr.x + p1 * std::sin (ang), ctr.y - p1 * std::cos (ang) + 0.8f, 3.2f);
    g.setColour (enabled ? c.brighter (0.2f) : juce::Colours::grey);
    g.drawLine (ctr.x + p0 * std::sin (ang), ctr.y - p0 * std::cos (ang), ctr.x + p1 * std::sin (ang), ctr.y - p1 * std::cos (ang), 2.2f);
}

inline juce::Label* KnobLAF::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (11.5f));
    l->setColour (juce::Label::textColourId, Theme::text.withAlpha (0.9f));
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::TextEditor::backgroundColourId, Theme::panel2);
    l->setColour (juce::TextEditor::textColourId, Theme::text);
    l->setColour (juce::TextEditor::highlightColourId, Theme::accent.withAlpha (0.4f));
    return l;
}

inline void KnobLAF::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool)
{
    const juce::Colour c = b.findColour (juce::ToggleButton::tickColourId);
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds().toFloat();

    if (b.getProperties()["power"])                                // illuminated power button
    {
        const float d = std::min (r.getWidth(), r.getHeight()) - 4.f;
        auto k = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
        if (on)
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (c.withAlpha (0.10f * (float) i));
                g.fillEllipse (k.expanded ((float) (4 - i) * 2.f));
            }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c313a), k.getX(), k.getY(), juce::Colour (0xff111318), k.getRight(), k.getBottom(), false));
        g.fillEllipse (k);
        g.setColour (on ? c : juce::Colour (0xff59606c).withAlpha (hl ? 1.f : 0.8f));
        const auto cc = k.getCentre();
        const float pr = d * 0.27f;
        juce::Path arc;
        arc.addCentredArc (cc.x, cc.y, pr, pr, 0.f, 0.6f, 2.f * Theme::kPi - 0.6f, true);
        g.strokePath (arc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (cc.x, cc.y - pr - 1.5f, cc.x, cc.y - pr * 0.15f, 1.8f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (k, 1.f);
        return;
    }

    // slide switch + label
    auto sw = r.removeFromLeft (30.f).withSizeKeepingCentre (28.f, 15.f);
    g.setColour (on ? c.withAlpha (0.85f) : juce::Colour (0xff2a2f38));
    g.fillRoundedRectangle (sw, 7.5f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (sw, 7.5f, 1.f);
    const float kx = on ? sw.getRight() - 13.f : sw.getX() + 2.f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff1f3f6), kx, sw.getY(), juce::Colour (0xffa8afba), kx, sw.getBottom(), false));
    g.fillEllipse (kx, sw.getY() + 2.f, 11.f, 11.f);
    g.setColour (Theme::text.withAlpha (hl ? 1.f : 0.85f));
    g.setFont (juce::FontOptions (12.f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (6.f), juce::Justification::centredLeft);
}
