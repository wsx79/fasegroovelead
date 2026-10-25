// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Identical vendored theme in Piano, Drums and Guitar. Keep this file in sync.
// Layout and instrument colours remain owned by the individual editors.
#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>

namespace Palette
{
    inline const juce::Colour background { 0xff0d0f13 };   // window
    inline const juce::Colour panel      { 0xff161920 };   // panels
    inline const juce::Colour panelEdge  { 0xff272c35 };   // borders, tracks, empty cells
    inline const juce::Colour control    { 0xff1d2129 };   // buttons
    inline const juce::Colour field      { 0xff0f1216 };   // menus, displays
    inline const juce::Colour accent     { 0xffffa94d };   // main actions; instrument colours keep their own meanings
    inline const juce::Colour accent2    { 0xff4cc3f0 };   // transport and linked controls
    inline const juce::Colour cymbals    { 0xff5fd68a };   // green: picking and licks
    inline const juce::Colour percussion { 0xffb69cff };   // violet: slides and swells
    inline const juce::Colour toms       { 0xffff5a5f };   // red: the low strings (octaves, power chords, root)
    inline const juce::Colour text       { 0xffe7e9ec };
    inline const juce::Colour strings = toms, fill = percussion, level = cymbals;
    inline const juce::Colour linked = accent2.darker (0.3f);
    inline const juce::Colour dim        { 0xff7f8794 };
}

namespace FamilyTheme
{
    inline juce::Typeface::Ptr typeface (bool bold)
    {
        static auto regular = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf, BinaryData::InterRegular_otfSize);
        static auto semiBold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);
        return bold ? semiBold : regular;
    }
    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (typeface (bold)).withHeight (height));
    }
}

class FamilyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FamilyLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isDown, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    juce::Font getPopupMenuFont() override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

private:
    juce::Typeface::Ptr regular, semiBold;
};

inline FamilyLookAndFeel::FamilyLookAndFeel()
{
    regular = FamilyTheme::typeface (false);
    semiBold = FamilyTheme::typeface (true);

    setColour (juce::ResizableWindow::backgroundColourId, Palette::background);
    setColour (juce::TextButton::buttonColourId, Palette::control);
    setColour (juce::TextButton::buttonOnColourId, Palette::accent);
    setColour (juce::TextButton::textColourOffId, Palette::text);
    setColour (juce::TextButton::textColourOnId, Palette::background);
    setColour (juce::ComboBox::backgroundColourId, Palette::field);
    setColour (juce::ComboBox::outlineColourId, Palette::panelEdge);
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::ComboBox::arrowColourId, Palette::accent);
    setColour (juce::ComboBox::focusedOutlineColourId, Palette::accent);
    setColour (juce::PopupMenu::backgroundColourId, Palette::panel);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::PopupMenu::headerTextColourId, Palette::accent);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::accent.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, Palette::background);
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::trackColourId, Palette::accent.withAlpha (0.35f));
    setColour (juce::Slider::backgroundColourId, Palette::field);
    setColour (juce::Label::textColourId, Palette::text);
    setColour (juce::TextEditor::backgroundColourId, Palette::field);
    setColour (juce::TextEditor::textColourId, Palette::text);
    setColour (juce::TextEditor::outlineColourId, Palette::panelEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent2);
    setColour (juce::ListBox::backgroundColourId, Palette::field);
    setColour (juce::ListBox::textColourId, Palette::text);
    setColour (juce::ScrollBar::thumbColourId, Palette::panelEdge.brighter (0.3f));
    setColour (juce::ToggleButton::textColourId, Palette::text);
}

inline juce::Typeface::Ptr FamilyLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    // The default sans-serif becomes Inter (bold styles: Inter SemiBold), the same on every system.
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() && regular != nullptr && semiBold != nullptr)
        return f.isBold() || f.getTypefaceStyle().containsIgnoreCase ("bold") ? semiBold : regular;
    return juce::LookAndFeel_V4::getTypefaceForFont (f);
}

inline void FamilyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& background, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : background;
    if (fill == Palette::background) fill = Palette::control;
    if (! b.isEnabled()) fill = fill.withMultipliedAlpha (0.5f);
    else if (down) fill = fill.darker (0.15f);
    else if (highlighted) fill = fill.brighter (0.08f);
    juce::Path shape;
    shape.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 4.0f, 4.0f,
                               ! b.isConnectedOnLeft(), ! b.isConnectedOnRight(),
                               ! b.isConnectedOnLeft(), ! b.isConnectedOnRight());
    g.setColour (fill);
    g.fillPath (shape);
    g.setColour (b.hasKeyboardFocus (true) ? Palette::accent2
                 : fill.getPerceivedBrightness() > 0.35f ? fill.darker (0.25f)
                 : highlighted ? Palette::panelEdge.brighter (0.35f) : Palette::panelEdge);
    g.strokePath (shape, juce::PathStrokeType (1.0f));
}

inline juce::Font FamilyLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return FamilyTheme::font (juce::jlimit (10.0f, 13.5f, buttonHeight * 0.45f), true);
}

inline void FamilyLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    const auto fillColour = b.findColour (on ? juce::TextButton::buttonOnColourId : juce::TextButton::buttonColourId);
    auto colour = fillColour.getPerceivedBrightness() > 0.35f ? Palette::background : Palette::text;
    g.setColour (colour.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1, 0.75f);
}

inline void FamilyLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? Palette::accent2 : box.isMouseOver (true) ? Palette::panelEdge.brighter (0.4f) : Palette::panelEdge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    const float cx = (float) width - 12.0f, cy = height * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.8f);
    chevron.lineTo (cx, cy + 1.8f);
    chevron.lineTo (cx + 3.5f, cy - 1.8f);
    g.setColour (box.isEnabled() ? Palette::dim : Palette::dim.withAlpha (0.4f));
    g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

inline juce::Font FamilyLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return FamilyTheme::font (juce::jlimit (11.0f, 14.0f, box.getHeight() * 0.46f));
}

inline void FamilyLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (2, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

inline void FamilyLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (Palette::panel);
    g.setColour (Palette::panelEdge);
    g.drawRect (0, 0, width, height, 1);
}

inline juce::Font FamilyLookAndFeel::getPopupMenuFont()
{
    return FamilyTheme::font (14.0f);
}

inline void FamilyLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (Palette::field);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Palette::panelEdge.brighter (0.3f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (Palette::text);
    g.setFont (FamilyTheme::font (13.0f));
    g.drawFittedText (text, r.reduced (10.0f, 6.0f).toNearestInt(), juce::Justification::centredLeft, 8, 1.0f);
}

inline juce::Rectangle<int> FamilyLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const auto f = FamilyTheme::font (13.0f);
    const float width = juce::GlyphArrangement::getStringWidth (f, text);
    const int w = juce::jmin (420, juce::roundToInt (width) + 22);
    const int lines = juce::jmax (1, juce::roundToInt (width / 398.0f + 0.5f));
    const int h = juce::roundToInt ((float) lines * f.getHeight() * 1.2f) + 12;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

inline juce::Font FamilyLookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}

inline void FamilyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    g.beginTransparencyLayer (b.isEnabled() ? 1.0f : 0.45f);
    auto r = b.getLocalBounds().toFloat();
    auto pill = r.removeFromLeft (34.0f).withSizeKeepingCentre (30.0f, 16.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? Palette::accent : Palette::field);
    g.fillRoundedRectangle (pill, 8.0f);
    g.setColour (on ? Palette::accent.darker (0.2f) : (highlighted ? Palette::panelEdge.brighter (0.4f) : Palette::panelEdge));
    g.drawRoundedRectangle (pill.reduced (0.5f), 8.0f, 1.0f);

    const float d = 12.0f;
    const float cx = on ? pill.getRight() - d - 2.0f : pill.getX() + 2.0f;
    g.setColour (on ? Palette::background : Palette::dim);
    g.fillEllipse (cx, pill.getCentreY() - d * 0.5f, d, d);

    g.setColour (Palette::text);
    g.setFont (FamilyTheme::font (13.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
    g.endTransparencyLayer();
}

inline void FamilyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                          float startAngle, float endAngle, juce::Slider&)
{
    // A studio knob: thin track, accent value arc, a dark body with a soft top light and a pointer.
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float lineW = juce::jlimit (2.0f, 3.5f, radius * 0.11f);
    const float arcR = radius - lineW * 0.5f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Palette::panelEdge);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (Palette::accent);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = arcR - lineW - 3.0f;
    const auto body = juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (body.translated (0.0f, 1.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c313b), centre.x, body.getY(),
                                             juce::Colour (0xff181b21), centre.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colour (0xff3a404b));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    g.setColour (Palette::text);
    g.drawLine ({ centre.getPointOnCircumference (knobR * 0.35f, angle), centre.getPointOnCircumference (knobR * 0.85f, angle) }, 2.0f);
}

inline void FamilyLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                                       int thumbStart, int thumbSize, bool over, bool down)
{
    auto track = juce::Rectangle<int> (x, y, width, height).toFloat();
    g.setColour (Palette::field);
    g.fillRoundedRectangle (track, 3.0f);
    auto thumb = vertical ? juce::Rectangle<float> ((float) x, (float) thumbStart, (float) width, (float) thumbSize)
                          : juce::Rectangle<float> ((float) thumbStart, (float) y, (float) thumbSize, (float) height);
    g.setColour (down ? Palette::accent : over ? Palette::panelEdge.brighter (0.6f) : Palette::panelEdge.brighter (0.3f));
    g.fillRoundedRectangle (thumb.reduced (1.5f), 3.0f);
}
