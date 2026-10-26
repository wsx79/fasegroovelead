// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "EditorWidgets.h"

namespace FaseGroove
{
namespace
{
    juce::Font font (float height, bool bold = false) { return FamilyTheme::font (height, bold); }

    // Where a menu keeps the value it shows (a property of the box, never saved).
    constexpr const char* shownKey = "fgShown";
}

void showInMenu (juce::ComboBox& box, int id)
{
    auto& props = box.getProperties();
    if ((int) props.getWithDefault (shownKey, -1) == id)
        return;
    props.set (shownKey, id);
    if (box.getSelectedId() != id)
        box.setSelectedId (id, juce::dontSendNotification);
}

void pickedFromMenu (juce::ComboBox& box)
{
    box.getProperties().set (shownKey, -1);   // shows the value again, whatever it is now
}

void styleCaption (juce::Label& label, const juce::String& text)
{
    label.setText (text.toUpperCase(), juce::dontSendNotification);
    label.setFont (font (11.0f, true));
    label.setColour (juce::Label::textColourId, Palette::dim);
    label.setInterceptsMouseClicks (false, false);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    const auto r = area.toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (Palette::accent);
        g.fillRoundedRectangle ((float) area.getX() + 12.0f, (float) area.getY() + 11.0f, 3.0f, 12.0f, 1.0f);
        g.setColour (Palette::text.withAlpha (0.92f));
        g.setFont (font (11.5f, true));
        g.drawText (Localisation::upper (tr (title)), area.withHeight (34).withTrimmedLeft (21).withTrimmedRight (12), juce::Justification::centredLeft);
    }
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& caption)
    : attachment (state, paramId, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    addAndMakeVisible (slider);

    styleCaption (label, caption);
    Localisation::bindLabel (label, caption);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (14));
    slider.setBounds (r);
}

ParamCombo::ParamCombo (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& caption)
    : parameter (state.getParameter (paramId))
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (parameter))
    {
        for (int i = 0; i < c->choices.size(); ++i)
            box.addItem (c->choices[i], i + 1);
        Localisation::bindItems (box, c->choices);
    }
    else if (auto* n = dynamic_cast<juce::AudioParameterInt*> (parameter))
    {
        for (int v = n->getRange().getStart(); v <= n->getRange().getEnd(); ++v)
            box.addItem (juce::String (v), v - n->getRange().getStart() + 1);
    }

    box.onChange = [this]
    {
        pickedFromMenu (box);
        if (parameter == nullptr || box.getSelectedId() <= 0)
            return;
        const float plain = (float) (box.getSelectedId() - 1);
        const float base = dynamic_cast<juce::AudioParameterInt*> (parameter) != nullptr
                               ? (float) dynamic_cast<juce::AudioParameterInt*> (parameter)->getRange().getStart() : 0.0f;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plain + base));
        parameter->endChangeGesture();
    };
    addAndMakeVisible (box);

    hasCaption = caption.isNotEmpty();
    styleCaption (label, caption);
    if (hasCaption)
        Localisation::bindLabel (label, caption);
    addAndMakeVisible (label);
    label.setVisible (hasCaption);
    refresh();
}

void ParamCombo::resized()
{
    auto r = getLocalBounds();
    if (hasCaption)
        label.setBounds (r.removeFromTop (15));
    box.setBounds (r.removeFromTop (juce::jmin (30, r.getHeight())));
}

void ParamCombo::refresh()
{
    if (parameter == nullptr)
        return;
    const float plain = parameter->convertFrom0to1 (parameter->getValue());
    const float base = dynamic_cast<juce::AudioParameterInt*> (parameter) != nullptr
                           ? (float) dynamic_cast<juce::AudioParameterInt*> (parameter)->getRange().getStart() : 0.0f;
    showInMenu (box, juce::roundToInt (plain - base) + 1);
}

//==============================================================================
namespace
{
    /** Icons are drawn in a unit square, then fitted to the area. */
    juce::Path unitIcon (IconButton::Icon icon, float& strokeWidth, bool& filled)
    {
        using I = IconButton::Icon;
        juce::Path p;
        strokeWidth = 0.09f;
        filled = false;
        switch (icon)
        {
            case I::play:   p.addTriangle (0.28f, 0.18f, 0.28f, 0.82f, 0.82f, 0.5f); filled = true; break;
            case I::pause:  p.addRoundedRectangle (0.24f, 0.2f, 0.17f, 0.6f, 0.03f); p.addRoundedRectangle (0.59f, 0.2f, 0.17f, 0.6f, 0.03f); filled = true; break;
            case I::stop:   p.addRoundedRectangle (0.24f, 0.24f, 0.52f, 0.52f, 0.05f); filled = true; break;
            case I::rewind: p.addRectangle (0.2f, 0.22f, 0.11f, 0.56f); p.addTriangle (0.8f, 0.22f, 0.8f, 0.78f, 0.33f, 0.5f); filled = true; break;
            case I::dice:
                p.addRoundedRectangle (0.14f, 0.14f, 0.72f, 0.72f, 0.14f);
                for (auto pip : { juce::Point<float> (0.33f, 0.33f), { 0.67f, 0.33f }, { 0.5f, 0.5f }, { 0.33f, 0.67f }, { 0.67f, 0.67f } })
                    p.addEllipse (pip.x - 0.065f, pip.y - 0.065f, 0.13f, 0.13f);
                strokeWidth = 0.08f;
                break;
            case I::undo:
            case I::redo:
            {
                juce::Path a;
                a.startNewSubPath (0.16f, 0.36f);
                a.lineTo (0.6f, 0.36f);
                a.addCentredArc (0.6f, 0.57f, 0.21f, 0.21f, 0.0f, 0.0f, juce::MathConstants<float>::pi, false);
                a.lineTo (0.34f, 0.78f);
                a.startNewSubPath (0.34f, 0.18f); a.lineTo (0.16f, 0.36f); a.lineTo (0.34f, 0.54f);
                if (icon == I::redo)
                    a.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (1.0f, 0.0f));
                p = a;
                break;
            }
            case I::gear:
            {
                const int teeth = 8;
                const float outer = 0.46f, inner = 0.34f;
                for (int k = 0; k < teeth * 4; ++k)
                {
                    const float a = ((float) k / (float) (teeth * 4)) * juce::MathConstants<float>::twoPi - juce::MathConstants<float>::halfPi;
                    const float r = (k % 4 == 0 || k % 4 == 1) ? outer : inner;
                    const juce::Point<float> pt (0.5f + r * std::cos (a), 0.5f + r * std::sin (a));
                    if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt);
                }
                p.closeSubPath();
                p.addEllipse (0.36f, 0.36f, 0.28f, 0.28f);
                p.setUsingNonZeroWinding (false);
                filled = true;
                break;
            }
            case I::close:
                p.startNewSubPath (0.24f, 0.24f); p.lineTo (0.76f, 0.76f);
                p.startNewSubPath (0.76f, 0.24f); p.lineTo (0.24f, 0.76f);
                strokeWidth = 0.1f;
                break;
            case I::fullScreen:
            case I::exitFullScreen:
            {
                const bool in = icon == I::exitFullScreen;
                for (int c = 0; c < 4; ++c)
                {
                    const float x = c % 2 == 0 ? 0.16f : 0.84f, y = c < 2 ? 0.16f : 0.84f;
                    const float dx = c % 2 == 0 ? 0.24f : -0.24f, dy = c < 2 ? 0.24f : -0.24f;
                    if (! in) { p.startNewSubPath (x + dx, y); p.lineTo (x, y); p.lineTo (x, y + dy); }
                    else      { p.startNewSubPath (x + dx, y); p.lineTo (x + dx, y + dy); p.lineTo (x, y + dy); }
                }
                break;
            }
            case I::lock:
            case I::unlock:
                p.addRoundedRectangle (0.2f, 0.46f, 0.6f, 0.42f, 0.06f);
                p.startNewSubPath (0.32f, 0.46f);
                if (icon == I::lock)
                {
                    p.lineTo (0.32f, 0.32f);
                    p.addCentredArc (0.5f, 0.32f, 0.18f, 0.18f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, false);
                    p.lineTo (0.68f, 0.46f);
                }
                else
                {
                    p.lineTo (0.32f, 0.3f);
                    p.addCentredArc (0.5f, 0.3f, 0.18f, 0.18f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, false);
                    p.lineTo (0.68f, 0.34f);
                }
                strokeWidth = 0.09f;
                break;
            case I::left:  p.addTriangle (0.7f, 0.2f, 0.7f, 0.8f, 0.26f, 0.5f); filled = true; break;
            case I::right: p.addTriangle (0.3f, 0.2f, 0.3f, 0.8f, 0.74f, 0.5f); filled = true; break;
            case I::song:
                // The song timeline: blocks of different lengths.
                p.addRoundedRectangle (0.06f, 0.36f, 0.2f, 0.28f, 0.04f);
                p.addRoundedRectangle (0.3f, 0.36f, 0.36f, 0.28f, 0.04f);
                p.addRoundedRectangle (0.7f, 0.36f, 0.24f, 0.28f, 0.04f);
                filled = true;
                break;
            case I::folder:
                p.startNewSubPath (0.1f, 0.24f); p.lineTo (0.4f, 0.24f); p.lineTo (0.48f, 0.34f); p.lineTo (0.9f, 0.34f);
                p.lineTo (0.9f, 0.8f); p.lineTo (0.1f, 0.8f); p.closeSubPath();
                strokeWidth = 0.08f;
                break;
            case I::none:
            default: break;
        }
        return p;
    }
}

void IconButton::drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour colour)
{
    if (icon == Icon::none)
        return;
    float stroke = 0.09f;
    bool filled = false;
    auto p = unitIcon (icon, stroke, filled);
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    const auto box = r.withSizeKeepingCentre (s, s);
    p.applyTransform (juce::AffineTransform::scale (s).translated (box.getX(), box.getY()));
    g.setColour (colour);
    if (filled)
        g.fillPath (p);
    else
        g.strokePath (p, juce::PathStrokeType (juce::jmax (1.2f, stroke * s), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    const bool filled = on || primary;
    const bool flatLeft = isConnectedOnLeft(), flatRight = isConnectedOnRight();

    auto fillColour = filled ? onColour : Palette::control;
    if (down)             fillColour = fillColour.darker (0.15f);
    else if (highlighted) fillColour = fillColour.brighter (0.08f);
    if (! isEnabled())    fillColour = fillColour.withMultipliedAlpha (0.5f);

    juce::Path shape;
    shape.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 4.0f, 4.0f, ! flatLeft, ! flatRight, ! flatLeft, ! flatRight);
    g.setColour (fillColour);
    g.fillPath (shape);
    g.setColour (hasKeyboardFocus (true) ? Palette::accent2 : filled ? fillColour.darker (0.3f) : (highlighted ? Palette::panelEdge.brighter (0.35f) : Palette::panelEdge));
    g.strokePath (shape, juce::PathStrokeType (1.0f));

    auto ink = filled ? Palette::background : Palette::text.withAlpha (0.92f);
    if (! isEnabled())
        ink = ink.withMultipliedAlpha (0.4f);
    const auto iconInk = ! filled && ! iconColour.isTransparent() ? iconColour.withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f) : ink;
    const float iconSize = juce::jmin (r.getHeight() * 0.56f, 18.0f);
    const auto label = getButtonText();

    auto content = r.reduced (6.0f, 0.0f);
    if (label.isEmpty())
    {
        drawIcon (g, icon, content.withSizeKeepingCentre (iconSize, iconSize), iconInk);
        return;
    }

    const auto f = font (juce::jlimit (10.0f, 13.0f, r.getHeight() * 0.42f), true);
    const float textW = juce::GlyphArrangement::getStringWidth (f, label);
    const float iconsW = icon == Icon::none ? 0.0f : iconSize + 6.0f;
    auto area = content.withSizeKeepingCentre (juce::jmin (content.getWidth(), iconsW + textW), content.getHeight());
    if (icon != Icon::none)
    {
        drawIcon (g, icon, area.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize), iconInk);
        area.removeFromLeft (6.0f);
    }
    g.setColour (ink);
    g.setFont (f);
    g.drawFittedText (label, area.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
}

//==============================================================================
void DragTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float alpha = isEnabled() ? 1.0f : 0.4f;
    if (filled)
    {
        // Fase Groove Piano's orange tile: a small MIDI clip, the title, what it drags.
        g.setColour (isMouseOver() || dragging ? Palette::accent.brighter (0.1f) : Palette::accent);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Palette::background.withAlpha (0.25f));
        g.drawRoundedRectangle (r.reduced (4.0f), 6.0f, 1.0f);

        auto content = r.reduced (12.0f, 8.0f);
        if (content.getHeight() > 54.0f)
        {
            auto icon = content.removeFromTop (content.getHeight() * 0.38f).withSizeKeepingCentre (56.0f, juce::jmin (26.0f, content.getHeight() * 0.38f));
            g.setColour (Palette::background.withAlpha (0.85f));
            g.fillRoundedRectangle (icon, 4.0f);
            g.setColour (Palette::accent);
            const float w = icon.getWidth() / 6.0f, h = icon.getHeight() / 5.0f;
            const int rows[] { 3, 1, 2, 1 };
            for (int k = 0; k < 4; ++k)
                g.fillRoundedRectangle (icon.getX() + w * (0.6f + 1.2f * k), icon.getY() + h * (float) rows[k], w, h, 1.5f);
        }

        g.setColour (Palette::background);
        g.setFont (font (juce::jmin (15.0f, r.getWidth() / 12.5f), true));
        auto titleArea = subtitle.isEmpty() ? content : content.removeFromTop (content.getHeight() * 0.55f);
        g.drawFittedText (tr (title), titleArea.toNearestInt(), subtitle.isEmpty() ? juce::Justification::centred : juce::Justification::centredBottom, 1, 0.8f);
        if (subtitle.isNotEmpty())
        {
            g.setColour (Palette::background.withAlpha (0.75f));
            g.setFont (font (11.5f));
            g.drawFittedText (tr (subtitle), content.toNearestInt().withTrimmedTop (3), juce::Justification::centredTop, 1, 0.8f);
        }
        return;
    }
    g.setColour (Palette::accent.withAlpha ((isMouseOver() ? 0.22f : 0.12f) * alpha));
    g.fillRoundedRectangle (r, 8.0f);
    const float dashes[] { 5.0f, 4.0f };
    juce::Path outline;
    outline.addRoundedRectangle (r, 8.0f);
    juce::Path dashed;
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (Palette::accent.withAlpha ((dragging ? 1.0f : 0.8f) * alpha));
    g.fillPath (dashed);

    g.setColour (Palette::text.withAlpha (alpha));
    g.setFont (font (juce::jlimit (11.0f, 15.0f, r.getHeight() * 0.22f), true));
    g.drawFittedText (tr (title), r.withTrimmedBottom (r.getHeight() * 0.45f).reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredBottom, 1, 0.7f);
    g.setColour (Palette::dim.withAlpha (alpha));
    g.setFont (font (11.0f));
    g.drawFittedText (tr (subtitle), r.withTrimmedTop (r.getHeight() * 0.6f).reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredTop, 1, 0.7f);
}

void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || ! isEnabled() || e.getDistanceFromDragStart() < 4 || createFiles == nullptr)
        return;

    const auto files = createFiles();
    if (files.isEmpty())
        return;

    dragging = true;
    repaint();
    auto* host = findParentComponentOfClass<MiniModeHost>();
    juce::Component::SafePointer<juce::Component> editor (dynamic_cast<juce::Component*> (host));
    if (host != nullptr)
        host->enterMiniMode();   // at once, as Fase Groove Piano (a deferred call may wait for the drop)
    juce::Component::SafePointer<DragTile> safe (this);
    juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this, [safe, editor]
    {
        if (safe != nullptr)
        {
            safe->dragging = false;
            safe->repaint();
        }
        juce::MessageManager::callAsync ([editor] { if (auto* h = dynamic_cast<MiniModeHost*> (editor.getComponent())) h->exitMiniMode(); });
    });
}

} // namespace FaseGroove
