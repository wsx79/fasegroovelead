// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginEditor.h"

#include "Localisation.h"

#include <BinaryData.h>

using FaseGroove::drawPanel;

namespace
{
    juce::Font font (float height, bool bold = false)
    {
        return FamilyTheme::font (height, bold);
    }


    const int clipBarChoices[] { 0, 1, 2, 4, 8, 16, FaseGrooveLeadProcessor::songArrangement };
    const int songBarChoices[] { 0, 1, 2, 4, 8, 12, 16, 24, 32 };
}

juce::Colour laneColour (int lane)
{
    switch ((fgl::Lane) lane)
    {
        case fgl::Lane::hook:
        case fgl::Lane::answer:    return Palette::accent;
        case fgl::Lane::stab:
        case fgl::Lane::chank:
        case fgl::Lane::scratch:    return Palette::accent2;
        case fgl::Lane::arp:
        case fgl::Lane::lick:       return Palette::cymbals;
        case fgl::Lane::octave:
        case fgl::Lane::power:
        case fgl::Lane::root:       return Palette::toms;
        case fgl::Lane::slide:
        case fgl::Lane::swell:
        default:                    return Palette::percussion;
    }
}

//==============================================================================
LeadGrid::LeadGrid (FaseGrooveLeadProcessor& p) : processor (p)
{
    lights.fill (0.0f);
    refresh();
}

void LeadGrid::refresh()
{
    pattern = processor.displayedPattern();
    for (int l = 0; l < fgl::numLanes; ++l)
        muted[(size_t) l] = processor.isLaneMuted (l);
    playStep = processor.displayStep();
    repaint();
}

void LeadGrid::setPlayStep (int step)
{
    if (step == playStep)
        return;
    const int steps = processor.steps();
    for (int s : { playStep, step })
        if (juce::isPositiveAndBelow (s, steps))
            repaint (cellBounds (0, s).withHeight ((float) getHeight()).getSmallestIntegerContainer().expanded (1, 0));
    playStep = step;
}

juce::Rectangle<float> LeadGrid::headerBounds (int lane) const
{
    const float rowH = (float) getHeight() / (float) fgl::numLanes;
    return { 0.0f, rowH * (float) lane, (float) headerWidth, rowH };
}

juce::Rectangle<float> LeadGrid::diceBounds (int lane) const
{
    auto h = headerBounds (lane).reduced (2.0f, 1.5f);
    return h.removeFromRight (h.getHeight()).reduced (4.0f);
}

juce::Rectangle<float> LeadGrid::cellBounds (int lane, int step) const
{
    const float rowH = (float) getHeight() / (float) fgl::numLanes;
    const float cellW = (float) (getWidth() - headerWidth) / (float) processor.steps();
    return { (float) headerWidth + cellW * (float) step, rowH * (float) lane, cellW, rowH };
}

bool LeadGrid::cellAt (juce::Point<float> p, int& lane, int& step) const
{
    const float rowH = (float) getHeight() / (float) fgl::numLanes;
    const float cellW = (float) (getWidth() - headerWidth) / (float) processor.steps();
    lane = (int) std::floor (p.y / rowH);
    step = (int) std::floor ((p.x - (float) headerWidth) / cellW);
    return p.x >= (float) headerWidth && juce::isPositiveAndBelow (lane, fgl::numLanes)
        && juce::isPositiveAndBelow (step, processor.steps());
}

void LeadGrid::paint (juce::Graphics& g)
{
    const int steps = processor.steps();

    // The playhead column under everything.
    if (juce::isPositiveAndBelow (playStep, steps))
    {
        auto col = cellBounds (0, playStep).withHeight ((float) getHeight());
        g.setColour (Palette::accent2.withAlpha (0.12f));
        g.fillRect (col);
    }

    for (int lane = 0; lane < fgl::numLanes; ++lane)
    {
        const auto colour = laneColour (lane);
        const bool isMuted = muted[(size_t) lane];

        // Header: light, name, octave, die.
        auto h = headerBounds (lane).reduced (2.0f, 1.0f);
        g.setColour (lane == selectedLane ? Palette::control.brighter (0.16f)
                     : lane == hoverLane && hoverStep < 0 ? Palette::control.brighter (0.06f) : Palette::control);
        g.fillRoundedRectangle (h, 4.0f);
        if (lane == selectedLane)
        {
            g.setColour (colour.withAlpha (0.6f));
            g.drawRoundedRectangle (h.reduced (0.5f), 4.0f, 1.0f);
        }
        g.setColour (colour.withAlpha (isMuted ? 0.25f : 0.9f));
        g.fillRoundedRectangle (h.withWidth (3.0f), 1.5f);

        const auto light = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ h.getX() + 12.0f, h.getCentreY() });
        g.setColour (Palette::field);
        g.fillEllipse (light);
        g.setColour (colour.withAlpha (juce::jlimit (0.0f, 1.0f, lights[(size_t) lane])));
        g.fillEllipse (light);

        auto textArea = h.withTrimmedLeft (20.0f).withTrimmedRight (h.getHeight() + 2.0f);
        g.setColour (isMuted ? Palette::dim.withAlpha (0.6f) : Palette::text);
        g.setFont (font (11.0f, true));
        g.drawText (tr (fgl::laneShortName (lane)), textArea.removeFromTop (textArea.getHeight() * 0.58f), juce::Justification::bottomLeft);
        g.setColour (Palette::dim);
        g.setFont (font (9.5f));
        const int octave = processor.laneOctave (lane);
        if (octave != 0 && ! isMuted)
            g.setColour (laneColour (lane));
        // Under the short name: muted, its octave, or its full name when that says more.
        const auto fullName = tr (fgl::laneName (lane));
        g.drawText (isMuted ? tr ("MUTE")
                    : octave != 0 ? tr ("Octave") + " " + (octave > 0 ? "+" : "") + juce::String (octave)
                    : Localisation::upper (fullName) == tr (fgl::laneShortName (lane)) ? juce::String() : fullName,
                    textArea, juce::Justification::topLeft);

        IconButton::drawIcon (g, IconButton::Icon::dice, diceBounds (lane),
                              (lane == hoverLane && hoverDice) ? Palette::accent : Palette::accent.withAlpha (0.55f));

        // Steps.
        const auto& line = pattern.lanes[(size_t) lane];
        for (int step = 0; step < steps; ++step)
        {
            auto c = cellBounds (lane, step).reduced (steps > 32 ? 0.8f : 1.5f, 1.5f);
            const bool beat = (step / 4) % 2 == 0;
            g.setColour (beat ? Palette::panelEdge.withAlpha (0.55f) : Palette::panelEdge.withAlpha (0.32f));
            g.fillRoundedRectangle (c, 3.0f);
            if (lane == hoverLane && step == hoverStep)
            {
                g.setColour (Palette::text.withAlpha (0.12f));
                g.fillRoundedRectangle (c, 3.0f);
            }

            const char symbol = step < (int) line.size() ? line[(size_t) step] : '.';
            if (! fgl::isHit (symbol))
                continue;

            const float alpha = isMuted ? 0.3f : 1.0f;
            switch (symbol)
            {
                case 'X':
                    g.setColour (colour.withAlpha (alpha));
                    g.fillRoundedRectangle (c, 3.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.35f * alpha));
                    g.fillRoundedRectangle (c.withHeight (3.0f).reduced (2.0f, 0.0f).translated (0.0f, 2.0f), 1.5f);
                    break;
                case 'x':
                    g.setColour (colour.withAlpha (0.68f * alpha));
                    g.fillRoundedRectangle (c, 3.0f);
                    break;
                case 'g':
                    g.setColour (colour.withAlpha (0.55f * alpha));
                    g.fillRoundedRectangle (c.withSizeKeepingCentre (c.getWidth() * 0.45f, c.getHeight() * 0.4f), 2.0f);
                    break;
                case 'f':
                    g.setColour (colour.withAlpha (0.8f * alpha));
                    g.fillRoundedRectangle (c.withTrimmedLeft (c.getWidth() * 0.3f), 3.0f);
                    g.fillRoundedRectangle (c.withWidth (c.getWidth() * 0.2f).reduced (0.0f, c.getHeight() * 0.25f), 1.5f);
                    break;
                default:
                    break;
            }

            // A roll: small ticks; a chance: the cell's corner cut and its percentage.
            if (const int n = processor.stepRoll (lane, step); n > 1)
            {
                g.setColour (Palette::background.withAlpha (0.75f));
                for (int k = 1; k < n; ++k)
                    g.fillRect (juce::Rectangle<float> (1.2f, c.getHeight() * 0.5f).withCentre ({ c.getX() + c.getWidth() * (float) k / (float) n, c.getCentreY() }));
            }
            if (const int pct = processor.stepChance (lane, step); pct < 100)
            {
                juce::Path corner;
                corner.addTriangle (c.getRight() - c.getWidth() * 0.55f, c.getY(), c.getRight(), c.getY(), c.getRight(), c.getY() + c.getHeight() * 0.55f);
                g.setColour (Palette::background.withAlpha (0.8f));
                g.fillPath (corner);
                g.setColour (Palette::text.withAlpha (0.9f));
                g.setFont (font (7.5f, true));
                g.drawText ("?", c.withTrimmedLeft (c.getWidth() * 0.5f).withHeight (c.getHeight() * 0.5f), juce::Justification::centred);
            }

            // A velocity set by hand: a small level mark at the bottom of the cell.
            if (const int v = processor.stepVelocity (lane, step); v > 0)
            {
                g.setColour (Palette::background.withAlpha (0.7f));
                g.fillRect (c.removeFromBottom (3.0f).withTrimmedRight (c.getWidth() * (1.0f - (float) v / 127.0f)));
            }
        }
    }

    // Lines between the parts (A B C D) and the bars.
    const int part = steps / fgl::numSegments;
    for (int k = 1; k < fgl::numSegments; ++k)
    {
        const float x = cellBounds (0, k * part).getX();
        const bool strong = k == 2 || part == 16;
        g.setColour (strong ? Palette::dim.withAlpha (0.6f) : Palette::panelEdge.brighter (0.2f));
        g.drawLine (x, 0.0f, x, (float) getHeight(), strong ? 1.2f : 1.0f);
    }
}

void LeadGrid::paintCell (int lane, int step, char symbol)
{
    if (lane == lastPaintLane && step == lastPaintStep)
        return;
    lastPaintLane = lane;
    lastPaintStep = step;
    processor.editStep (lane, step, symbol);
    pattern = processor.displayedPattern();
    repaint();
}

void LeadGrid::mouseDown (const juce::MouseEvent& e)
{
    int lane = 0, step = 0;
    lastPaintLane = lastPaintStep = -1;
    paintSymbol = 0;

    if (! cellAt (e.position, lane, step))
    {
        // The header: the die rolls a new line for the lane, a click plays and picks it, a
        // right-click opens the lane's menu, a double-click shows its velocities.
        if (juce::isPositiveAndBelow (lane, fgl::numLanes) && e.position.x < (float) headerWidth)
        {
            if (e.mods.isPopupMenu())
                showLaneMenu (lane);
            else if (diceBounds (lane).expanded (3.0f).contains (e.position))
            {
                processor.randomizeLane (lane);
                refresh();
            }
            else if (e.getNumberOfClicks() == 2 && onShowVelocity != nullptr)
                onShowVelocity (lane);
            else
            {
                processor.audition (lane, 100);
                if (onSelectLane != nullptr)
                    onSelectLane (lane);
            }
        }
        return;
    }

    if (e.mods.isPopupMenu())
    {
        showCellMenu (lane, step);
        return;
    }

    const char current = pattern.lanes[(size_t) lane][(size_t) step];
    if (e.mods.isShiftDown())                  paintSymbol = 'X';
    else if (e.mods.isAltDown())               paintSymbol = 'g';
    else if (e.mods.isCommandDown())           paintSymbol = 'f';
    else                                       paintSymbol = fgl::isHit (current) ? '.' : brush;
    paintCell (lane, step, paintSymbol);
}

void LeadGrid::mouseDrag (const juce::MouseEvent& e)
{
    int lane = 0, step = 0;
    if (paintSymbol != 0 && cellAt (e.position, lane, step))
        paintCell (lane, step, paintSymbol);
}

void LeadGrid::mouseUp (const juce::MouseEvent&)
{
    paintSymbol = 0;
}

void LeadGrid::mouseMove (const juce::MouseEvent& e)
{
    int lane = 0, step = 0;
    const bool onCell = cellAt (e.position, lane, step);
    const int newLane = juce::isPositiveAndBelow (lane, fgl::numLanes) ? lane : -1;
    const int newStep = onCell ? step : -1;
    const bool overDice = newLane >= 0 && ! onCell && diceBounds (newLane).expanded (3.0f).contains (e.position);
    if (newLane != hoverLane || newStep != hoverStep || overDice != hoverDice)
    {
        hoverLane = newLane;
        hoverStep = newStep;
        hoverDice = overDice;
        setTooltip (overDice ? tr ("A new line for this lane only (in the NEW GROOVE genre; locked parts stay)")
                    : newLane >= 0 && newStep < 0 ? tr ("Click: hear it and pick it. Double-click: its velocities. Right-click: mute, octave, level")
                    : newStep >= 0 ? tr ("Click: hit. Shift: accent, alt: ghost, ctrl: two-note repeat. Drag paints. Right-click: chance and roll")
                    : juce::String());
        repaint();
    }
}

void LeadGrid::mouseExit (const juce::MouseEvent&)
{
    hoverLane = hoverStep = -1;
    hoverDice = false;
    repaint();
}

void LeadGrid::showCellMenu (int lane, int step)
{
    juce::PopupMenu menu;
    menu.addSectionHeader (tr (fgl::laneName (lane)) + " - " + tr ("step") + " " + juce::String (step + 1));
    menu.addItem (1, tr ("Accent (X)"));
    menu.addItem (2, tr ("Hit (x)"));
    menu.addItem (3, tr ("Ghost note (g)"));
    menu.addItem (4, tr ("Rake (f)"));
    menu.addItem (5, tr ("Delete"));
    menu.addItem (6, tr ("Move left"), processor.stepSymbol (lane, step) != '.');
    menu.addItem (7, tr ("Move right"), processor.stepSymbol (lane, step) != '.');

    // How often it plays, and a quick roll of 2-4 repeats in the 16th.
    const bool hit = fgl::isHit (processor.stepSymbol (lane, step));
    juce::PopupMenu chance, roll;
    const int currentChance = processor.stepChance (lane, step), currentRoll = processor.stepRoll (lane, step);
    for (int pct : { 100, 90, 75, 50, 33, 25, 10 })
        chance.addItem (100 + pct, pct == 100 ? tr ("Always") : juce::String (pct) + "%", true, pct == currentChance);
    for (int n = 1; n <= 4; ++n)
        roll.addItem (200 + n, n == 1 ? tr ("One hit") : juce::String (n) + " " + tr ("repeats"), true, n == currentRoll);
    menu.addSeparator();
    menu.addSubMenu (tr ("Chance") + ": " + (currentChance >= 100 ? tr ("Always") : juce::String (currentChance) + "%"), chance, hit);
    menu.addSubMenu (tr ("Roll") + ": " + juce::String (currentRoll), roll, hit);

    juce::Component::SafePointer<LeadGrid> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, lane, step] (int result)
    {
        if (safe == nullptr || result < 1)
            return;
        auto& p = safe->processor;
        const auto choice = stepMenuChoice (result);
        if (choice.kind == StepMenuChoice::Kind::symbol)
            p.editStep (lane, step, (char) choice.value);
        else if (choice.kind == StepMenuChoice::Kind::moveLeft || choice.kind == StepMenuChoice::Kind::moveRight)
        {
            // The hit moves one 16th, with its velocity, chance and roll.
            const int to = (step + (choice.kind == StepMenuChoice::Kind::moveLeft ? -1 : 1) + p.steps()) % p.steps();
            const char symbol = p.stepSymbol (lane, step);
            const int velocity = p.stepVelocity (lane, step), chance = p.stepChance (lane, step), roll = p.stepRoll (lane, step);
            p.editStep (lane, step, '.');
            p.editStep (lane, to, symbol);
            p.setStepVelocity (lane, to, velocity);
            p.setStepChance (lane, to, chance);
            p.setStepRoll (lane, to, roll);
        }
        else if (choice.kind == StepMenuChoice::Kind::roll)
            p.setStepRoll (lane, step, choice.value);
        else if (choice.kind == StepMenuChoice::Kind::chance)
            p.setStepChance (lane, step, choice.value);
        safe->refresh();
    });
}

void LeadGrid::showLaneMenu (int lane)
{
    juce::PopupMenu menu;
    menu.addSectionHeader (tr (fgl::laneName (lane)));
    menu.addItem (1, tr ("Mute"), true, processor.isLaneMuted (lane));
    menu.addItem (3, tr ("New random line (die)"));
    menu.addItem (4, tr ("Edit its velocities"));
    menu.addItem (2, tr ("Clear this row"));
    menu.addSeparator();

    // Up or down by octaves (the voicing stays the same shape).
    juce::PopupMenu octaves;
    const int current = processor.laneOctave (lane);
    for (int o = -2; o <= 2; ++o)
        octaves.addItem (1003 + o, o == 0 ? tr ("As voiced") : (o > 0 ? "+" : "") + juce::String (o), true, o == current);
    menu.addSubMenu (tr ("Octave") + ": " + (current > 0 ? "+" : "") + juce::String (current), octaves);

    juce::PopupMenu levels;
    const float level = processor.raw (ParamIDs::level (lane));
    for (int pct : { 25, 50, 75, 100, 125, 150 })
        levels.addItem (2000 + pct, juce::String (pct) + "%", true, juce::roundToInt (level * 100.0f) == pct);
    menu.addSubMenu (tr ("Level") + ": " + juce::String (juce::roundToInt (level * 100.0f)) + "%", levels);

    juce::Component::SafePointer<LeadGrid> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, lane] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        auto& p = safe->processor;
        if (result == 1)                          p.setLaneMuted (lane, ! p.isLaneMuted (lane));
        else if (result == 2)                     p.clearLane (lane);
        else if (result == 3)                     p.randomizeLane (lane);
        else if (result == 4)                     { if (safe->onShowVelocity != nullptr) safe->onShowVelocity (lane); }
        else if (result >= 1000 && result < 1010) p.setParameterFromUI (ParamIDs::octave (lane), (float) (result - 1003));
        else if (result >= 2000)                  p.setParameterFromUI (ParamIDs::level (lane), (float) (result - 2000) / 100.0f);
        safe->refresh();
    });
}

//==============================================================================
VelocityView::VelocityView (FaseGrooveLeadProcessor& p) : processor (p)
{
    juce::StringArray laneNames;
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        laneBox.addItem (fgl::laneName (l), l + 1);
        laneNames.add (fgl::laneName (l));
    }
    Localisation::bindItems (laneBox, laneNames);
    laneBox.setSelectedId (lane + 1, juce::dontSendNotification);
    Localisation::bindTip (laneBox, "The lane whose velocities you see and edit");
    laneBox.onChange = [this] { setLane (laneBox.getSelectedId() - 1); };
    addAndMakeVisible (laneBox);

    Localisation::bindText (diceButton, "RANDOM");
    diceButton.iconColour = Palette::accent;
    Localisation::bindTip (diceButton, "Random velocities for this lane: a groove shape with a little life on every hit");
    diceButton.onClick = [this] { processor.randomizeVelocities (lane); refresh(); };
    addAndMakeVisible (diceButton);

    const auto shapes = FaseGrooveLeadProcessor::velocityShapeNames();
    for (int i = 0; i < shapes.size(); ++i)
        shapeBox.addItem (shapes[i], i + 1);
    Localisation::bindItems (shapeBox, shapes);
    Localisation::bind (shapeBox, [this] { shapeBox.setTextWhenNothingSelected (tr ("SHAPE...")); });
    Localisation::bindTip (shapeBox, "A velocity shape for this lane's hits (Offbeat push: the funk lead's accent)");
    shapeBox.onChange = [this]
    {
        if (const int id = shapeBox.getSelectedId(); id > 0)
            processor.applyVelocityShape (lane, id - 1);
        shapeBox.setSelectedId (0, juce::dontSendNotification);
        refresh();
    };
    addAndMakeVisible (shapeBox);

    Localisation::bindTip (autoButton, "This lane back to the automatic velocities (from accents and ghost notes)");
    autoButton.onClick = [this] { processor.resetStepVelocities (lane); refresh(); };
    Localisation::bindText (autoButton, "AUTO");
    addAndMakeVisible (autoButton);
    Localisation::bindTip (autoAllButton, "Every lane back to the automatic velocities");
    autoAllButton.onClick = [this] { processor.resetStepVelocities(); refresh(); };
    Localisation::bindText (autoAllButton, "ALL AUTO");
    addAndMakeVisible (autoAllButton);
    refresh();
}

void VelocityView::setLane (int newLane)
{
    lane = juce::jlimit (0, fgl::numLanes - 1, newLane);
    laneBox.setSelectedId (lane + 1, juce::dontSendNotification);
    refresh();
}

void VelocityView::refresh()
{
    const auto p = processor.displayedPattern();
    for (int i = 0; i < processor.steps() && i < p.length(); ++i)
    {
        symbols[(size_t) i] = p.lanes[(size_t) lane][(size_t) i];
        setByHand[(size_t) i] = processor.stepVelocity (lane, i);
        levels[(size_t) i] = processor.effectiveVelocity (lane, i);
    }
    playStep = processor.displayStep();
    repaint();
}

void VelocityView::setPlayStep (int step)
{
    if (step == playStep)
        return;
    const int steps = processor.steps();
    const auto strip = stripArea(), area = laneArea();
    for (int s : { playStep, step })
        if (juce::isPositiveAndBelow (s, steps))
            repaint (column (s, strip).getUnion (column (s, area)).getSmallestIntegerContainer().expanded (1, 1));
    playStep = step;
}

void VelocityView::resized()
{
    auto bar = getLocalBounds().removeFromTop (28);
    laneBox.setBounds (bar.removeFromLeft (LeadGrid::headerWidth + 60));
    bar.removeFromLeft (8);
    diceButton.setBounds (bar.removeFromLeft (100));
    bar.removeFromLeft (6);
    shapeBox.setBounds (bar.removeFromLeft (150));
    bar.removeFromLeft (6);
    autoButton.setBounds (bar.removeFromLeft (64));
    bar.removeFromLeft (6);
    autoAllButton.setBounds (bar.removeFromLeft (86));
}

juce::Rectangle<float> VelocityView::stripArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (38.0f).withHeight (46.0f).withTrimmedLeft ((float) LeadGrid::headerWidth);
}

juce::Rectangle<float> VelocityView::laneArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (96.0f).withTrimmedBottom (4.0f).withTrimmedLeft ((float) LeadGrid::headerWidth);
}

juce::Rectangle<float> VelocityView::column (int step, juce::Rectangle<float> area) const
{
    const float w = area.getWidth() / (float) processor.steps();
    return { area.getX() + w * (float) step, area.getY(), w, area.getHeight() };
}

int VelocityView::stepAt (float x) const
{
    const auto area = laneArea();
    const int step = (int) std::floor ((x - area.getX()) / (area.getWidth() / (float) processor.steps()));
    return juce::isPositiveAndBelow (step, processor.steps()) ? step : -1;
}

void VelocityView::paint (juce::Graphics& g)
{
    const auto colour = laneColour (lane);
    const auto strip = stripArea(), area = laneArea();
    const int steps = processor.steps();

    // Captions on the left.
    g.setColour (Palette::dim);
    g.setFont (font (11.0f, true));
    g.drawText (tr ("STEPS"), juce::Rectangle<float> (0.0f, strip.getY(), (float) LeadGrid::headerWidth - 8.0f, strip.getHeight()), juce::Justification::centredLeft);
    g.drawText (tr ("VELOCITY"), juce::Rectangle<float> (0.0f, area.getY(), (float) LeadGrid::headerWidth - 8.0f, 20.0f), juce::Justification::centredLeft);
    g.setFont (font (10.0f));
    for (int v : { 127, 96, 64, 32 })
    {
        const float y = area.getBottom() - area.getHeight() * (float) v / 127.0f;
        g.setColour (Palette::dim);
        g.drawText (juce::String (v), juce::Rectangle<float> (0.0f, y - 7.0f, (float) LeadGrid::headerWidth - 8.0f, 14.0f), juce::Justification::centredRight);
        g.setColour (Palette::panelEdge.withAlpha (0.6f));
        g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getRight());
    }

    // Playhead.
    if (juce::isPositiveAndBelow (playStep, steps))
    {
        g.setColour (Palette::accent2.withAlpha (0.12f));
        g.fillRect (column (playStep, strip).getUnion (column (playStep, area)));
    }

    for (int i = 0; i < steps; ++i)
    {
        // The lane's steps (click to add / remove a hit).
        auto c = column (i, strip).reduced (1.5f, 2.0f);
        g.setColour ((i / 4) % 2 == 0 ? Palette::panelEdge.withAlpha (0.55f) : Palette::panelEdge.withAlpha (0.32f));
        g.fillRoundedRectangle (c, 3.0f);
        const char symbol = symbols[(size_t) i];
        if (fgl::isHit (symbol))
        {
            g.setColour (colour.withAlpha (symbol == 'X' ? 1.0f : symbol == 'g' ? 0.45f : 0.72f));
            g.fillRoundedRectangle (symbol == 'g' ? c.withSizeKeepingCentre (c.getWidth() * 0.5f, c.getHeight() * 0.4f) : c, 3.0f);
        }

        // Its velocity bar.
        auto col = column (i, area);
        if (i == hoverStep)
        {
            g.setColour (Palette::text.withAlpha (0.05f));
            g.fillRect (col);
        }
        if (! fgl::isHit (symbol))
            continue;
        const float h = col.getHeight() * (float) levels[(size_t) i] / 127.0f;
        auto bar = col.reduced (col.getWidth() * 0.18f, 0.0f).withTop (col.getBottom() - h);
        const bool hand = setByHand[(size_t) i] > 0;
        g.setColour (colour.withAlpha (hand ? 0.95f : 0.38f));
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (hand ? Palette::text : colour);
        g.fillRect (bar.withHeight (2.5f));
        if (i == hoverStep)
        {
            g.setColour (Palette::text);
            g.setFont (font (10.5f, true));
            g.drawText (juce::String (levels[(size_t) i]), bar.withY (bar.getY() - 16.0f).withHeight (14.0f).expanded (8.0f, 0.0f),
                        juce::Justification::centred);
        }
    }

    // Lines between the parts.
    for (int k = 1; k < fgl::numSegments; ++k)
    {
        const float x = column (k * steps / fgl::numSegments, area).getX();
        g.setColour (k == 2 ? Palette::dim.withAlpha (0.6f) : Palette::panelEdge.brighter (0.2f));
        g.drawLine (x, strip.getY(), x, area.getBottom(), 1.0f);
    }

    g.setColour (Palette::dim);
    g.setFont (font (10.5f));
    g.drawFittedText (tr ("drag the bars - double-click: automatic\nfaint bars: automatic levels"),
                      juce::Rectangle<int> (autoAllButton.getRight() + 12, 0, getWidth() - autoAllButton.getRight() - 12, 28),
                      juce::Justification::centredLeft, 2, 0.8f);
}

void VelocityView::setFromMouse (juce::Point<float> pos)
{
    const int step = stepAt (pos.x);
    if (step < 0 || ! fgl::isHit (symbols[(size_t) step]))
        return;
    const auto area = laneArea();
    const int v = juce::jlimit (1, 127, juce::roundToInt ((area.getBottom() - pos.y) / area.getHeight() * 127.0f));
    processor.setStepVelocity (lane, step, v);
    setByHand[(size_t) step] = v;
    levels[(size_t) step] = v;
    repaint();
}

void VelocityView::mouseDown (const juce::MouseEvent& e)
{
    painting = false;
    if (stripArea().contains (e.position))
    {
        if (const int step = stepAt (e.position.x); step >= 0)
        {
            processor.editStep (lane, step, fgl::isHit (symbols[(size_t) step]) ? '.' : 'x');
            refresh();
        }
        return;
    }
    if (laneArea().expanded (0.0f, 6.0f).contains (e.position))
    {
        painting = true;
        setFromMouse (e.position);
    }
}

void VelocityView::mouseDrag (const juce::MouseEvent& e)
{
    if (painting)
        setFromMouse (e.position);
}

void VelocityView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int step = stepAt (e.position.x);
    if (step >= 0 && laneArea().contains (e.position))
    {
        processor.setStepVelocity (lane, step, 0);
        refresh();
    }
}

void VelocityView::mouseMove (const juce::MouseEvent& e)
{
    const int step = laneArea().contains (e.position) || stripArea().contains (e.position) ? stepAt (e.position.x) : -1;
    if (step != hoverStep)
    {
        hoverStep = step;
        repaint();
    }
}

void VelocityView::mouseExit (const juce::MouseEvent&)
{
    hoverStep = -1;
    repaint();
}

//==============================================================================
FillView::FillView (FaseGrooveLeadProcessor& p) : processor (p)
{
    Localisation::bindText (startButton, "START FROM...");
    Localisation::bindTip (startButton, "Start the custom fill from what a fill style plays");
    startButton.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addSectionHeader (tr ("Start from"));
        for (int i = 0; i < fgl::numFillStyles; ++i)
            if (i != (int) fgl::FillStyle::custom)
                menu.addItem (i + 1, tr (fgl::fillStyleName (i)));
        juce::Component::SafePointer<FillView> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&startButton), [safe] (int result)
        {
            if (safe != nullptr && result > 0)
            {
                safe->processor.customFillFromStyle (result - 1);
                safe->repaint();
            }
        });
    };
    Localisation::bindText (clearButton, "CLEAR FILL");
    Localisation::bindTip (clearButton, "Empty the custom fill");
    clearButton.onClick = [this] { processor.clearCustomFill(); repaint(); };
    addAndMakeVisible (startButton);
    addAndMakeVisible (clearButton);
    Localisation::bind (*this, [this] { setTooltip (tr ("Click: hit. Shift: accent, alt: ghost, ctrl: flam. Drag paints. Fill style Custom plays it")); });
}

void FillView::resized()
{
    auto top = getLocalBounds().removeFromTop (24);
    clearButton.setBounds (top.removeFromRight (96));
    top.removeFromRight (6);
    startButton.setBounds (top.removeFromRight (124));
}

juce::Rectangle<float> FillView::gridArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (30.0f);
}

bool FillView::cellAt (juce::Point<float> p, int& lane, int& step) const
{
    const auto area = gridArea();
    const float rowH = area.getHeight() / (float) fgl::numLanes;
    const float cellW = (area.getWidth() - (float) LeadGrid::headerWidth) / 16.0f;
    lane = (int) std::floor ((p.y - area.getY()) / rowH);
    step = (int) std::floor ((p.x - area.getX() - (float) LeadGrid::headerWidth) / cellW);
    return p.x >= area.getX() + (float) LeadGrid::headerWidth && juce::isPositiveAndBelow (lane, fgl::numLanes) && juce::isPositiveAndBelow (step, 16);
}

void FillView::paint (juce::Graphics& g)
{
    const bool active = juce::roundToInt (processor.raw (ParamIDs::fillStyle)) == (int) fgl::FillStyle::custom;
    g.setColour (active ? Palette::accent : Palette::dim);
    g.setFont (font (11.5f, true));
    g.drawFittedText (active ? tr ("Custom fill: plays at the end of every fill bar (from its first step)")
                             : tr ("Draw your own fill: Fill style becomes Custom"),
                      getLocalBounds().removeFromTop (24).withTrimmedRight (236), juce::Justification::centredLeft, 1, 0.7f);

    const auto area = gridArea();
    const float rowH = area.getHeight() / (float) fgl::numLanes;
    const float cellW = (area.getWidth() - (float) LeadGrid::headerWidth) / 16.0f;
    for (int lane = 0; lane < fgl::numLanes; ++lane)
    {
        const auto colour = laneColour (lane);
        auto h = juce::Rectangle<float> (area.getX(), area.getY() + rowH * (float) lane, (float) LeadGrid::headerWidth, rowH).reduced (2.0f, 1.0f);
        g.setColour (Palette::control);
        g.fillRoundedRectangle (h, 4.0f);
        g.setColour (colour.withAlpha (0.9f));
        g.fillRoundedRectangle (h.withWidth (3.0f), 1.5f);
        g.setColour (Palette::text);
        g.setFont (font (11.0f, true));
        g.drawText (tr (fgl::laneShortName (lane)), h.withTrimmedLeft (10.0f), juce::Justification::centredLeft);

        for (int step = 0; step < 16; ++step)
        {
            auto c = juce::Rectangle<float> (area.getX() + (float) LeadGrid::headerWidth + cellW * (float) step, area.getY() + rowH * (float) lane, cellW, rowH).reduced (1.5f);
            g.setColour (Palette::panelEdge.withAlpha ((step / 4) % 2 == 0 ? 0.55f : 0.32f));
            g.fillRoundedRectangle (c, 3.0f);
            const char symbol = processor.customFillSymbol (lane, step);
            if (! fgl::isHit (symbol))
                continue;
            if (symbol == 'g')
            {
                g.setColour (colour.withAlpha (0.55f));
                g.fillRoundedRectangle (c.withSizeKeepingCentre (c.getWidth() * 0.45f, c.getHeight() * 0.4f), 2.0f);
            }
            else
            {
                g.setColour (colour.withAlpha (symbol == 'X' ? 1.0f : symbol == 'f' ? 0.8f : 0.68f));
                g.fillRoundedRectangle (symbol == 'f' ? c.withTrimmedLeft (c.getWidth() * 0.3f) : c, 3.0f);
            }
        }
    }
}

void FillView::paintCell (int lane, int step, char symbol)
{
    if (lane == lastLane && step == lastStep)
        return;
    lastLane = lane;
    lastStep = step;
    processor.editCustomFill (lane, step, symbol);
    repaint();
}

void FillView::mouseDown (const juce::MouseEvent& e)
{
    int lane = 0, step = 0;
    lastLane = lastStep = -1;
    paintSymbol = 0;
    if (! cellAt (e.position, lane, step))
    {
        if (juce::isPositiveAndBelow (lane, fgl::numLanes) && e.position.y > gridArea().getY())
            processor.audition (lane);
        return;
    }
    const char current = processor.customFillSymbol (lane, step);
    if (e.mods.isShiftDown())          paintSymbol = 'X';
    else if (e.mods.isAltDown())       paintSymbol = 'g';
    else if (e.mods.isCommandDown())   paintSymbol = 'f';
    else                               paintSymbol = fgl::isHit (current) ? '.' : brush;
    paintCell (lane, step, paintSymbol);
}

void FillView::mouseDrag (const juce::MouseEvent& e)
{
    int lane = 0, step = 0;
    if (paintSymbol != 0 && cellAt (e.position, lane, step))
        paintCell (lane, step, paintSymbol);
}

//==============================================================================
SongOverlay::SongOverlay (FaseGrooveLeadProcessor& p) : processor (p)
{
    for (int i = 0; i < FaseGrooveLeadProcessor::numSongParts; ++i)
    {
        auto* box = barBoxes.add (new juce::ComboBox());
        for (int bars : songBarChoices)
            box->addItem (bars == 0 ? "-" : juce::String (bars), bars + 1);
        Localisation::bindTip (*box, "Bars of this section (- leaves it out)");
        box->onChange = [this, i, box] { processor.setSongBars (i, box->getSelectedId() - 1); refresh(); };
        addAndMakeVisible (box);

        auto* fresh = newButtons.add (new IconButton (IconButton::Icon::none));
        Localisation::bindText (*fresh, "NEW VARIATION");
        fresh->iconColour = Palette::accent;
        Localisation::bindTip (*fresh, "This section plays its own version of the groove (hats, percussion and ghost notes move)");
        fresh->onClick = [this, i] { processor.regenerateSongPart (i); refresh(); };
        addAndMakeVisible (fresh);

        auto* original = originalButtons.add (new juce::TextButton ("ORIGINAL"));
        Localisation::bindText (*original, "ORIGINAL");
        Localisation::bindTip (*original, "Back to the groove as it is");
        original->onClick = [this, i] { processor.resetSongPart (i); refresh(); };
        addAndMakeVisible (original);

        auto* fill = fillBoxes.add (new juce::ComboBox());
        fill->addItem ("Fill: as set", 1);
        fill->addItem ("No fill", 2);
        for (int f = 0; f < fgl::numFillStyles; ++f)
            fill->addItem (fgl::fillStyleName (f), f + 3);
        Localisation::bind (*fill, [fill]
        {
            fill->changeItemText (1, tr ("Fill: as set"));
            fill->changeItemText (2, tr ("No fill"));
            for (int f = 0; f < fgl::numFillStyles; ++f)
                fill->changeItemText (f + 3, tr ("Fill") + ": " + tr (fgl::fillStyleName (f)));
            fill->setText (fill->getItemText (fill->getSelectedItemIndex()), juce::dontSendNotification);
        });
        Localisation::bindTip (*fill, "The fill on the last bar of this section (as set: the Song panel's fills)");
        fill->onChange = [this, i, fill]
        {
            const int id = fill->getSelectedId();
            processor.setSongFill (i, id == 1 ? -1 : id == 2 ? -2 : id - 3);
        };
        addAndMakeVisible (fill);
    }

    for (auto* b : { &allButton, &tracksButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4321);
        addAndMakeVisible (b);
    }
    allButton.setToggleState (true, juce::dontSendNotification);
    Localisation::bindText (allButton, "ONE TRACK");
    Localisation::bindText (tracksButton, "ONE TRACK PER LANE");
    Localisation::bindText (saveSongButton, "SAVE SONG .MID...");
    Localisation::bindText (saveSectionsButton, "ONE .MID PER SECTION...");
    Localisation::bindText (resetButton, "RESET SONG");
    Localisation::bindText (closeButton, "CLOSE");
    allButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    tracksButton.setConnectedEdges (juce::Button::ConnectedOnLeft);

    Localisation::bindTip (songTile, "Drag the whole song (every section, with markers) onto a track in your DAW");
    songTile.createFiles = [this]
    {
        const auto file = processor.writeMidiClip (FaseGrooveLeadProcessor::songArrangement, clipPart());
        return file.existsAsFile() ? juce::StringArray (file.getFullPathName()) : juce::StringArray();
    };
    Localisation::bindTip (sectionTile, "Drag only the section picked on the timeline (click a section to pick it)");
    sectionTile.createFiles = [this]
    {
        const auto file = processor.writeSongSectionClip (selected, clipPart());
        return file.existsAsFile() ? juce::StringArray (file.getFullPathName()) : juce::StringArray();
    };
    Localisation::bindTip (sectionsTile, "Drag every section at once, each as its own clip: the DAW puts them one after another");
    sectionsTile.createFiles = [this]
    {
        juce::StringArray files;
        for (int i = 0; i < FaseGrooveLeadProcessor::numSongParts; ++i)
            if (const auto file = processor.writeSongSectionClip (i, clipPart()); file.existsAsFile())
                files.add (file.getFullPathName());
        return files;
    };
    for (auto* t : { &sectionTile, &sectionsTile, &songTile })
    {
        t->filled = true;   // the orange tiles of Fase Groove Piano
        addAndMakeVisible (t);
    }

    saveSongButton.onClick = [this] { saveSong(); };
    saveSectionsButton.onClick = [this] { saveSections(); };
    saveSectionsButton.setColour (juce::TextButton::buttonColourId, Palette::accent);
    Localisation::bindTip (resetButton, "Default bars, no own variations, fills as set");
    resetButton.onClick = [this] { processor.resetSong(); refresh(); };
    closeButton.onClick = [this] { setVisible (false); };
    for (auto* b : { &saveSongButton, &saveSectionsButton, &resetButton, &closeButton })
        addAndMakeVisible (b);
}

FaseGrooveLeadProcessor::ClipPart SongOverlay::clipPart() const
{
    return tracksButton.getToggleState() ? FaseGrooveLeadProcessor::ClipPart::tracks : FaseGrooveLeadProcessor::ClipPart::all;
}

juce::Rectangle<float> SongOverlay::card() const
{
    return getLocalBounds().toFloat().withSizeKeepingCentre (960.0f, 640.0f);
}

juce::Rectangle<float> SongOverlay::timelineArea() const
{
    return card().reduced (28.0f, 20.0f).withTrimmedTop (64.0f).withHeight (46.0f);
}

juce::Rectangle<int> SongOverlay::rowArea (int part) const
{
    auto inner = card().toNearestInt().reduced (28, 20);
    inner.removeFromTop (64 + 46 + 14 + 18);   // title, timeline, gap, column names
    return inner.removeFromTop (46 * (part + 1)).removeFromBottom (46).reduced (0, 4);
}

void SongOverlay::show()
{
    refresh();
    setVisible (true);
    toFront (true);
}

void SongOverlay::refresh()
{
    const auto parts = processor.songArrangementParts();
    if (! juce::isPositiveAndBelow (selected, (int) parts.size()) || parts[(size_t) selected].bars <= 0)
    {
        selected = -1;
        for (int i = 0; i < (int) parts.size() && selected < 0; ++i)
            if (parts[(size_t) i].bars > 0)
                selected = i;
    }
    sectionTile.setSubtitle (selected >= 0 ? tr ("Section") + ": " + tr (FaseGrooveLeadProcessor::sectionClipName (selected)) : tr ("No section"));
    sectionTile.setEnabled (selected >= 0);

    for (int i = 0; i < (int) parts.size() && i < barBoxes.size(); ++i)
    {
        int id = 1;
        for (int bars : songBarChoices)
            if (bars <= parts[(size_t) i].bars) id = bars + 1;
        barBoxes[i]->setSelectedId (id, juce::dontSendNotification);
        originalButtons[i]->setEnabled (parts[(size_t) i].variation != 0);
        newButtons[i]->setEnabled (parts[(size_t) i].bars > 0);
        const int fill = parts[(size_t) i].endFill;
        fillBoxes[i]->setSelectedId (fill == -1 ? 1 : fill == -2 ? 2 : fill + 3, juce::dontSendNotification);
    }
    repaint();
}

void SongOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (! card().contains (e.position))
    {
        setVisible (false);
        return;
    }

    // Pick a section: click its block on the timeline.
    const auto line = timelineArea();
    if (! line.contains (e.position))
        return;
    const auto parts = processor.songArrangementParts();
    int total = 0;
    for (const auto& part : parts)
        total += juce::jmax (0, part.bars);
    if (total == 0)
        return;
    float x = line.getX() + 3.0f;
    const float width = line.getWidth() - 6.0f;
    for (int i = 0; i < (int) parts.size(); ++i)
    {
        if (parts[(size_t) i].bars <= 0)
            continue;
        const float w = width * (float) parts[(size_t) i].bars / (float) total;
        if (e.position.x >= x && e.position.x < x + w)
        {
            selected = i;
            refresh();
            return;
        }
        x += w;
    }
}

void SongOverlay::resized()
{
    for (int i = 0; i < barBoxes.size(); ++i)
    {
        auto row = rowArea (i);
        row.removeFromLeft (190);   // section name
        barBoxes[i]->setBounds (row.removeFromLeft (76));
        row.removeFromLeft (14);
        newButtons[i]->setBounds (row.removeFromLeft (150));
        row.removeFromLeft (6);
        originalButtons[i]->setBounds (row.removeFromLeft (100));
        row.removeFromLeft (14);
        fillBoxes[i]->setBounds (row);
    }

    // Bottom, as in Fase Groove Piano: the three drag tiles, then SAVE and CLOSE on one row.
    resetButton.setBounds (card().toNearestInt().reduced (28, 20).removeFromTop (30).removeFromRight (120).withSizeKeepingCentre (120, 28));
    auto bottom = card().toNearestInt().reduced (28, 20).removeFromBottom (52);
    closeButton.setBounds (bottom.removeFromRight (72).withSizeKeepingCentre (72, 32));
    bottom.removeFromRight (10);
    saveSectionsButton.setBounds (bottom.removeFromRight (176).withSizeKeepingCentre (176, 32));
    bottom.removeFromRight (6);
    saveSongButton.setBounds (bottom.removeFromRight (146).withSizeKeepingCentre (146, 32));
    bottom.removeFromRight (8);
    songTile.setBounds (bottom.removeFromRight (162));
    bottom.removeFromRight (8);
    sectionsTile.setBounds (bottom.removeFromRight (162));
    bottom.removeFromRight (8);
    sectionTile.setBounds (bottom.removeFromRight (162));

    // Above the tiles: what they drag (and SAVE writes), as the piano's instrument row.
    auto parts = card().toNearestInt().reduced (28, 20).withTrimmedBottom (60).removeFromBottom (30);
    parts = parts.withLeft (sectionsTile.getX() + 130).withRight (saveSectionsButton.getRight());
    parts.removeFromLeft (90);   // "MIDI" label
    allButton.setBounds (parts.removeFromLeft (parts.getWidth() / 3));
    tracksButton.setBounds (parts);
    allButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    tracksButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
}

void SongOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.78f));
    auto c = card();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (c, 10.0f);
    g.setColour (Palette::panelEdge.brighter (0.3f));
    g.drawRoundedRectangle (c, 10.0f, 1.0f);

    auto inner = c.reduced (28.0f, 20.0f);
    g.setColour (Palette::accent);
    g.setFont (font (20.0f, true));
    g.drawText (Localisation::upper (tr ("Song arrangement")), inner.removeFromTop (28.0f), juce::Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (font (12.0f));
    g.drawFittedText (tr ("Bars of each section (- leaves it out), its own variation of the groove and the fill that leads out of it. "
                          "Drag the whole song, or click a section on the timeline and drag just that one."),
                      inner.removeFromTop (30.0f).toNearestInt(), juce::Justification::topLeft, 2);

    const auto parts = processor.songArrangementParts();
    int total = 0;
    for (const auto& part : parts)
        total += juce::jmax (0, part.bars);

    // The timeline: each section as long as its bars.
    {
        auto line = timelineArea();
        g.setColour (Palette::background);
        g.fillRoundedRectangle (line, 8.0f);
        const juce::Colour colours[] { Palette::accent, juce::Colour (0xff6f8fd6), juce::Colour (0xff9b7fd6), juce::Colour (0xffd6b25f),
                                       Palette::accent2, juce::Colour (0xff6fc8a8) };
        float x = line.getX() + 3.0f;
        const float width = line.getWidth() - 6.0f;
        for (int i = 0; i < (int) parts.size(); ++i)
        {
            const auto& part = parts[(size_t) i];
            if (part.bars <= 0 || total == 0)
                continue;
            const float w = width * (float) part.bars / (float) total;
            auto block = juce::Rectangle<float> (x, line.getY() + 3.0f, w, line.getHeight() - 6.0f).reduced (1.5f, 0.0f);
            const auto colour = colours[(size_t) juce::jlimit (0, 5, (int) part.section)];
            g.setColour (colour.withAlpha (part.variation != 0 ? 1.0f : 0.8f));
            g.fillRoundedRectangle (block, 6.0f);
            if (i == selected)
            {
                g.setColour (Palette::text);
                g.drawRoundedRectangle (block.reduced (1.0f), 6.0f, 2.5f);
            }
            g.setColour (Palette::background);
            g.setFont (font (12.0f, true));
            auto text = block.reduced (6.0f, 3.0f);
            g.drawFittedText (tr (FaseGrooveLeadProcessor::sectionClipName (i)), text.removeFromTop (text.getHeight() * 0.55f).toNearestInt(),
                              juce::Justification::centredLeft, 1, 0.6f);
            g.setFont (font (10.5f));
            g.drawFittedText (juce::String (part.bars) + " " + tr ("bars"), text.toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
            x += w;
        }
    }

    // Column names.
    {
        auto header = rowArea (0).toFloat().translated (0.0f, -22.0f).withHeight (16.0f);
        g.setColour (Palette::dim);
        g.setFont (font (10.5f, true));
        auto col = [&] (float w, const juce::String& text) { g.drawText (Localisation::upper (tr (text)), header.removeFromLeft (w), juce::Justification::centredLeft); };
        col (190.0f, "SECTION");
        col (90.0f, "BARS");
        col (276.0f, "GROOVE");
        col (220.0f, "FILL OUT");
    }

    for (int i = 0; i < (int) parts.size(); ++i)
    {
        const auto row = rowArea (i).toFloat();
        g.setColour (Palette::background.withAlpha (i % 2 == 0 ? 1.0f : 0.6f));
        g.fillRoundedRectangle (row.expanded (8.0f, 2.0f), 6.0f);
        auto label = row.withWidth (180.0f);
        g.setColour (parts[(size_t) i].bars > 0 ? Palette::text : Palette::dim);
        g.setFont (font (14.0f, true));
        g.drawText (tr (FaseGrooveLeadProcessor::sectionClipName (i)), label.removeFromTop (label.getHeight() * 0.56f), juce::Justification::bottomLeft);
        g.setFont (font (10.5f));
        g.setColour (parts[(size_t) i].variation != 0 ? Palette::accent : Palette::dim);
        g.drawText (tr (parts[(size_t) i].bars <= 0 ? "left out" : parts[(size_t) i].variation != 0 ? "own variation" : "the groove as it is"),
                    label, juce::Justification::topLeft);
    }

    // Length of the whole song.
    const double bpm = processor.exportBpm();
    const int seconds = bpm > 0.0 ? juce::roundToInt (total * 4 * 60.0 / bpm) : 0;
    auto info = card().reduced (28.0f, 20.0f).withTrimmedBottom (60.0f).removeFromBottom (34.0f);
    g.setColour (Palette::text);
    g.setFont (font (15.0f, true));
    g.drawText (tr ("Total") + ": " + juce::String (total) + " " + tr ("bars"), info.removeFromTop (19.0f), juce::Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (font (12.0f));
    g.drawText (juce::String (seconds / 60) + ":" + juce::String (seconds % 60).paddedLeft ('0', 2) + "  @ " + juce::String (bpm, 1) + " BPM",
                info, juce::Justification::centredLeft);
    {
        auto label = allButton.getBounds().toFloat().withX ((float) allButton.getX() - 90.0f).withWidth (82.0f);
        g.setFont (font (10.5f, true));
        g.drawText ("MIDI", label, juce::Justification::centredRight);
    }
}

void SongOverlay::saveSong()
{
    auto folder = FaseGrooveLeadProcessor::clipFolder();
    folder.createDirectory();
    const auto part = clipPart();
    chooser = std::make_unique<juce::FileChooser> (tr ("Save the whole song"),
                                                   folder.getChildFile (processor.clipFileName (FaseGrooveLeadProcessor::songArrangement, part)), "*.mid");
    juce::Component::SafePointer<SongOverlay> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, part] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (safe == nullptr || file == juce::File())
            return;
        file = file.withFileExtension ("mid");
        FaseGrooveLeadProcessor::writeMidiFileSafely (safe->processor.createMidiClip (FaseGrooveLeadProcessor::songArrangement, part), file);
    });
}

void SongOverlay::saveSections()
{
    const auto part = clipPart();
    chooser = std::make_unique<juce::FileChooser> (tr ("Choose a folder for the sections"), FaseGrooveLeadProcessor::clipFolder());
    juce::Component::SafePointer<SongOverlay> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe, part] (const juce::FileChooser& fc)
    {
        const auto folder = fc.getResult();
        if (safe == nullptr || folder == juce::File())
            return;
        const int written = safe->processor.saveSongSections (folder, part);
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, tr ("Song sections"),
                                                juce::String (written) + " " + tr (".mid files saved in") + "\n" + folder.getFullPathName());
    });
}

//==============================================================================
OptionsOverlay::OptionsOverlay() : artist (Branding::artistLogo())
{
    setInterceptsMouseClicks (true, true);

    auto styleTab = [this] (juce::TextButton& b)
    {
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonColourId, Palette::background);
        b.setColour (juce::TextButton::buttonOnColourId, Palette::accent);
        b.setColour (juce::TextButton::textColourOffId, Palette::text);
        b.setColour (juce::TextButton::textColourOnId, Palette::background);
        addAndMakeVisible (b);
    };
    styleTab (settingsTab);
    styleTab (aboutTab);
    styleTab (closeButton);
    settingsTab.onClick = [this] { selectTab (false); };
    aboutTab.onClick    = [this] { selectTab (true); };
    closeButton.onClick = [this] { setVisible (false); };

    auto setUpLink = [this] (juce::TextButton& b, const char* url, juce::Colour colour, juce::Colour text)
    {
        b.setColour (juce::TextButton::buttonColourId, colour);
        b.setColour (juce::TextButton::textColourOffId, text);
        b.onClick = [url] { juce::URL (url).launchInDefaultBrowser(); };
        addChildComponent (b);
    };
    setUpLink (sourceButton, Branding::sourceUrl, Palette::background, Palette::text);
    setUpLink (kofiButton, Branding::kofiUrl, Palette::accent, Palette::background);
    setUpLink (paypalButton, Branding::paypalUrl, Palette::accent2, Palette::background);
    setUpLink (spotifyButton, Branding::spotifyUrl, juce::Colour (0xff1db954), Palette::background);

    // Settings tab
    for (auto* l : { &languageLabel, &sizeLabel })
    {
        l->setFont (font (11.0f, true));
        l->setColour (juce::Label::textColourId, Palette::dim);
        addChildComponent (l);
    }

    for (int i = 0; i < Localisation::numLanguages; ++i)
        languageBox.addItem (juce::String::fromUTF8 (Localisation::languageName (i)), i + 1);
    languageBox.onChange = [this]
    {
        Localisation::setCurrent ((Localisation::Language) (languageBox.getSelectedItemIndex()));
        refreshText();
        if (auto* top = getParentComponent())
            top->repaint();
    };
    addChildComponent (languageBox);

    for (int percent : { 60, 75, 90, 100, 110, 125, 150, 175, 200 })
        sizeBox.addItem (juce::String (percent) + " %", percent);
    sizeBox.onChange = [this]
    {
        if (onScaleChosen != nullptr && sizeBox.getSelectedId() > 0)
            onScaleChosen (sizeBox.getSelectedId() / 100.0f);
    };
    addChildComponent (sizeBox);

    tipsToggle.onClick = [this] { if (onTipsChanged != nullptr) onTipsChanged (tipsToggle.getToggleState()); };
    addChildComponent (tipsToggle);
    updatesToggle.onClick = [this] { Updater::setAutoCheckEnabled (updatesToggle.getToggleState()); };
    addChildComponent (updatesToggle);
    checkUpdatesButton.onClick = [this] { if (onCheckUpdates != nullptr) onCheckUpdates(); };
    addChildComponent (checkUpdatesButton);

    refreshText();
    selectTab (false);
}

void OptionsOverlay::show (bool about, float currentScale)
{
    languageBox.setSelectedItemIndex ((int) Localisation::current(), juce::dontSendNotification);

    int closest = 100;
    for (int i = 0; i < sizeBox.getNumItems(); ++i)
        if (std::abs (sizeBox.getItemId (i) - currentScale * 100.0f) < std::abs (closest - currentScale * 100.0f))
            closest = sizeBox.getItemId (i);
    sizeBox.setSelectedId (closest, juce::dontSendNotification);

    tipsToggle.setToggleState (Localisation::tipsEnabled(), juce::dontSendNotification);
    updatesToggle.setToggleState (Updater::autoCheckEnabled(), juce::dontSendNotification);
    refreshText();
    selectTab (about);
    setVisible (true);
    toFront (false);
}

void OptionsOverlay::refreshText()
{
    settingsTab.setButtonText (tr ("SETTINGS"));
    aboutTab.setButtonText (tr ("ABOUT"));
    closeButton.setButtonText (tr ("CLOSE"));
    sourceButton.setButtonText (tr ("SOURCE CODE"));
    kofiButton.setButtonText (tr ("DONATE: Ko-fi"));
    paypalButton.setButtonText (tr ("DONATE: PayPal"));
    spotifyButton.setButtonText (tr ("SPOTIFY"));
    spotifyButton.setTooltip (tr ("Listen to my productions on Spotify"));
    languageLabel.setText (Localisation::upper (tr ("Language")), juce::dontSendNotification);
    sizeLabel.setText (Localisation::upper (tr ("Interface size")), juce::dontSendNotification);
    tipsToggle.setButtonText (tr ("Show tips when the mouse rests on a control"));
    updatesToggle.setButtonText (tr ("Check for updates automatically (GitHub)"));
    if (checkUpdatesButton.isEnabled())
        checkUpdatesButton.setButtonText (tr ("CHECK NOW"));
    repaint();
}

void OptionsOverlay::selectTab (bool about)
{
    showingAbout = about;
    settingsTab.setToggleState (! about, juce::dontSendNotification);
    aboutTab.setToggleState (about, juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &languageLabel, &languageBox, &sizeLabel, &sizeBox, &tipsToggle,
                                                             &updatesToggle, &checkUpdatesButton })
        c->setVisible (! about);

    auto showLink = [about] (juce::Component& b, const char* url) { b.setVisible (about && juce::String (url).isNotEmpty()); };
    showLink (sourceButton, Branding::sourceUrl);
    showLink (kofiButton, Branding::kofiUrl);
    showLink (paypalButton, Branding::paypalUrl);
    showLink (spotifyButton, Branding::spotifyUrl);

    resized();
    repaint();
}

void OptionsOverlay::mouseUp (const juce::MouseEvent& e)
{
    if (! card().contains (e.position))
        setVisible (false);
}

void OptionsOverlay::resized()
{
    auto inner = card().toNearestInt().reduced (24, 18);

    auto tabs = inner.removeFromTop (30);
    closeButton.setBounds (tabs.removeFromRight (100));
    settingsTab.setBounds (tabs.removeFromLeft (150));
    tabs.removeFromLeft (8);
    aboutTab.setBounds (tabs.removeFromLeft (150));

    // Settings tab
    auto settings = inner.withTrimmedTop (40).reduced (40, 0);
    languageLabel.setBounds (settings.removeFromTop (18));
    languageBox.setBounds (settings.removeFromTop (30).withWidth (260));
    settings.removeFromTop (22);
    sizeLabel.setBounds (settings.removeFromTop (18));
    sizeBox.setBounds (settings.removeFromTop (30).withWidth (260));
    settings.removeFromTop (22);
    tipsToggle.setBounds (settings.removeFromTop (26).withWidth (460));
    settings.removeFromTop (10);
    {
        auto row = settings.removeFromTop (26);
        updatesToggle.setBounds (row.removeFromLeft (380));
        checkUpdatesButton.setBounds (row.removeFromLeft (130).reduced (0, 1));
    }

    // About tab: link buttons above the footer
    auto row = inner.withTrimmedBottom (26).removeFromBottom (30);
    juce::Array<juce::Component*> buttons;
    for (auto* b : { &sourceButton, &kofiButton, &paypalButton, &spotifyButton })
        if (b->isVisible())
            buttons.add (b);

    const int w = 126, gap = 10;
    row = row.withSizeKeepingCentre (juce::jmax (1, buttons.size() * w + (buttons.size() - 1) * gap), row.getHeight());
    for (auto* b : buttons)
    {
        b->setBounds (row.removeFromLeft (w));
        row.removeFromLeft (gap);
    }
}

void OptionsOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.78f));

    auto card = this->card();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (card, 16.0f);
    g.setColour (Palette::panelEdge.brighter (0.3f));
    g.drawRoundedRectangle (card, 10.0f, 1.0f);

    auto r = card.reduced (36.0f, 18.0f);
    r.removeFromTop (46.0f);   // tabs
    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");

    if (! showingAbout)
    {
        // Under the settings: the shortcuts of the lead.
        auto keys = juce::Rectangle<float> (r.getX() + 28.0f, r.getY() + 266.0f, r.getWidth() - 28.0f, 220.0f);
        g.setColour (Palette::dim);
        g.setFont (font (11.0f, true));
        g.drawText (Localisation::upper (tr ("Shortcuts")), keys.removeFromTop (18.0f), juce::Justification::centredLeft);
        g.setColour (Palette::text.withAlpha (0.85f));
        g.setFont (font (12.0f));
        for (const auto* line : { "Space: play / pause      Ctrl+Z: undo      Ctrl+Shift+Z / Ctrl+Y: redo",
                                  "Grid: click hit, Shift accent, Alt ghost, Ctrl/Cmd rake, drag to paint",
                                  "Lane name: click plays and picks it, double-click velocities, right-click mute / octave / level",
                                  "Chord slot: click to change it. MIDI IN: hold a chord on the MIDI input",
                                  "Drop a .mid file on the window: its rhythm into the grid, its chords into the slots",
                                  "Drop an audio loop on the window: its groove as a template" })
            g.drawFittedText (tr (line), keys.removeFromTop (20.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);

        g.setColour (Palette::dim);
        g.setFont (font (12.0f));
        g.drawText (tr ("Settings are saved for all your projects."),
                    juce::Rectangle<float> (r.getX() + 28.0f, r.getY() + 500.0f, r.getWidth() - 28.0f, 18.0f), juce::Justification::centredLeft);
        g.setFont (font (11.0f));
        g.drawText ("Fase Groove Lead " + juce::String (JucePlugin_VersionString) + dot + tr ("Built with JUCE"),
                    r.removeFromBottom (16.0f), juce::Justification::centred);
        return;
    }

    Branding::drawFullLogo (g, r.removeFromTop (70.0f).withSizeKeepingCentre (340.0f, 70.0f));

    g.setColour (Palette::dim);
    g.setFont (font (13.0f));
    g.drawText (tr ("Version") + " " + juce::String (JucePlugin_VersionString) + dot + tr (Branding::tagline),
                r.removeFromTop (26.0f), juce::Justification::centred);

    r.removeFromTop (8.0f);
    g.setColour (Palette::panelEdge);
    g.fillRect (r.removeFromTop (1.0f));
    r.removeFromTop (12.0f);

    auto caption = [&g] (const juce::String& text, juce::Rectangle<float> area)
    {
        g.setColour (Palette::accent);
        g.setFont (font (11.0f, true));
        g.drawText (text, area, juce::Justification::centred);
    };

    caption (tr ("CREATED BY"), r.removeFromTop (16.0f));
    r.removeFromTop (6.0f);
    if (artist.isValid())
        g.drawImageWithin (artist, (int) r.getX(), (int) r.getY(), (int) r.getWidth(), 110,
                           juce::RectanglePlacement::centred);
    r.removeFromTop (114.0f);

    g.setColour (Palette::text);
    g.setFont (font (14.0f));
    g.drawText (tr ("DJ & Producer"), r.removeFromTop (20.0f), juce::Justification::centred);
    g.setColour (juce::Colour (0xff1db954));
    g.setFont (font (12.0f, true));
    g.drawText (tr ("Listen to my productions on Spotify"), r.removeFromTop (18.0f), juce::Justification::centred);

    r.removeFromTop (12.0f);
    caption (tr ("CODE"), r.removeFromTop (16.0f));
    r.removeFromTop (4.0f);
    g.setColour (Palette::text);
    g.setFont (font (16.0f, true));
    g.drawText ("Claude  (Anthropic)", r.removeFromTop (22.0f), juce::Justification::centred);
    g.setColour (Palette::dim);
    g.setFont (font (12.0f));
    g.drawText (tr ("AI coding assistant"), r.removeFromTop (16.0f), juce::Justification::centred);

    g.setFont (font (11.0f));
    g.drawText (tr ("Built with JUCE") + dot + "VST is a registered trademark of Steinberg Media Technologies GmbH",
                r.removeFromBottom (16.0f), juce::Justification::centred);

    r.removeFromBottom (48.0f);   // link buttons
    g.setColour (Palette::dim);
    g.setFont (font (12.0f));
    g.drawText (juce::String (Branding::sourceUrl).fromFirstOccurrenceOf ("://", false, false),
                r.removeFromBottom (18.0f), juce::Justification::centred);
    g.setColour (Palette::text);
    g.setFont (font (12.0f, true));
    g.drawText (tr (Branding::licence), r.removeFromBottom (18.0f), juce::Justification::centred);
}

//==============================================================================
PartCard::PartCard (FaseGrooveLeadProcessor& p, int pos)
    : processor (p), position (pos), combo (p.apvts, ParamIDs::segment (pos), juce::String())
{
    combo.setTip ("Which part (A-D) plays in this half bar: e.g. A A A B");
    addAndMakeVisible (combo);
    dice.iconColour = Palette::accent;
    Localisation::bindTip (dice, "New random content for this part (in the groove genre)");
    dice.onClick = [this] { processor.randomizeSegment (processor.segmentAt (position)); };
    addAndMakeVisible (dice);
    Localisation::bindTip (lock, "Lock this part: the dice leave it alone");
    lock.onClick = [this]
    {
        const int part = processor.segmentAt (position);
        processor.setSegmentLocked (part, ! processor.isSegmentLocked (part));
    };
    addAndMakeVisible (lock);
}

void PartCard::refresh (const fgl::LeadPattern& shown, int playStep)
{
    pattern = shown;
    playing = playStep >= 0 && playStep / juce::jmax (1, shown.length() / fgl::numSegments) == position;
    locked = processor.isSegmentLocked (processor.segmentAt (position));
    lock.setIcon (locked ? IconButton::Icon::lock : IconButton::Icon::unlock);
    lock.setToggleState (locked, juce::dontSendNotification);
    dice.setEnabled (! locked);
    combo.refresh();
    repaint();
}

void PartCard::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto top = r.removeFromTop (22);
    dice.setBounds (top.removeFromLeft (26));
    lock.setBounds (top.removeFromRight (26));
    combo.setBounds (r.removeFromBottom (26));
}

void PartCard::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (playing ? Palette::accent.withAlpha (0.1f) : Palette::field);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (playing ? Palette::accent : locked ? Palette::dim : Palette::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, playing ? 1.5f : 1.0f);
    // The bar on top, as on the chord slots (green when it plays).
    g.setColour (playing ? Palette::cymbals : Palette::panelEdge.brighter (0.2f));
    g.fillRoundedRectangle (r.reduced (10.0f, 0.0f).withHeight (3.0f).translated (0.0f, 2.0f), 1.5f);

    const int part = processor.segmentAt (position);
    g.setColour (Palette::dim);
    g.setFont (font (11.0f, true));
    const bool wholeBars = pattern.length() > 32;
    g.drawText (juce::String (position + 1) + "   " + Localisation::upper (tr ("Bar")) + " "
                    + (wholeBars ? juce::String (position + 1) : juce::String (position < 2 ? "1" : "2") + (position % 2 == 0 ? " a" : " b")),
                getLocalBounds().reduced (34, 6).removeFromTop (22), juce::Justification::centred);

    // The part's letter and a picture of its hits: repeats, stabs and scratches, picking, low strings.
    auto body = getLocalBounds().reduced (8).withTrimmedTop (24).withTrimmedBottom (30).toFloat();
    g.setColour (locked ? Palette::dim : Palette::text);
    g.setFont (font (24.0f, true));
    g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + part)), body.removeFromLeft (30.0f), juce::Justification::centred);

    if (pattern.length() < 32)
        return;
    const int partSteps = pattern.length() / fgl::numSegments;
    struct Row { std::initializer_list<fgl::Lane> lanes; int colourLane; };
    const Row rows[] { { { fgl::Lane::hook, fgl::Lane::answer, fgl::Lane::swell, fgl::Lane::slide }, (int) fgl::Lane::hook },
                       { { fgl::Lane::stab, fgl::Lane::chank, fgl::Lane::scratch }, (int) fgl::Lane::stab },
                       { { fgl::Lane::arp, fgl::Lane::lick }, (int) fgl::Lane::arp },
                       { { fgl::Lane::octave, fgl::Lane::power, fgl::Lane::root }, (int) fgl::Lane::root } };
    const float rowH = body.getHeight() / 4.0f, cellW = body.getWidth() / (float) partSteps;
    for (int row = 0; row < 4; ++row)
        for (int i = 0; i < partSteps; ++i)
        {
            char strongest = '.';
            for (auto lane : rows[row].lanes)
            {
                const char c = pattern[lane][(size_t) (position * partSteps + i)];
                if (c == 'X' || (fgl::isHit (c) && strongest == '.') || (c != 'g' && fgl::isHit (c) && strongest == 'g'))
                    strongest = c;
            }
            auto cell = juce::Rectangle<float> (body.getX() + cellW * (float) i, body.getY() + rowH * (float) row, cellW, rowH).reduced (1.5f);
            g.setColour (Palette::panelEdge.withAlpha (0.45f));
            g.fillRoundedRectangle (cell, 1.5f);
            if (fgl::isHit (strongest))
            {
                g.setColour (laneColour (rows[row].colourLane).withAlpha (strongest == 'X' ? 1.0f : strongest == 'g' ? 0.4f : 0.75f));
                g.fillRoundedRectangle (cell, 1.5f);
            }
        }
}

//==============================================================================
GrooveBrowser::GrooveBrowser (FaseGrooveLeadProcessor& p) : processor (p)
{
    setWantsKeyboardFocus (true);
    Localisation::bindText (randomButton, "NEW RANDOM GROOVE IN THIS GENRE");
    randomButton.iconColour = Palette::accent;
    randomButton.onClick = [this]
    {
        if (genre < (int) fgl::grooveCategories().size())
            processor.setParameterFromUI (ParamIDs::genre, (float) genre);
        processor.rollNewBeat();
        repaint();
    };
    addAndMakeVisible (randomButton);
    closeButton.onClick = [this] { setVisible (false); };
    Localisation::bindText (closeButton, "CLOSE");
    addAndMakeVisible (closeButton);
}

juce::Rectangle<float> GrooveBrowser::card() const
{
    return getLocalBounds().toFloat().withSizeKeepingCentre (1040.0f, 680.0f);
}

juce::Rectangle<float> GrooveBrowser::genreBounds (int g) const
{
    auto c = card().reduced (22.0f);
    return { c.getX(), c.getY() + 56.0f + 38.0f * (float) g, 210.0f, 34.0f };
}

juce::Rectangle<float> GrooveBrowser::tileBounds (int index) const
{
    auto c = card().reduced (22.0f);
    auto area = c.withTrimmedLeft (230.0f).withTrimmedTop (56.0f).withTrimmedBottom (56.0f);
    const float w = (area.getWidth() - 2 * 12.0f) / 3.0f, h = 76.0f;
    return { area.getX() + (w + 12.0f) * (float) (index % 3), area.getY() + (h + 10.0f) * (float) (index / 3), w, h };
}

const std::vector<int>& GrooveBrowser::groovesOf (int g) const
{
    static const std::vector<int> yourBeats { fgl::randomPatternIndex(), fgl::userPatternIndex() };
    if (g >= (int) fgl::grooveCategories().size())
        return yourBeats;
    return fgl::patternsOfCategory (g);
}

void GrooveBrowser::show()
{
    const int current = juce::roundToInt (processor.raw (ParamIDs::pattern));
    genre = current < (int) fgl::leadPatterns().size() ? juce::jmax (0, fgl::categoryIndex (fgl::leadPatterns()[(size_t) current].category))
                                                        : (int) fgl::grooveCategories().size();
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
    repaint();
}

void GrooveBrowser::pick (int pattern)
{
    processor.selectGroove (pattern);
    repaint();
}

void GrooveBrowser::resized()
{
    auto c = card().reduced (22.0f).toNearestInt();
    auto bottom = c.removeFromBottom (36);
    closeButton.setBounds (bottom.removeFromRight (90));
    bottom.removeFromRight (10);
    randomButton.setBounds (bottom.removeFromRight (300));
}

void GrooveBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.78f));
    const auto c = card();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (c, 10.0f);
    g.setColour (Palette::panelEdge.brighter (0.3f));
    g.drawRoundedRectangle (c, 10.0f, 1.0f);

    auto inner = c.reduced (22.0f);
    g.setColour (Palette::accent);
    g.setFont (font (20.0f, true));
    g.drawText (Localisation::upper (tr ("Grooves")), inner.removeFromTop (30.0f), juce::Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (font (12.0f));
    g.drawText (tr ("Pick a genre, click a groove: it loads at once (press play to hear it).  Arrows: previous / next."),
                c.reduced (22.0f).withTrimmedTop (30.0f).withHeight (18.0f).withTrimmedLeft (0.0f), juce::Justification::centredLeft);

    // Genres
    const int numGenres = (int) fgl::grooveCategories().size();
    for (int i = 0; i <= numGenres; ++i)
    {
        auto r = genreBounds (i);
        const bool on = i == genre;
        g.setColour (on ? Palette::accent : Palette::control);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? Palette::background : Palette::text);
        g.setFont (font (13.0f, on));
        const auto name = i < numGenres ? juce::String (fgl::grooveCategories()[(size_t) i]) : tr ("Your beats");
        g.drawText (name, r.reduced (12.0f, 0.0f), juce::Justification::centredLeft);
        g.setColour (on ? Palette::background.withAlpha (0.7f) : Palette::dim);
        g.setFont (font (11.0f));
        g.drawText (juce::String ((int) groovesOf (i).size()), r.reduced (12.0f, 0.0f), juce::Justification::centredRight);
    }

    // Grooves of the genre, with a picture of their rhythm.
    const int current = juce::roundToInt (processor.raw (ParamIDs::pattern));
    const auto& list = groovesOf (genre);
    fgl::LeadSettings s = processor.currentSettings();
    for (int k = 0; k < (int) list.size(); ++k)
    {
        const int index = list[(size_t) k];
        auto r = tileBounds (k);
        const bool on = index == current;
        g.setColour (k == hover ? Palette::control.brighter (0.08f) : Palette::field);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? Palette::accent : Palette::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, on ? 2.0f : 1.0f);

        s.pattern = index;
        auto pattern = fgl::GrooveEngine::basePattern (s);
        auto inner2 = r.reduced (10.0f, 7.0f);
        g.setColour (on ? Palette::accent : Palette::text);
        g.setFont (font (13.0f, true));
        g.drawFittedText (index == fgl::userPatternIndex() ? tr ("User pattern") : index == fgl::randomPatternIndex() ? tr ("Random groove") : juce::String (pattern.name),
                          inner2.removeFromTop (18.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        inner2.removeFromTop (5.0f);

        struct Row { std::initializer_list<fgl::Lane> lanes; int colourLane; };
        const Row rows[] { { { fgl::Lane::hook, fgl::Lane::answer, fgl::Lane::swell, fgl::Lane::slide }, (int) fgl::Lane::hook },
                           { { fgl::Lane::stab, fgl::Lane::chank, fgl::Lane::scratch, fgl::Lane::arp, fgl::Lane::lick }, (int) fgl::Lane::stab },
                           { { fgl::Lane::octave, fgl::Lane::power, fgl::Lane::root }, (int) fgl::Lane::root } };
        const float rowH = inner2.getHeight() / 3.0f, cellW = inner2.getWidth() / 32.0f;
        for (int row = 0; row < 3; ++row)
            for (int i = 0; i < 32 && i < pattern.length(); ++i)
            {
                char strongest = '.';
                for (auto lane : rows[row].lanes)
                {
                    const char ch = pattern[lane][(size_t) i];
                    if (ch == 'X' || (fgl::isHit (ch) && (strongest == '.' || strongest == 'g')))
                        strongest = ch;
                }
                auto cell = juce::Rectangle<float> (inner2.getX() + cellW * (float) i, inner2.getY() + rowH * (float) row, cellW, rowH).reduced (0.8f);
                g.setColour (Palette::panelEdge.withAlpha (i / 4 % 2 == 0 ? 0.5f : 0.3f));
                g.fillRect (cell);
                if (fgl::isHit (strongest))
                {
                    g.setColour (laneColour (rows[row].colourLane).withAlpha (strongest == 'X' ? 1.0f : strongest == 'g' ? 0.4f : 0.75f));
                    g.fillRect (cell);
                }
            }
    }
}

void GrooveBrowser::mouseDown (const juce::MouseEvent& e)
{
    if (! card().contains (e.position))
    {
        setVisible (false);
        return;
    }
    for (int i = 0; i <= (int) fgl::grooveCategories().size(); ++i)
        if (genreBounds (i).contains (e.position))
        {
            genre = i;
            hover = -1;
            repaint();
            return;
        }
    const auto& list = groovesOf (genre);
    for (int k = 0; k < (int) list.size(); ++k)
        if (tileBounds (k).contains (e.position))
        {
            pick (list[(size_t) k]);
            if (e.getNumberOfClicks() > 1)
                setVisible (false);
            return;
        }
}

void GrooveBrowser::mouseMove (const juce::MouseEvent& e)
{
    int now = -1;
    const auto& list = groovesOf (genre);
    for (int k = 0; k < (int) list.size(); ++k)
        if (tileBounds (k).contains (e.position))
            now = k;
    if (now != hover)
    {
        hover = now;
        repaint();
    }
}

bool GrooveBrowser::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        setVisible (false);
        return true;
    }
    const auto& list = groovesOf (genre);
    if (list.empty())
        return false;
    const int current = juce::roundToInt (processor.raw (ParamIDs::pattern));
    auto it = std::find (list.begin(), list.end(), current);
    const int at = it == list.end() ? -1 : (int) (it - list.begin());
    if (key == juce::KeyPress::rightKey || key == juce::KeyPress::downKey)
    {
        pick (list[(size_t) ((at + 1) % (int) list.size())]);
        return true;
    }
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::upKey)
    {
        pick (list[(size_t) ((at - 1 + (int) list.size()) % (int) list.size())]);
        return true;
    }
    return false;
}

//==============================================================================
MixerView::MixerView (FaseGrooveLeadProcessor& p) : processor (p)
{
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        auto* f = faders.add (new juce::Slider (juce::Slider::LinearVertical, juce::Slider::TextBoxBelow));
        f->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
        f->setColour (juce::Slider::trackColourId, laneColour (l));
        f->setColour (juce::Slider::thumbColourId, Palette::text);
        f->setColour (juce::Slider::backgroundColourId, Palette::field);
        f->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        Localisation::bind (*f, [f, l] { f->setTooltip (tr (fgl::laneName (l)) + ": " + tr ("volume of the guide sound. The MIDI velocities don't change: LEVEL does that")); });
        f->setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (f);
        faderAttachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (processor.apvts, ParamIDs::laneVolume (l), *f));

        auto* m = mutes.add (new juce::TextButton ("M"));
        m->setClickingTogglesState (true);
        m->setColour (juce::TextButton::buttonOnColourId, Palette::toms);
        Localisation::bind (*m, [m, l] { m->setTooltip (tr ("Mute") + ": " + tr (fgl::laneName (l))); });
        addAndMakeVisible (m);
        muteAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (processor.apvts, ParamIDs::mute (l), *m));

        // The lane's feel: ahead of / behind the beat, and its share of the swing.
        for (int k = 0; k < 2; ++k)
        {
            auto* knob = (k == 0 ? pushKnobs : swingKnobs).add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow));
            knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
            knob->setColour (juce::Slider::rotarySliderFillColourId, laneColour (l));
            knob->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            knob->setDoubleClickReturnValue (true, k == 0 ? 0.0 : 1.0);
            if (k == 0)
                Localisation::bind (*knob, [knob, l] { knob->setTooltip (tr (fgl::laneName (l)) + ": " + tr ("PUSH: ahead of the beat (-) or laid back (+), in milliseconds")); });
            else
                Localisation::bind (*knob, [knob, l] { knob->setTooltip (tr (fgl::laneName (l)) + ": " + tr ("SWING: how much of the swing this lane takes (100% = as the Swing knob)")); });
            addAndMakeVisible (knob);
            knobAttachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (processor.apvts, k == 0 ? ParamIDs::push (l) : ParamIDs::laneSwing (l), *knob));
        }
    }
}

juce::Rectangle<int> MixerView::strip (int lane) const
{
    const int w = getWidth() / fgl::numLanes;
    return { lane * w, 0, w, getHeight() };
}

void MixerView::resized()
{
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        auto r = strip (l).reduced (4, 2);
        r.removeFromTop (30);
        r.removeFromTop (10);
        pushKnobs[l]->setBounds (r.removeFromTop (78));
        r.removeFromTop (10);
        swingKnobs[l]->setBounds (r.removeFromTop (78));
        mutes[l]->setBounds (r.removeFromBottom (22).reduced (6, 0));
        r.removeFromBottom (4);
        faders[l]->setBounds (r);
    }
}

void MixerView::paint (juce::Graphics& g)
{
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        auto r = strip (l).reduced (2, 0).toFloat();
        g.setColour (Palette::field);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (laneColour (l).withAlpha (0.9f));
        g.fillRoundedRectangle (r.withHeight (3.0f).reduced (6.0f, 0.0f), 1.5f);
        g.setColour (Palette::text);
        g.setFont (font (10.5f, true));
        g.drawText (tr (fgl::laneShortName (l)), r.withTrimmedTop (5.0f).withHeight (14.0f), juce::Justification::centred);
        const float light = juce::jlimit (0.0f, 1.0f, lights[(size_t) l]);
        auto led = juce::Rectangle<float> (r.getWidth() - 20.0f, 4.0f).withCentre ({ r.getCentreX(), r.getY() + 24.0f });
        g.setColour (Palette::panelEdge);
        g.fillRoundedRectangle (led, 2.0f);
        g.setColour (laneColour (l));
        g.fillRoundedRectangle (led.withWidth (led.getWidth() * light), 2.0f);

        g.setColour (Palette::dim);
        g.setFont (font (8.5f, true));
        g.drawText (Localisation::upper (tr ("Push")), r.withTrimmedTop (32.0f).withHeight (10.0f), juce::Justification::centred);
        g.drawText (Localisation::upper (tr ("Swing")), r.withTrimmedTop (120.0f).withHeight (10.0f), juce::Justification::centred);
    }
}

//==============================================================================
ChordPanel::ChordPanel (FaseGrooveLeadProcessor& p) : processor (p)
{
    Localisation::bind (*this, [this] { setTooltip (tr ("The chord of each bar: click one to change it")); });
    for (int k = 0; k < 12; ++k)
        keyBox.addItem (juce::String (fgl::pitchClassName (k)), k + 1);
    Localisation::bindTip (keyBox, "The key: the chords move with it");
    keyBox.onChange = [this]
    {
        pickedFromMenu (keyBox);
        if (const int id = keyBox.getSelectedId(); id > 0)
            processor.setKey (id - 1);
    };
    addAndMakeVisible (keyBox);

    Localisation::bindText (progressionButton, "PROGRESSION...");
    Localisation::bindTip (progressionButton, "Four chords by genre, in the key (the same genres as Fase Groove Piano)");
    progressionButton.onClick = [this] { if (onBrowse != nullptr) onBrowse(); };
    addAndMakeVisible (progressionButton);

    diceButton.iconColour = Palette::accent;
    Localisation::bindTip (diceButton, "New chords: a progression of the groove genre (undo with the arrow)");
    diceButton.onClick = [this] { processor.randomizeChords(); };
    addAndMakeVisible (diceButton);

    Localisation::bindText (inputButton, "MIDI IN");
    inputButton.setClickingTogglesState (true);
    inputButton.setColour (juce::TextButton::buttonOnColourId, Palette::linked);
    Localisation::bindTip (inputButton, "On: a chord held on the MIDI input (your keyboard, or Fase Groove Piano's MIDI out on this track) is what the lead plays");
    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::chordInput, inputButton);
    addAndMakeVisible (inputButton);
    refresh();
}

juce::String ChordPanel::nameOf (const fgl::ChordTones& c)
{
    if (c.mask == 0)
        return {};
    // The chord type that matches exactly, else the biggest one inside the notes held.
    int best = -1, bestSize = 0;
    for (int t = 0; t < fgl::numChordTypes; ++t)
    {
        const auto mask = fgl::chordTypeMask (t);
        int size = 0;
        for (unsigned m = mask; m != 0; m >>= 1)
            size += (int) (m & 1u);
        if (mask == c.mask)
            return fgl::chordName ({ c.root, t });
        if ((mask & ~c.mask) == 0 && size > bestSize)
        {
            best = t;
            bestSize = size;
        }
    }
    if (best >= 0)
        return fgl::chordName ({ c.root, best });
    return juce::String (fgl::pitchClassName (c.root)) + (c.mask == 0x81 ? " (5)" : " (?)");
}

void ChordPanel::refresh()
{
    showInMenu (keyBox, juce::roundToInt (processor.raw (ParamIDs::key)) + 1);
    std::array<fgl::Chord, fgl::maxChords> now;
    for (int i = 0; i < fgl::maxChords; ++i)
        now[(size_t) i] = processor.chordAt (i);
    const int n = processor.chordCount();
    const double ppq = processor.displayPpq();
    const int playing = ppq >= 0.0 ? (int) std::fmod (std::floor (ppq / 4.0), (double) n) : -1;
    const auto heldNow = processor.heldChord();
    const bool followingNow = processor.followsHeldChord();
    if (now != chords || n != count || playing != playingSlot || ! (heldNow == held) || followingNow != following)
    {
        chords = now;
        count = n;
        playingSlot = playing;
        held = heldNow;
        following = followingNow;
        repaint();
    }
}

juce::Rectangle<float> ChordPanel::slotBounds (int slot) const
{
    // One row of up to four chords; five to eight in two rows.
    const int perRow = count > 4 ? (count + 1) / 2 : juce::jmax (1, count);
    const int rows = count > 4 ? 2 : 1;
    const float w = (float) getWidth() / (float) perRow, h = 76.0f / (float) rows;
    return juce::Rectangle<float> (w * (float) (slot % perRow), 34.0f + h * (float) (slot / perRow), w, h).reduced (3.0f, rows > 1 ? 1.5f : 0.0f);
}

void ChordPanel::resized()
{
    keyBox.setBounds (0, 0, 64, 26);
    progressionButton.setBounds (68, 0, getWidth() - 68 - 40, 26);
    diceButton.setBounds (getWidth() - 36, 0, 36, 26);
    inputButton.setBounds (0, getHeight() - 28, 82, 26);
}

void ChordPanel::paint (juce::Graphics& g)
{
    const bool small = count > 4;
    for (int i = 0; i < count; ++i)
    {
        const auto r = slotBounds (i);
        const bool on = i == playingSlot && ! following;
        g.setColour (on ? Palette::control.brighter (0.12f) : Palette::control);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? Palette::accent2 : Palette::panelEdge.brighter (0.15f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, on ? 1.6f : 1.0f);

        auto inner = r.reduced (4.0f, small ? 2.0f : 6.0f);
        g.setColour (Palette::dim);
        g.setFont (font (small ? 8.0f : 9.5f, true));
        g.drawText (small ? juce::String (i + 1) : Localisation::upper (tr ("Bar")) + " " + juce::String (i + 1),
                    inner.removeFromTop (small ? 9.0f : 12.0f), juce::Justification::centred);
        g.setColour (following ? Palette::dim.withAlpha (0.5f) : Palette::text);
        g.setFont (font (small ? 12.5f : fgl::chordName (chords[(size_t) i]).size() > 5 ? 15.0f : 19.0f, true));
        g.drawFittedText (fgl::chordName (chords[(size_t) i]), inner.toNearestInt(), juce::Justification::centred, 1, 0.6f);
    }

    // The chord held on the MIDI input.
    auto info = juce::Rectangle<float> (88.0f, (float) getHeight() - 28.0f, (float) getWidth() - 88.0f, 26.0f);
    g.setFont (font (11.5f, true));
    if (following)
    {
        g.setColour (Palette::accent2);
        g.drawFittedText (tr ("Playing the chord held") + ": " + nameOf (held), info.toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
    }
    else
    {
        g.setColour (Palette::dim);
        g.drawFittedText (processor.raw (ParamIDs::chordInput) > 0.5f ? tr ("Hold a chord on the MIDI input") : tr ("The chord slots play"),
                          info.toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
    }
}

void ChordPanel::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < count; ++i)
        if (slotBounds (i).contains (e.position))
        {
            showSlotMenu (i);
            return;
        }
}

void ChordPanel::showSlotMenu (int slot)
{
    const auto chord = processor.chordAt (slot);
    juce::PopupMenu menu, roots, types, copies;
    menu.addSectionHeader (Localisation::upper (tr ("Bar")) + " " + juce::String (slot + 1) + ": " + fgl::chordName (chord));
    for (int k = 0; k < 12; ++k)
        roots.addItem (100 + k, fgl::pitchClassName (k), true, k == chord.root);
    for (int t = 0; t < fgl::numChordTypes; ++t)
        types.addItem (200 + t, juce::String (fgl::pitchClassName (chord.root)) + fgl::chordTypeName (t), true, t == chord.type);
    menu.addSubMenu (tr ("Root") + ": " + fgl::pitchClassName (chord.root), roots);
    menu.addSubMenu (tr ("Chord") + ": " + fgl::chordName (chord), types);
    menu.addSeparator();
    for (int i = 0; i < processor.chordCount(); ++i)
        if (i != slot)
            copies.addItem (300 + i, Localisation::upper (tr ("Bar")) + " " + juce::String (i + 1) + ": " + fgl::chordName (processor.chordAt (i)));
    menu.addSubMenu (tr ("The same chord as..."), copies);
    menu.addItem (400, tr ("This chord on every bar"));
    juce::PopupMenu counts;
    for (int n = 1; n <= fgl::maxChords; ++n)
        counts.addItem (500 + n, juce::String (n), true, n == processor.chordCount());
    menu.addSubMenu (tr ("Chords") + ": " + juce::String (processor.chordCount()), counts);

    juce::Component::SafePointer<ChordPanel> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, slot, chord] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        auto& p = safe->processor;
        if (result >= 100 && result < 112)      p.setChord (slot, { result - 100, chord.type });
        else if (result >= 200 && result < 300) p.setChord (slot, { chord.root, result - 200 });
        else if (const int from = chordMenuCopySlot (result); from >= 0) p.setChord (slot, p.chordAt (from));
        else if (result == 400)
            for (int i = 0; i < fgl::maxChords; ++i)
                p.setChord (i, chord);
        else if (result > 500 && result <= 500 + fgl::maxChords)
            p.setChordCount (result - 500);
        safe->refresh();
    });
}


//==============================================================================
ProgressionBrowser::ProgressionBrowser (FaseGrooveLeadProcessor& p) : processor (p)
{
    setWantsKeyboardFocus (true);
    for (int k = 0; k < 12; ++k)
        keyBox.addItem (fgl::pitchClassName (k), k + 1);
    for (int sc = 0; sc < fgl::numScales; ++sc)
        scaleBox.addItem (fgl::scaleName (sc), sc + 1);
    juce::StringArray colourNames;
    for (int col = 0; col < fgl::numChordColours; ++col)
    {
        colourBox.addItem (fgl::chordColourName (col), col + 1);
        colourNames.add (fgl::chordColourName (col));
    }
    Localisation::bindItems (colourBox, colourNames);
    for (int n = 1; n <= fgl::maxChords; ++n)
        countBox.addItem (juce::String (n), n);
    keyBox.onChange = [this] { pickedFromMenu (keyBox); if (keyBox.getSelectedId() > 0) processor.setKey (keyBox.getSelectedId() - 1); refresh(); };
    scaleBox.onChange = [this] { pickedFromMenu (scaleBox); if (scaleBox.getSelectedId() > 0) processor.setParameterFromUI (ParamIDs::scale, (float) (scaleBox.getSelectedId() - 1)); refresh(); };
    colourBox.onChange = [this]
    {
        pickedFromMenu (colourBox);
        if (colourBox.getSelectedId() <= 0)
            return;
        processor.setParameterFromUI (ParamIDs::chordColour, (float) (colourBox.getSelectedId() - 1));
        processor.applyProgression (juce::roundToInt (processor.raw (ParamIDs::progression)));   // the same chords, richer or plainer
        refresh();
    };
    countBox.onChange = [this] { pickedFromMenu (countBox); if (countBox.getSelectedId() > 0) processor.setChordCount (countBox.getSelectedId()); refresh(); };
    for (auto* b : { &keyBox, &scaleBox, &colourBox, &countBox })
        addAndMakeVisible (b);

    harmonyDice.iconColour = Palette::accent;
    harmonyDice.onClick = [this] { processor.randomizeChords(); refresh(); };
    addAndMakeVisible (harmonyDice);
    closeButton.onClick = [this] { setVisible (false); };
    addAndMakeVisible (closeButton);
    Localisation::bind (*this, [this]
    {
        harmonyDice.setButtonText (tr ("NEW HARMONY"));
        harmonyDice.setTooltip (tr ("Generate a new harmonic path for the lead melody in the selected key and scale"));
        closeButton.setButtonText (tr ("CLOSE"));
        keyBox.setTooltip (tr ("The key: the chords move with it"));
        scaleBox.setTooltip (tr ("The scale used to generate the harmonic path"));
        colourBox.setTooltip (tr ("Triads, 7ths or 9ths: how rich the chords of a progression are"));
        countBox.setTooltip (tr ("How many chords play, one per bar"));
        repaint();
    });
}

juce::Rectangle<float> ProgressionBrowser::card() const
{
    return getLocalBounds().toFloat().reduced (14.0f);
}

juce::Rectangle<float> ProgressionBrowser::categoryBounds (int i) const
{
    auto c = card().reduced (20.0f);
    return { c.getX(), c.getY() + 56.0f + 40.0f * (float) i, 200.0f, 36.0f };
}

juce::Rectangle<float> ProgressionBrowser::listArea() const
{
    auto c = card().reduced (20.0f);
    return { c.getX() + 216.0f, c.getY() + 56.0f, c.getWidth() - 216.0f - 340.0f, c.getHeight() - 56.0f };
}

juce::Rectangle<float> ProgressionBrowser::tileBounds (int index) const
{
    const auto a = listArea();
    const float w = (a.getWidth() - 10.0f) / 2.0f, h = 50.0f;
    return { a.getX() + (w + 10.0f) * (float) (index % 2), a.getY() + (h + 8.0f) * (float) (index / 2), w, h };
}

std::vector<int> ProgressionBrowser::progressionsOf (int cat) const
{
    std::vector<int> list;
    const auto& all = fgl::progressions();
    const auto& cats = fgl::progressionCategories();
    for (size_t i = 0; i < all.size(); ++i)
        if (juce::isPositiveAndBelow (cat, (int) cats.size()) && juce::String (all[i].category) == cats[(size_t) cat])
            list.push_back ((int) i);
    return list;
}

void ProgressionBrowser::show()
{
    const int current = juce::roundToInt (processor.raw (ParamIDs::progression));
    const auto& cats = fgl::progressionCategories();
    for (int i = 0; i < (int) cats.size(); ++i)
        if (juce::String (fgl::progressions()[(size_t) juce::jlimit (0, (int) fgl::progressions().size() - 1, current)].category) == cats[(size_t) i])
            category = i;
    scroll = 0.0f;
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
    refresh();
}

void ProgressionBrowser::pick (int progression)
{
    processor.applyProgression (progression);
    refresh();
}

void ProgressionBrowser::resized()
{
    auto c = card().reduced (20.0f);
    auto right = juce::Rectangle<float> (c.getRight() - 324.0f, c.getY() + 48.0f, 324.0f, c.getHeight() - 48.0f).toNearestInt();
    right.removeFromTop (210);
    right.removeFromTop (22);
    auto row1 = right.removeFromTop (28);
    keyBox.setBounds (row1.removeFromLeft (70));
    row1.removeFromLeft (6);
    scaleBox.setBounds (row1);
    right.removeFromTop (22);
    auto row2 = right.removeFromTop (28);
    colourBox.setBounds (row2.removeFromLeft (150));
    row2.removeFromLeft (6);
    countBox.setBounds (row2.removeFromLeft (70));
    right.removeFromTop (14);
    harmonyDice.setBounds (right.removeFromTop (32));
    closeButton.setBounds (right.removeFromBottom (30).removeFromRight (100));
}

void ProgressionBrowser::refresh()
{
    showInMenu (keyBox, juce::roundToInt (processor.raw (ParamIDs::key)) + 1);
    showInMenu (scaleBox, juce::roundToInt (processor.raw (ParamIDs::scale)) + 1);
    showInMenu (colourBox, juce::roundToInt (processor.raw (ParamIDs::chordColour)) + 1);
    showInMenu (countBox, processor.chordCount());
    repaint();
}

void ProgressionBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.78f));
    const auto c = card();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (c, 10.0f);
    g.setColour (Palette::panelEdge.brighter (0.3f));
    g.drawRoundedRectangle (c, 10.0f, 1.0f);

    auto inner = c.reduced (20.0f);
    g.setColour (Palette::accent);
    g.setFont (font (20.0f, true));
    g.drawText (Localisation::upper (tr ("Chord progressions")), inner.removeFromTop (30.0f), juce::Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (font (12.0f));
    g.drawText (tr ("Pick a category, click a progression: its chords load at once, in the key.  Arrows: previous / next."),
                inner.removeFromTop (18.0f), juce::Justification::centredLeft);

    const auto& cats = fgl::progressionCategories();
    for (int i = 0; i < (int) cats.size(); ++i)
    {
        auto r = categoryBounds (i);
        const bool on = i == category;
        g.setColour (on ? Palette::accent : Palette::control);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? Palette::background : Palette::text);
        g.setFont (font (13.0f, on));
        g.drawText (cats[(size_t) i], r.reduced (12.0f, 0.0f), juce::Justification::centredLeft);
        g.setColour (on ? Palette::background.withAlpha (0.7f) : Palette::dim);
        g.setFont (font (11.0f));
        g.drawText (juce::String ((int) progressionsOf (i).size()), r.reduced (12.0f, 0.0f), juce::Justification::centredRight);
    }

    auto preview = juce::Rectangle<float> (c.getRight() - 344.0f, c.getY() + 72.0f, 324.0f, 160.0f);
    g.setColour (Palette::control);
    g.fillRoundedRectangle (preview, 6.0f);
    const auto settings = processor.currentSettings();
    const auto events = fgl::GrooveEngine::renderClip (settings, 8.0);
    g.setColour (Palette::accent);
    for (const auto& e : events)
        if (e.isNoteOn() && e.lane < fgl::numLanes)
            g.fillRoundedRectangle (preview.getX() + 8.0f + (float) e.ppq * 37.0f,
                                    preview.getBottom() - 12.0f - (float) (e.note - 36) * 2.0f,
                                    4.0f, 3.0f, 1.0f);

    // Captions over the key, scale, colour and number of chords.
    g.setColour (Palette::dim);
    g.setFont (font (10.5f, true));
    for (auto [box, text] : { std::pair<juce::Component*, const char*> { &keyBox, "Key" }, { &scaleBox, "Scale" }, { &colourBox, "Colour" }, { &countBox, "Chords" } })
        g.drawText (Localisation::upper (tr (text)), box->getBounds().translated (0, -15).withHeight (14), juce::Justification::centredLeft);

    // The progressions of the category, with their chords in the key.
    const int key = juce::roundToInt (processor.raw (ParamIDs::key)), colour = juce::roundToInt (processor.raw (ParamIDs::chordColour));
    const int current = juce::roundToInt (processor.raw (ParamIDs::progression));
    const auto area = listArea();
    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (area.toNearestInt());
    const auto list = progressionsOf (category);
    for (int k = 0; k < (int) list.size(); ++k)
    {
        auto r = tileBounds (k).translated (0.0f, -scroll);
        if (! r.intersects (area))
            continue;
        const int index = list[(size_t) k];
        const bool on = index == current;
        g.setColour (k == hover ? Palette::control.brighter (0.08f) : Palette::field);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? Palette::accent : Palette::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, on ? 2.0f : 1.0f);
        auto t = r.reduced (10.0f, 6.0f);
        g.setColour (on ? Palette::accent : Palette::text);
        g.setFont (font (12.5f, true));
        g.drawFittedText (fgl::progressions()[(size_t) index].name, t.removeFromTop (18.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        juce::StringArray names;
        for (const auto& ch : fgl::progressionChords (index, key, colour))
            names.add (fgl::chordName (ch));
        g.setColour (Palette::accent2);
        g.setFont (font (12.0f));
        g.drawFittedText (names.joinIntoString ("  "), t.toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
    }
}

void ProgressionBrowser::mouseDown (const juce::MouseEvent& e)
{
    if (! card().contains (e.position))
    {
        setVisible (false);
        return;
    }
    for (int i = 0; i < (int) fgl::progressionCategories().size(); ++i)
        if (categoryBounds (i).contains (e.position))
        {
            category = i;
            hover = -1;
            scroll = 0.0f;
            repaint();
            return;
        }
    if (! listArea().contains (e.position))
        return;
    const auto list = progressionsOf (category);
    for (int k = 0; k < (int) list.size(); ++k)
        if (tileBounds (k).translated (0.0f, -scroll).contains (e.position))
        {
            pick (list[(size_t) k]);
            if (e.getNumberOfClicks() > 1)
                setVisible (false);
            return;
        }
}

void ProgressionBrowser::mouseMove (const juce::MouseEvent& e)
{
    int now = -1;
    const auto list = progressionsOf (category);
    if (listArea().contains (e.position))
        for (int k = 0; k < (int) list.size(); ++k)
            if (tileBounds (k).translated (0.0f, -scroll).contains (e.position))
                now = k;
    if (now != hover)
    {
        hover = now;
        repaint();
    }
}

void ProgressionBrowser::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const auto list = progressionsOf (category);
    const float content = list.empty() ? 0.0f : tileBounds ((int) list.size() - 1).getBottom() - listArea().getY();
    scroll = juce::jlimit (0.0f, juce::jmax (0.0f, content - listArea().getHeight()), scroll - wheel.deltaY * 120.0f);
    repaint();
}

bool ProgressionBrowser::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        setVisible (false);
        return true;
    }
    const auto list = progressionsOf (category);
    if (list.empty())
        return false;
    const int current = juce::roundToInt (processor.raw (ParamIDs::progression));
    auto it = std::find (list.begin(), list.end(), current);
    const int at = it == list.end() ? -1 : (int) (it - list.begin());
    const int n = (int) list.size();
    if (key == juce::KeyPress::rightKey || key == juce::KeyPress::downKey)
    {
        pick (list[(size_t) ((at + 1) % n)]);
        return true;
    }
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::upKey)
    {
        pick (list[(size_t) ((at - 1 + n) % n)]);
        return true;
    }
    return false;
}

//==============================================================================
void GrooveOverview::refresh()
{
    // What plays in the bars under the playhead (from the start when stopped): variations, fills
    // and the section too, not only the written groove.
    const int len = processor.steps();
    const double ppq = processor.displayPpq();
    const long long first = ppq < 0.0 ? 0 : (long long) std::floor (ppq / fgl::stepLength) / len * len;
    pattern = processor.playedPattern (first, len);
    for (int l = 0; l < fgl::numLanes; ++l)
        muted[(size_t) l] = processor.isLaneMuted (l);
    playStep = processor.displayStep();
    repaint();
}

juce::Rectangle<int> GrooveOverview::stepColumn (int step) const
{
    const int steps = juce::jmax (1, pattern.length());
    const float label = 64.0f, cellW = ((float) getWidth() - label) / (float) steps;
    if (! juce::isPositiveAndBelow (step, steps))
        return {};
    return juce::Rectangle<float> (label + cellW * (float) step, 0.0f, cellW, (float) getHeight()).getSmallestIntegerContainer().expanded (1, 0);
}

void GrooveOverview::setPlayStep (int step)
{
    if (step == playStep)
        return;
    repaint (stepColumn (playStep));
    playStep = step;
    repaint (stepColumn (playStep));
}

void GrooveOverview::paint (juce::Graphics& g)
{
    const int steps = juce::jmax (1, pattern.length());
    const float label = 64.0f, rowH = (float) getHeight() / (float) fgl::numLanes;
    const float cellW = ((float) getWidth() - label) / (float) steps;
    if (juce::isPositiveAndBelow (playStep, steps))
    {
        g.setColour (Palette::accent2.withAlpha (0.14f));
        g.fillRect (label + cellW * (float) playStep, 0.0f, cellW, (float) getHeight());
    }
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        const float y = rowH * (float) l;
        const auto colour = laneColour (l);
        g.setColour (muted[(size_t) l] ? Palette::dim.withAlpha (0.5f) : Palette::text.withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions (juce::jmin (10.5f, rowH * 0.8f)).withStyle ("Bold")));
        g.drawText (tr (fgl::laneShortName (l)), juce::Rectangle<float> (4.0f, y, label - 8.0f, rowH), juce::Justification::centredLeft);
        const auto& line = pattern.lanes[(size_t) l];
        for (int s = 0; s < steps; ++s)
        {
            auto cell = juce::Rectangle<float> (label + cellW * (float) s, y, cellW, rowH).reduced (0.8f, 0.8f);
            g.setColour (Palette::panelEdge.withAlpha ((s / 4) % 2 == 0 ? 0.5f : 0.3f));
            g.fillRect (cell);
            const char c = s < (int) line.size() ? line[(size_t) s] : '.';
            if (fgl::isHit (c))
            {
                g.setColour (colour.withAlpha ((muted[(size_t) l] ? 0.3f : 1.0f) * (c == 'X' ? 1.0f : c == 'g' ? 0.4f : 0.72f)));
                g.fillRect (c == 'g' ? cell.withSizeKeepingCentre (cell.getWidth() * 0.5f, cell.getHeight() * 0.5f) : cell);
            }
        }
    }
    // The parts A-D.
    g.setColour (Palette::dim.withAlpha (0.6f));
    for (int k = 1; k < 4; ++k)
        g.drawVerticalLine (juce::roundToInt (label + cellW * (float) (k * steps / 4)), 0.0f, (float) getHeight());
}

MiniMixer::MiniMixer (FaseGrooveLeadProcessor& p) : processor (p)
{
    Localisation::bind (*this, [this] { repaint(); });   // the lane names
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        auto* f = faders.add (new juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox));
        f->setColour (juce::Slider::trackColourId, laneColour (l));
        f->setColour (juce::Slider::thumbColourId, Palette::text);
        f->setColour (juce::Slider::backgroundColourId, Palette::field);
        f->setDoubleClickReturnValue (true, 0.0);
        f->setPopupDisplayEnabled (true, true, nullptr);
        Localisation::bind (*f, [f, l] { f->setTooltip (tr (fgl::laneName (l)) + ": " + tr ("volume in the guide sound (double-click: 0 dB)")); });
        addAndMakeVisible (f);
        attachments.add (new juce::AudioProcessorValueTreeState::SliderAttachment (processor.apvts, ParamIDs::laneVolume (l), *f));
    }
}

juce::Rectangle<int> MiniMixer::strip (int lane) const
{
    const int w = getWidth() / fgl::numLanes;
    return { lane * w, 0, w, getHeight() };
}

void MiniMixer::resized()
{
    for (int l = 0; l < fgl::numLanes; ++l)
        faders[l]->setBounds (strip (l).reduced (6, 0).withTrimmedBottom (15));
}

void MiniMixer::refresh()
{
    // The labels dim with a lane's mute: redrawn only when one changes.
    std::uint32_t mask = 0;
    for (int l = 0; l < fgl::numLanes; ++l)
        if (processor.isLaneMuted (l))
            mask |= 1u << l;
    if (mask != mutedShown)
    {
        mutedShown = mask;
        repaint();
    }
}

void MiniMixer::paint (juce::Graphics& g)
{
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        auto r = strip (l);
        g.setColour (processor.isLaneMuted (l) ? Palette::dim.withAlpha (0.5f) : Palette::dim);
        g.setFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")));
        g.drawFittedText (tr (fgl::laneShortName (l)), r.removeFromBottom (14), juce::Justification::centred, 1, 0.6f);
    }
}

EditorOverlay::EditorOverlay()
{
    for (juce::Button* b : std::initializer_list<juce::Button*> { &rewindButton, &playButton, &stopButton, &closeButton,
                                                                  &accentBrush, &hitBrush, &ghostBrush, &flamBrush })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    rewindButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    playButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    stopButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    playButton.onColour = Palette::accent2;
    accentBrush.setConnectedEdges (juce::Button::ConnectedOnRight);
    hitBrush.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    ghostBrush.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    flamBrush.setConnectedEdges (juce::Button::ConnectedOnLeft);
    accentBrush.onClick = [this] { setBrush ('X'); };
    hitBrush.onClick    = [this] { setBrush ('x'); };
    ghostBrush.onClick  = [this] { setBrush ('g'); };
    flamBrush.onClick   = [this] { setBrush ('f'); };
    closeButton.onClick = [this] { setVisible (false); };
    Localisation::bind (*this, [this] { refreshText(); });
    setBrush ('x');
}

void EditorOverlay::setBrush (char symbol)
{
    brush = symbol;
    accentBrush.setToggleState (brush == 'X', juce::dontSendNotification);
    hitBrush.setToggleState (brush == 'x', juce::dontSendNotification);
    ghostBrush.setToggleState (brush == 'g', juce::dontSendNotification);
    flamBrush.setToggleState (brush == 'f', juce::dontSendNotification);
    if (onBrush != nullptr)
        onBrush (brush);
}

void EditorOverlay::refreshText()
{
    accentBrush.setButtonText (tr ("ACCENT"));
    hitBrush.setButtonText (tr ("HIT"));
    ghostBrush.setButtonText (tr ("GHOST"));
    flamBrush.setButtonText (tr ("REPEAT"));
    const auto tip = tr ("What a click on an empty step draws (Shift: accent, Alt: ghost, Ctrl: rake work too)");
    for (auto* b : { &accentBrush, &hitBrush, &ghostBrush, &flamBrush })
        b->setTooltip (tip);
    rewindButton.setTooltip (tr ("Back to the start"));
    playButton.setTooltip (tr ("Play / pause the groove without the DAW (space bar)"));
    stopButton.setTooltip (tr ("Stop and go back to the start"));
    closeButton.setTooltip (tr ("CLOSE"));
    repaint();
}

void EditorOverlay::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background.withAlpha (0.97f));
    const auto card = getLocalBounds().toFloat().reduced (10.0f);
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (card, 10.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (card.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (Palette::accent);
    g.fillRoundedRectangle (28.0f, 30.0f, 3.0f, 14.0f, 1.0f);
    g.setColour (Palette::text);
    g.setFont (juce::Font (juce::FontOptions (15.0f).withStyle ("Bold")));
    g.drawText (Localisation::upper (tr ("Edit groove")), 38, 22, 260, 30, juce::Justification::centredLeft);

    g.setColour (Palette::dim);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    auto caption = [&g] (const juce::String& text, int x, int w)
    {
        g.drawFittedText (Localisation::upper (tr (text)), { x, 58, w, 14 }, juce::Justification::centredLeft, 1, 0.7f);
    };
    caption ("Transport", 28, 120);
    caption ("Brush", 174, 120);
    caption ("View", 476, 120);
    caption ("New rhythm", 784, 90);
    caption ("Groove", 874, 90);
    caption (viewMode != nullptr && viewMode() == 2 ? "Feel" : "Template", 1062, 96);
}

//==============================================================================
FaseGrooveLeadEditor::FaseGrooveLeadEditor (FaseGrooveLeadProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      grooveCombo (p.apvts, ParamIDs::pattern, "Groove"),
      genreCombo (p.apvts, ParamIDs::genre, "Groove genre"),
      partLengthCombo (p.apvts, ParamIDs::partLength, "Part length"),
      chordPanel (p),
      sectionCombo (p.apvts, ParamIDs::section, "Song section"),
      fillsCombo (p.apvts, ParamIDs::fills, "Fills"),
      variationCombo (p.apvts, ParamIDs::variation, "Variation"),
      fillStyleCombo (p.apvts, ParamIDs::fillStyle, "Fill style"),
      swing (p.apvts, ParamIDs::swing, "Swing"),
      humanize (p.apvts, ParamIDs::humanize, "Humanize"),
      velocity (p.apvts, ParamIDs::velocity, "Velocity"),
      accents (p.apvts, ParamIDs::accents, "Accents"),
      kitCombo (p.apvts, ParamIDs::kit, "Lead"),
      channelCombo (p.apvts, ParamIDs::midiChannel, "MIDI ch."),
      tone (p.apvts, ParamIDs::tone, "Tone"),
      reverb (p.apvts, ParamIDs::reverb, "Reverb"),
      volume (p.apvts, ParamIDs::volume, "Volume"),
      grid (p), velocityView (p), mixerView (p), fillView (p)
{
    Localisation::loadSaved();
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);
    content.setWantsKeyboardFocus (true);

    auto show = [this] (juce::Component& c) { content.addAndMakeVisible (c); };

    // ---- Header: presets, total random, undo / redo, full screen; style, reset, options ----
    Localisation::bindTip (presetsButton, "Your presets: save everything, load it in any project");
    presetsButton.onClick = [this] { showPresetsMenu(); };
    Localisation::bindText (totalRandomButton, "TOTAL");
    totalRandomButton.iconColour = Palette::accent;
    Localisation::bindTip (totalRandomButton, "Total random: style, groove or new groove, chords, swing, feel, fills and sound all at once (undo with the arrow)");
    totalRandomButton.onClick = [this] { processor.totalRandom(); };
    Localisation::bindTip (undoButton, "Undo (Ctrl+Z)");
    undoButton.onClick = [this] { undo(); };
    undoButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    Localisation::bindTip (redoButton, "Redo (Ctrl+Shift+Z)");
    redoButton.onClick = [this] { redo(); };
    redoButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    Localisation::bindTip (fullScreenButton, "Full screen");
    fullScreenButton.onClick = [this] { toggleFullScreen(); };
    Localisation::bindTip (styleButton, "A groove, its chords, swing, feel, fills and sound for a genre, in one click");
    styleButton.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addSectionHeader (tr ("One-click setups by genre"));
        const auto names = FaseGrooveLeadProcessor::styleNames();
        for (int i = 0; i < names.size(); ++i)
            menu.addItem (i + 1, names[i]);
        juce::Component::SafePointer<FaseGrooveLeadEditor> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&styleButton), [safe] (int result)
        {
            if (safe != nullptr && result > 0)
                safe->processor.applyStyle (result - 1);
        });
    };
    Localisation::bindTip (resetButton, "Every setting back to its default (undo with the arrow)");
    resetButton.onClick = [this] { processor.resetToDefaults(); };
    Localisation::bindText (optionsButton, "OPTIONS");
    Localisation::bindTip (optionsButton, "Language, interface size, tips and credits");
    optionsButton.onClick = [this] { options.show (false, currentScale()); };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &presetsButton, &totalRandomButton, &undoButton, &redoButton,
                                                                        &fullScreenButton, &styleButton, &resetButton, &optionsButton })
        show (*c);

    // ---- Transport & tempo ----
    Localisation::bindTip (rewindButton, "Back to the start");
    rewindButton.onClick = [this] { processor.previewToStart(); };
    rewindButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    Localisation::bindTip (playButton, "Play / pause the groove without the DAW (space bar)");
    playButton.onClick = [this] { processor.previewToggle(); };
    playButton.onColour = Palette::accent2;
    playButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    Localisation::bindTip (stopButton, "Stop and go back to the start");
    stopButton.onClick = [this] { processor.previewStop(); };
    stopButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    Localisation::bindTip (sequencerToggle, "Off: the groove doesn't play with the DAW (e.g. while the dragged clip plays on this track). The play button still previews");
    sequencerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::sequencer, sequencerToggle);
    syncButton.setClickingTogglesState (true);
    Localisation::bindTip (syncButton, "On: the DAW's tempo and position. Off: your own tempo (still started by the DAW's play)");
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::tempoSync, syncButton);
    tempoSlider.setSliderStyle (juce::Slider::LinearBar);
    Localisation::bindTip (tempoSlider, "Tempo when SYNC DAW is off (drag or double-click to type)");
    tempoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ParamIDs::tempo, tempoSlider);
    dawTempo.setColour (juce::Label::backgroundColourId, Palette::background);
    dawTempo.setColour (juce::Label::textColourId, Palette::text);
    dawTempo.setJustificationType (juce::Justification::centred);
    dawTempo.setFont (font (15.0f, true));
    for (juce::Component* c : std::initializer_list<juce::Component*> { &rewindButton, &playButton, &stopButton, &sequencerToggle, &syncButton, &tempoSlider, &dawTempo })
        show (*c);

    // ---- Groove row: presets, random, genre, density and the parts A-D ----
    {
        auto& box = grooveCombo.comboBox();
        box.clear (juce::dontSendNotification);
        const auto& presets = fgl::leadPatterns();
        for (const auto* category : fgl::grooveCategories())
        {
            box.addSectionHeading (category);
            for (size_t i = 0; i < presets.size(); ++i)
                if (presets[i].category == category)
                    box.addItem (presets[i].name, (int) i + 1);
        }
        box.addSectionHeading (tr ("Your grooves"));
        box.addItem (tr ("Random groove"), fgl::randomPatternIndex() + 1);
        box.addItem (tr ("User pattern (edited on the grid)"), fgl::userPatternIndex() + 1);
        grooveCombo.refresh();
    }
    grooveCombo.setTip ("The groove, by genre (the same genres as Fase Groove Piano and Drums). Edit any groove on the grid");
    Localisation::bindTip (prevGroove, "Previous groove");
    prevGroove.onClick = [this] { stepGroove (-1); };
    prevGroove.setConnectedEdges (juce::Button::ConnectedOnRight);
    Localisation::bindTip (nextGroove, "Next groove");
    nextGroove.onClick = [this] { stepGroove (1); };
    nextGroove.setConnectedEdges (juce::Button::ConnectedOnLeft);
    genreCombo.setTip ("The genre the dice write in");
    Localisation::bindText (newBeatButton, "RANDOM");
    newBeatButton.iconColour = Palette::accent;
    Localisation::bindTip (newBeatButton, "A brand new random groove in the chosen genre (locked parts stay)");
    newBeatButton.onClick = [this] { processor.rollNewBeat(); };
    partLengthCombo.setTip ("How long each part A-D is: half a bar (a 2-bar groove) or one bar (a 4-bar groove)");
    densitySlider.setSliderStyle (juce::Slider::LinearBar);
    Localisation::bindTip (densitySlider, "Melody density: from sparse hooks to busy sixteenth-note phrases");
    densityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ParamIDs::density, densitySlider);
    Localisation::bindTip (grooveButton, "The groove: click to open the browser (genres, grooves and the picture of their rhythm)");
    grooveButton.onClick = [this] { browser.show(); };
    linkButton.setClickingTogglesState (true);
    linkButton.setColour (juce::TextButton::buttonOnColourId, Palette::linked);
    Localisation::bindTip (linkButton, "Link with Fase Groove Piano and Drums: the same genre, swing, tempo and play button in all of them, on the same beat (change it in any one)");
    linkButton.onClick = [this] { processor.setLinked (linkButton.getToggleState()); };

    keyLinkButton.setClickingTogglesState (true);
    keyLinkButton.setColour (juce::TextButton::buttonOnColourId, Palette::linked);
    Localisation::bindTip (keyLinkButton, "Same key (with LINK on): the key and the scale stay the same in Fase Groove Piano and Lead (change them in either one)");
    keyLinkButton.onClick = [this] { processor.setKeyLinked (keyLinkButton.getToggleState()); };
    show (keyLinkButton);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &grooveButton, &prevGroove, &nextGroove, &genreCombo, &newBeatButton, &densitySlider,
                                                                        &partLengthCombo, &linkButton })
        show (*c);
    for (int i = 0; i < fgl::numSegments; ++i)
    {
        partCards[i] = std::make_unique<PartCard> (p, i);
        show (*partCards[i]);
    }

    // ---- Lead part: the lane picked ----
    juce::StringArray laneNames;
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        laneBox.addItem (fgl::laneName (l), l + 1);
        laneNames.add (fgl::laneName (l));
    }
    Localisation::bindItems (laneBox, laneNames);
    Localisation::bindTip (laneBox, "The lane you edit here (or click its name on the grid)");
    laneBox.onChange = [this] { pickedFromMenu (laneBox); selectLane (laneBox.getSelectedId() - 1); };
    for (int o = -2; o <= 2; ++o)
        octaveBox.addItem (o == 0 ? juce::String ("0") : (o > 0 ? "+" : "") + juce::String (o), o + 3);
    for (int o = -2; o <= 2; ++o)
        globalOctaveBox.addItem ((o > 0 ? "+" : "") + juce::String (o), o + 3);
    Localisation::bindTip (globalOctaveBox, "Transpose every lead lane by octaves, keeping its own octave offset");
    globalOctaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ParamIDs::globalOctave, globalOctaveBox);
    show (globalOctaveBox);
    Localisation::bindTip (octaveBox, "This lane up or down by octaves (the voicing keeps its shape)");
    octaveBox.onChange = [this]
    {
        pickedFromMenu (octaveBox);
        if (const int id = octaveBox.getSelectedId(); id > 0)
            processor.setParameterFromUI (ParamIDs::octave (selectedLane), (float) (id - 3));
    };
    laneDiceButton.iconColour = Palette::accent;
    Localisation::bindTip (laneDiceButton, "A new line for this lane only (locked parts stay)");
    laneDiceButton.onClick = [this] { processor.randomizeLane (selectedLane); };
    muteButton.setClickingTogglesState (false);
    Localisation::bindTip (muteButton, "Mute this lane (MIDI out, guide lead and clips)");
    muteButton.onClick = [this] { processor.setLaneMuted (selectedLane, ! processor.isLaneMuted (selectedLane)); };
    Localisation::bindTip (hearButton, "Hear this lane over the chord of the bar");
    hearButton.onClick = [this] { processor.audition (selectedLane, 105); };
    levelKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    levelKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    levelKnob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    Localisation::bindTip (levelKnob, "This lane's LEVEL: its MIDI velocities (and so the clips) go up or down");
    volumeKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    volumeKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
    volumeKnob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    volumeKnob.setDoubleClickReturnValue (true, 0.0);
    Localisation::bindTip (volumeKnob, "This lane's VOLUME in the guide sound; the MIDI doesn't change. All of them: MIXER");
    for (juce::Component* c : std::initializer_list<juce::Component*> { &laneBox, &octaveBox, &laneDiceButton, &muteButton, &hearButton,
                                                                        &levelKnob, &volumeKnob })
        show (*c);

    // ---- Chords ----
    show (chordPanel);

    // ---- Groove panel ----
    sectionCombo.setTip ("Song section: sparse intro and outro, sustained break, rising build-up, full melody on the drop");
    fillsCombo.setTip ("A fill at the end of every N bars");
    fillStyleCombo.setTip ("What the fill plays. Mix: a different one now and then");
    variationCombo.setTip ("Every 4 or 8 bars the last bar plays a variation (a slide, a pick-up, a lick...)");
    swing.setTip ("Pushes every other 16th later: 50% straight, 67% triplet shuffle, 75% the heaviest. Each lane's share: MIXER");
    humanize.setTip ("Like a soloist: small timing and velocity changes, and the soft notes come and go over the four bars. 0%: exactly as written");
    velocity.setTip ("Overall velocity of the hits");
    accents.setTip ("Melodic accents: emphasise offbeats and soften passing notes (0%: flat)");
    resolveNoteButton.setClickingTogglesState (true);
    Localisation::bindTip (resolveNoteButton, "Resolve the melody on the bar after a fill");
    resolveNoteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::resolveNote, resolveNoteButton);
    Localisation::bindTip (newFeelButton, "Another humanization (the same every time you play it)");
    newFeelButton.onClick = [this] { processor.newFeel(); };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &sectionCombo, &fillsCombo, &variationCombo, &fillStyleCombo,
                                                                        &swing, &humanize, &velocity, &accents, &resolveNoteButton, &newFeelButton })
        show (*c);

    // ---- Guide sound & MIDI out ----
    Localisation::bindTip (soundToggle, "The guide lead: hear the groove here. Off: MIDI only, for your own lead instrument");
    soundAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamIDs::sound, soundToggle);
    kitCombo.setTip ("The guide lead's sound");
    channelCombo.setTip ("The MIDI channel of the notes");
    attackSlider.setSliderStyle (juce::Slider::LinearBar);
    Localisation::bindTip (attackSlider, "Phrase attack delay in milliseconds. Also sets the length of approach notes; exported in MIDI");
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ParamIDs::attackDelay, attackSlider);
    tone.setTip ("The lead's tone knob: darker or brighter");
    Localisation::bindTip (playingButton, "Lead performance: held hooks, upper register, anticipated harmony and articulation keyswitches");
    playingButton.onClick = [this] { showPlayingMenu(); };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &soundToggle, &kitCombo, &channelCombo, &attackSlider, &tone, &reverb, &volume, &playingButton })
        show (*c);

    // ---- Edit: step grid / velocity ----
    gridTab.setConnectedEdges (juce::Button::ConnectedOnRight);
    velocityTab.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    mixerTab.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    fillTab.setConnectedEdges (juce::Button::ConnectedOnLeft);
    Localisation::bindTip (mixerTab, "Every lane: volume, mute, push / pull and its share of the swing");
    Localisation::bindTip (fillTab, "Draw your own fill (Fill style: Custom)");
    fillTab.onClick = [this] { setView (3); };
    Localisation::bindTip (templateButton, "Groove template: the timing and accents of an audio loop or a MIDI file, on any groove");
    templateButton.onClick = [this] { showTemplateMenu(); };
    Localisation::bindTip (timingButton, "Ready-made feels: which lanes sit ahead of or behind the beat, which ones swing");
    timingButton.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addSectionHeader (tr ("Timing of the lanes"));
        const auto names = FaseGrooveLeadProcessor::timingFeelNames();
        for (int i = 0; i < names.size(); ++i)
            menu.addItem (i + 1, tr (names[i]));
        juce::Component::SafePointer<FaseGrooveLeadEditor> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&timingButton), [safe] (int result)
        {
            if (safe != nullptr && result > 0)
                safe->processor.applyTimingFeel (result - 1);
        });
    };
    mixerTab.onClick = [this] { setView (2); };
    gridTab.onClick = [this] { setView (0); };
    Localisation::bindTip (velocityTab, "The velocity of every hit, one lane at a time (the dynamics of the melody)");
    velocityTab.onClick = [this] { setView (1); };
    Localisation::bindText (newRhythmButton, "GROOVE");
    newRhythmButton.iconColour = Palette::accent;
    Localisation::bindTip (newRhythmButton, "A brand new random groove (locked parts stay)");
    newRhythmButton.onClick = [this] { processor.rollNewBeat(); };
    Localisation::bindTip (myGroovesButton, "Save the grid as your own groove, or load one");
    myGroovesButton.onClick = [this] { showGroovesMenu(); };
    Localisation::bindText (importButton, "IMPORT...");
    Localisation::bindTip (importButton, "A MIDI file (a lead part, chords, a riff): its rhythm into the grid, its chords into the slots (or drop the file on the window)");
    importButton.onClick = [this] { importMidi(); };
    Localisation::bindTip (clearButton, "Empty the grid and start your own groove");
    clearButton.onClick = [this] { processor.clearGrid(); grid.refresh(); velocityView.refresh(); };
    grid.onShowVelocity = [this] (int lane) { selectLane (lane); setView (1); };
    grid.onSelectLane = [this] (int lane) { selectLane (lane); };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &gridTab, &velocityTab, &mixerTab, &fillTab, &newRhythmButton, &myGroovesButton,
                                                                        &importButton, &templateButton, &clearButton, &grid })
        editOverlay.addAndMakeVisible (*c);
    editOverlay.addChildComponent (velocityView);
    editOverlay.addChildComponent (mixerView);
    editOverlay.addChildComponent (fillView);
    editOverlay.addChildComponent (timingButton);

    // The main window: the groove at a glance (a click opens the editor) and the volume faders.
    overview.onOpen = [this] { openEditor(); };
    Localisation::bind (overview, [this] { overview.setTooltip (tr ("Click: open the editor")); });
    Localisation::bind (openEditorButton, [this]
    {
        openEditorButton.setButtonText (tr ("EDITOR..."));
        openEditorButton.setTooltip (tr ("The big editor: step grid, velocity, mixer and fill, to edit everything (or click the groove)"));
    });
    openEditorButton.onClick = [this] { openEditor(); };
    show (overview);
    content.addChildComponent (miniMixer);
    show (openEditorButton);
    mainGrooveTab.setConnectedEdges (juce::Button::ConnectedOnRight);
    mainVolumeTab.setConnectedEdges (juce::Button::ConnectedOnLeft);
    mainGrooveTab.onClick = [this] { setMainTab (false); };
    mainVolumeTab.onClick = [this] { setMainTab (true); };
    mainNewRhythm.iconColour = Palette::accent;
    mainNewRhythm.onClick = [this] { if (newRhythmButton.onClick) newRhythmButton.onClick(); };
    mainMyGrooves.onClick = [this] { if (myGroovesButton.onClick) myGroovesButton.onClick(); };
    mainImport.onClick    = [this] { if (importButton.onClick) importButton.onClick(); };
    mainTemplate.onClick  = [this] { if (templateButton.onClick) templateButton.onClick(); };
    mainTemplate.setColour (juce::TextButton::buttonOnColourId, Palette::accent.darker (0.35f));
    templateButton.setColour (juce::TextButton::buttonOnColourId, Palette::accent.darker (0.35f));
    mainClear.onClick     = [this] { if (clearButton.onClick) clearButton.onClick(); };
    Localisation::bind (mainGrooveTab, [this]
    {
        mainGrooveTab.setButtonText (tr ("GROOVE"));
        mainVolumeTab.setButtonText (tr ("VOLUMES"));
        mainVolumeTab.setTooltip (tr ("The volume of each one in the guide sound"));
        mainNewRhythm.setButtonText (tr ("GROOVE"));
        mainNewRhythm.setTooltip (newRhythmButton.getTooltip());
        mainMyGrooves.setButtonText (tr ("MY GROOVES"));
        mainMyGrooves.setTooltip (myGroovesButton.getTooltip());
        mainImport.setButtonText (tr ("IMPORT..."));
        mainImport.setTooltip (importButton.getTooltip());
        mainTemplate.setButtonText (tr ("TEMPLATE..."));
        mainTemplate.setTooltip (templateButton.getTooltip());
        mainClear.setButtonText (tr ("CLEAR"));
        mainClear.setTooltip (clearButton.getTooltip());
    });
    for (juce::Component* m : std::initializer_list<juce::Component*> { &mainGrooveTab, &mainVolumeTab, &mainNewRhythm, &mainMyGrooves, &mainImport,
                                                                        &mainTemplate, &mainClear })
        show (*m);
    setMainTab (false);
    editOverlay.viewMode = [this] { return viewMode; };
    editOverlay.onBrush = [this] (char b) { grid.brush = b; fillView.brush = b; };
    editOverlay.rewindButton.onClick = [this] { processor.previewToStart(); };
    editOverlay.playButton.onClick = [this] { processor.previewToggle(); };
    editOverlay.stopButton.onClick = [this] { processor.previewStop(); };

    // ---- Export MIDI ----
    notesBox.addItem ("All lanes", 1);
    notesBox.addItem ("One track per lane", 2);
    Localisation::bindItems (notesBox, { "All lanes", "One track per lane" });
    notesBox.setSelectedId (1, juce::dontSendNotification);
    Localisation::bindTip (notesBox, "One lead track, or separate MIDI tracks for each phrase lane");
    const char* lengths[] { "Full loop", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars", "Whole song" };
    juce::StringArray lengthNames;
    for (int i = 0; i < (int) std::size (lengths); ++i)
    {
        lengthBox.addItem (lengths[i], i + 1);
        lengthNames.add (lengths[i]);
    }
    Localisation::bindItems (lengthBox, lengthNames);
    lengthBox.setSelectedId (1, juce::dontSendNotification);
    Localisation::bindTip (lengthBox, "Length of the clip. Full loop: until groove, fills and variations come round. "
                                      "Whole song: the sections of the song window, with markers");
    dragTile.filled = true;   // the orange tile of Fase Groove Piano, at the same size
    dragTile.createFiles = [this]
    {
        const auto file = processor.writeMidiClip (selectedClipBars(), selectedClipPart());
        return file.existsAsFile() ? juce::StringArray (file.getFullPathName()) : juce::StringArray();
    };
    Localisation::bindTip (saveButton, "Save the clip as a .mid file");
    saveButton.onClick = [this] { saveMidiFile(); };
    Localisation::bindTip (songButton, "The song window: bars, variation and fill of each section; drag or save the whole song or each section");
    songButton.onClick = [this] { songOverlay.show(); };
    Localisation::bindTip (wavButton, "The guide sound as audio (WAV): this clip, the whole song, or one file per lane");
    wavButton.onClick = [this] { showAudioExportMenu(); };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &notesBox, &lengthBox, &dragTile, &saveButton, &wavButton, &songButton })
        show (*c);

    content.addChildComponent (editOverlay);
    content.addChildComponent (songOverlay);
    content.addChildComponent (browser);
    content.addChildComponent (progressionBrowser);
    chordPanel.onBrowse = [this] { progressionBrowser.show(); };
    content.addChildComponent (options);
    content.addChildComponent (updateOverlay);

    // OPTIONS, as in Fase Groove Piano.
    options.onScaleChosen = [this] (float newScale)
    {
        setSize (juce::roundToInt (designWidth * newScale), juce::roundToInt (designHeight * newScale));
    };
    options.onTipsChanged = [this] (bool on)
    {
        Localisation::setTipsEnabled (on);
        applyTips();
    };
    applyTips();

    // Automatic updates, as in Fase Groove Piano.
    options.onCheckUpdates = [this]
    {
        options.checkUpdatesButton.setEnabled (false);
        options.checkUpdatesButton.setButtonText (tr ("CHECKING..."));
        updateChecker.start (true);
    };
    updateChecker.onResult = [this] (const Updater::Result& result, bool manual)
    {
        options.checkUpdatesButton.setEnabled (true);
        options.refreshText();
        if (result.status == Updater::Result::Status::ready)
        {
            options.setVisible (false);
            updateOverlay.show (result);
        }
        else if (manual)
        {
            const bool upToDate = result.status == Updater::Result::Status::upToDate;
            juce::AlertWindow::showMessageBoxAsync (upToDate ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                                                    "Fase Groove Lead",
                                                    upToDate ? tr ("You have the latest version.") + " (" + juce::String (JucePlugin_VersionString) + ")"
                                                             : tr ("Could not check for updates. Check your internet connection and try again."));
        }
    };
    Updater::cleanupAfterUpdate();
    if (Updater::autoCheckEnabled() && Updater::firstAutoCheckThisSession())
        updateChecker.start (false);

    combos = { &partLengthCombo, &genreCombo, &sectionCombo, &fillsCombo, &variationCombo, &fillStyleCombo, &kitCombo, &channelCombo };

    lastSnapshot = processor.snapshot();
    // Texts that follow the interface language.
    for (auto [b, english] : std::initializer_list<std::pair<juce::Button*, const char*>> {
             { &presetsButton, "PRESETS" }, { &styleButton, "STYLE" }, { &resetButton, "RESET" }, { &syncButton, "SYNC DAW" },
             { &sequencerToggle, "Sequencer" }, { &soundToggle, "Guide sound" }, { &hearButton, "HEAR IT" },
             { &muteButton, "MUTE" }, { &resolveNoteButton, "RESOLVE" }, { &newFeelButton, "NEW FEEL" },
             { &gridTab, "STEP GRID" }, { &velocityTab, "VELOCITY" }, { &mixerTab, "MIXER" }, { &clearButton, "CLEAR" },
             { &myGroovesButton, "MY GROOVES" }, { &saveButton, "SAVE .MID..." }, { &songButton, "FULL SONG..." },
             { &fillTab, "FILL" }, { &templateButton, "TEMPLATE..." }, { &timingButton, "TIMING..." }, { &wavButton, "SAVE .WAV..." },
             { &linkButton, "LINK" }, { &keyLinkButton, "KEY" }, { &playingButton, "PLAYING..." } })
        Localisation::bindText (*b, english);
    Localisation::bind (content, [this] { content.repaint(); options.refreshText(); translateHostWindow(); });

    selectLane ((int) fgl::Lane::hook);
    setView (0);
    timerCallback();   // the groove's name, the parts and the lights from the start

    // The first time: as big as the screen comfortably allows (bigger buttons and text).
    float scale = processor.editorScale;
    if (scale <= 0.0f)
    {
        scale = 1.0f;
        if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            scale = juce::jmin ((float) display->userArea.getWidth() * 0.9f / (float) designWidth,
                                (float) display->userArea.getHeight() * 0.88f / (float) designHeight);
    }
    scale = juce::jlimit (0.6f, 1.25f, scale);
    setResizable (true, true);
    setResizeLimits ((int) (designWidth * 0.55f), (int) (designHeight * 0.55f), designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (juce::roundToInt (designWidth * scale), juce::roundToInt (designHeight * scale));

    addMouseListener (this, true);   // the undo history: see mouseUp
    startTimerHz (30);
    playTimer.startTimerHz (60);
}

FaseGrooveLeadEditor::~FaseGrooveLeadEditor()
{
    setLookAndFeel (nullptr);
}

float FaseGrooveLeadEditor::currentScale() const
{
    return juce::jmax (0.3f, juce::jmin ((float) getWidth() / (float) designWidth, (float) getHeight() / (float) designHeight));
}

void FaseGrooveLeadEditor::applyTips()
{
    // Tips appear when the mouse rests on a control; they can be switched off in OPTIONS.
    if (Localisation::tipsEnabled())
    {
        if (tooltips == nullptr)
            tooltips = std::make_unique<juce::TooltipWindow> (this, 600);
    }
    else
    {
        tooltips.reset();
    }
}

void FaseGrooveLeadEditor::parentHierarchyChanged()
{
    translateHostWindow();
}

void FaseGrooveLeadEditor::translateHostWindow()
{
    // The Standalone app's window (Options button, feedback warning) has a few fixed English
    // texts; its menus and the audio settings follow the language through JUCE's TRANS().
    if (processor.wrapperType != juce::AudioProcessor::wrapperType_Standalone)
        return;

    auto* top = getTopLevelComponent();
    if (top == nullptr || top == this)
        return;

    static const char* const fixedTexts[] { "Options", "Mute audio input", "Feedback Loop:", "Audio input is muted to avoid feedback loop" };

    std::function<void (juce::Component&)> visit = [&] (juce::Component& c)
    {
        if (&c == this)
            return;

        auto& props = c.getProperties();
        auto translate = [&] (const juce::String& current, auto&& apply)
        {
            if (! props.contains ("fglEnglish"))
                for (auto* t : fixedTexts)
                    if (current == t)
                        props.set ("fglEnglish", t);

            if (props.contains ("fglEnglish"))
                apply (tr (props["fglEnglish"].toString()));
        };

        if (auto* b = dynamic_cast<juce::Button*> (&c))
            translate (b->getButtonText(), [b] (const juce::String& t) { b->setButtonText (t); });
        else if (auto* l = dynamic_cast<juce::Label*> (&c))
            translate (l->getText(), [l] (const juce::String& t) { l->setText (t, juce::dontSendNotification); });

        for (auto* child : c.getChildren())
            visit (*child);
    };

    visit (*top);
}

void FaseGrooveLeadEditor::MiniStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (Palette::accent);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Palette::background);
    g.setFont (font (12.0f, true));
    g.drawFittedText (tr ("Drop the clip on a track"), r.reduced (8.0f, 2.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
}

void FaseGrooveLeadEditor::enterMiniMode()
{
    if (miniMode || isFullScreen())
        return;
    miniMode = true;
    miniStartedAt = juce::Time::getMillisecondCounter();
    fullSize = { getWidth(), getHeight() };
    content.setAlpha (0.0f);   // still there (the drag belongs to its tile), not seen
    addAndMakeVisible (miniStrip);
    miniStrip.toFront (false);
    getConstrainer()->setFixedAspectRatio (0.0);
    setResizeLimits (120, 20, designWidth * 2, designHeight * 2);
    setSize (240, 28);   // the strip of Fase Groove Piano
}

void FaseGrooveLeadEditor::exitMiniMode()
{
    if (! miniMode)
        return;
    miniMode = false;
    miniStrip.setVisible (false);
    content.setAlpha (1.0f);
    setResizeLimits ((int) (designWidth * 0.55f), (int) (designHeight * 0.55f), designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (fullSize.x, fullSize.y);
}

void FaseGrooveLeadEditor::resized()
{
    if (miniMode)
    {
        miniStrip.setBounds (getLocalBounds());
        return;
    }
    // Full screen can have another shape: the interface keeps its proportions, centred.
    const float scale = juce::jmax (0.3f, juce::jmin ((float) getWidth() / (float) designWidth, (float) getHeight() / (float) designHeight));
    if (! isFullScreen())
        processor.editorScale = scale;
    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale (scale).translated (((float) getWidth() - designWidth * scale) * 0.5f,
                                                                         ((float) getHeight() - designHeight * scale) * 0.5f));
}

bool FaseGrooveLeadEditor::isFullScreen() const
{
    if (auto* window = findParentComponentOfClass<juce::ResizableWindow>())
        if (window->isFullScreen())
            return true;
    return ! sizeBeforeFullScreen.isEmpty();
}

void FaseGrooveLeadEditor::toggleFullScreen()
{
    // Full screen = the screen's usable area (the taskbar stays visible). Standalone app: its
    // window fills that area. In a DAW (the DAW owns the window) the plug-in grows to fit in it.
    auto* window = findParentComponentOfClass<juce::ResizableWindow>();
    if (! sizeBeforeFullScreen.isEmpty())
    {
        getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);
        if (window != nullptr)
            window->setBounds (sizeBeforeFullScreen);
        else
            setSize (sizeBeforeFullScreen.getWidth(), sizeBeforeFullScreen.getHeight());
        sizeBeforeFullScreen = {};
        return;
    }
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
    {
        if (window != nullptr)
        {
            sizeBeforeFullScreen = window->getBounds();
            getConstrainer()->setFixedAspectRatio (0.0);
            window->setBounds (display->userArea);
            return;
        }
        sizeBeforeFullScreen = getBounds();
        const auto area = display->userArea.reduced (8).withTrimmedTop (30);
        const float scale = juce::jmin ((float) area.getWidth() / (float) designWidth, (float) area.getHeight() / (float) designHeight, 2.0f);
        setSize (juce::roundToInt (designWidth * scale), juce::roundToInt (designHeight * scale));
    }
}

void FaseGrooveLeadEditor::Content::resized()
{
    editor.layout();
}

void FaseGrooveLeadEditor::Content::paint (juce::Graphics& g)
{
    editor.paintContent (g);
}

void FaseGrooveLeadEditor::Content::mouseDown (const juce::MouseEvent& e)
{
    if (editor.logoArea.contains (e.getPosition()))
    {
        editor.options.show (true, editor.currentScale());
    }
    grabKeyboardFocus();
}

bool FaseGrooveLeadEditor::Content::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (key == juce::KeyPress::spaceKey)
    {
        editor.processor.previewToggle();
        return true;
    }
    if (mods.isCommandDown() && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z'))
    {
        mods.isShiftDown() ? editor.redo() : editor.undo();
        return true;
    }
    if (mods.isCommandDown() && (key.getKeyCode() == 'Y' || key.getKeyCode() == 'y'))
    {
        editor.redo();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        editor.options.setVisible (false);
        editor.songOverlay.setVisible (false);
        editor.browser.setVisible (false);
        editor.progressionBrowser.setVisible (false);
        editor.editOverlay.setVisible (false);
        return true;
    }
    return false;
}

void FaseGrooveLeadEditor::layout()
{
    // Header: logo | presets, total, undo / redo, full screen / style, reset, options | transport & tempo | display
    // The same places and sizes as in Fase Groove Piano.
    logoArea = { 20, 18, 238, 54 };
    presetsButton.setBounds (282, 22, 46, 26);
    totalRandomButton.setBounds (332, 22, 84, 26);
    undoButton.setBounds (422, 22, 30, 26);
    redoButton.setBounds (452, 22, 30, 26);
    fullScreenButton.setBounds (486, 22, 30, 26);
    styleButton.setBounds (282, 60, 66, 26);
    resetButton.setBounds (352, 60, 60, 26);
    optionsButton.setBounds (416, 60, 100, 26);

    tempoArea = { 520, 18, 390, 68 };
    sequencerToggle.setBounds (794, 24, 104, 18);
    linkButton.setBounds (590, 24, 66, 18);
    keyLinkButton.setBounds (660, 24, 130, 18);   // KEY and the key itself, beside Sequencer
    rewindButton.setBounds (532, 48, 34, 30);
    playButton.setBounds (566, 48, 40, 30);
    stopButton.setBounds (606, 48, 34, 30);
    syncButton.setBounds (652, 48, 86, 30);
    tempoSlider.setBounds (744, 48, 154, 30);
    dawTempo.setBounds (tempoSlider.getBounds());
    displayArea = { 920, 18, 240, 68 };   // (narrower: room for the key on KEY)

    // Groove row (20, 100, 1142, 120)
    grooveButton.setBounds (34, 135, 204, 30);
    prevGroove.setBounds (240, 135, 30, 30);
    nextGroove.setBounds (270, 135, 30, 30);
    genreCombo.setBounds (34, 170, 150, 44);
    newBeatButton.setBounds (190, 184, 110, 30);
    densitySlider.setBounds (310, 184, 104, 30);
    partLengthCombo.setBounds (310, 120, 104, 44);
    for (int i = 0; i < fgl::numSegments; ++i)
        partCards[i]->setBounds (430 + i * 182, 118, 174, 96);

    // Lead part (20, 230, 290, 186)
    laneBox.setBounds (34, 268, 164, 28);
    laneDiceButton.setBounds (202, 268, 34, 28);
    muteButton.setBounds (240, 268, 56, 28);
    octaveBox.setBounds (34, 318, 70, 26);
    globalOctaveBox.setBounds (174, 378, 122, 26);
    hearButton.setBounds (108, 318, 72, 26);
    levelKnob.setBounds (184, 314, 56, 58);
    volumeKnob.setBounds (242, 314, 58, 58);

    // Chords (320, 230, 260, 186)
    chordPanel.setBounds (326, 258, 248, 150);

    // Groove panel (590, 230, 300, 186)
    newFeelButton.setBounds (730, 234, 84, 24);
    resolveNoteButton.setBounds (818, 234, 62, 24);
    sectionCombo.setBounds (604, 256, 134, 40);
    fillsCombo.setBounds (744, 256, 134, 40);
    variationCombo.setBounds (604, 298, 134, 40);
    fillStyleCombo.setBounds (744, 298, 134, 40);
    swing.setBounds (600, 340, 70, 72);
    humanize.setBounds (670, 340, 70, 72);
    velocity.setBounds (740, 340, 70, 72);
    accents.setBounds (810, 340, 70, 72);

    // Guide sound & MIDI out (900, 230, 262, 186)
    soundToggle.setBounds (914, 272, 116, 24);
    kitCombo.setBounds (1034, 256, 116, 40);
    channelCombo.setBounds (914, 298, 100, 40);
    attackSlider.setBounds (1034, 314, 116, 24);
    playingButton.setBounds (1070, 234, 80, 20);
    tone.setBounds (914, 340, 76, 72);
    reverb.setBounds (994, 340, 76, 72);
    volume.setBounds (1074, 340, 76, 72);

    // Edit (20, 426, 830, 424): the biggest part of the window
    // The groove at a glance and the faders (20, 426, 830, 276); the editor over the window.
    openEditorButton.setBounds (34, 450, 112, 30);
    mainGrooveTab.setBounds (156, 450, 84, 30);
    mainVolumeTab.setBounds (240, 450, 84, 30);
    mainNewRhythm.setBounds (338, 450, 86, 30);
    mainMyGrooves.setBounds (432, 450, 100, 30);
    mainImport.setBounds (536, 450, 86, 30);
    mainTemplate.setBounds (626, 450, 96, 30);
    mainClear.setBounds (762, 450, 76, 30);
    overview.setBounds (30, 488, 810, 206);
    miniMixer.setBounds (30, 490, 810, 204);

    editOverlay.setBounds (content.getLocalBounds());
    editOverlay.closeButton.setBounds (1134, 22, 28, 28);
    editOverlay.rewindButton.setBounds (28, 74, 38, 30);
    editOverlay.playButton.setBounds (66, 74, 44, 30);
    editOverlay.stopButton.setBounds (110, 74, 38, 30);
    editOverlay.accentBrush.setBounds (174, 74, 74, 30);
    editOverlay.hitBrush.setBounds (248, 74, 60, 30);
    editOverlay.ghostBrush.setBounds (308, 74, 70, 30);
    editOverlay.flamBrush.setBounds (378, 74, 70, 30);
    gridTab.setBounds (476, 74, 92, 30);
    velocityTab.setBounds (568, 74, 84, 30);
    mixerTab.setBounds (652, 74, 64, 30);
    fillTab.setBounds (716, 74, 54, 30);
    newRhythmButton.setBounds (784, 74, 82, 30);
    myGroovesButton.setBounds (874, 74, 96, 30);
    importButton.setBounds (970, 74, 84, 30);
    templateButton.setBounds (1062, 74, 96, 30);
    timingButton.setBounds (1062, 74, 96, 30);
    clearButton.setBounds (1068, 22, 60, 28);
    grid.setBounds (24, 116, 1132, 576);
    velocityView.setBounds (24, 116, 1132, 576);
    mixerView.setBounds (24, 116, 1132, 576);
    fillView.setBounds (24, 116, 1132, 576);

    // Export MIDI & audio (860, 426, 302, 424)
    notesBox.setBounds (874, 472, 134, 28);
    lengthBox.setBounds (1012, 472, 134, 28);
    dragTile.setBounds (874, 512, 272, 114);
    saveButton.setBounds (874, 636, 88, 30);
    wavButton.setBounds (966, 636, 88, 30);
    songButton.setBounds (1058, 636, 88, 30);

    songOverlay.setBounds (content.getLocalBounds());
    browser.setBounds (content.getLocalBounds());
    progressionBrowser.setBounds (content.getLocalBounds());
    options.setBounds (content.getLocalBounds());
    updateOverlay.setBounds (content.getLocalBounds());
}

void FaseGrooveLeadEditor::paintContent (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // ---- Header ----
    Branding::drawFullLogo (g, logoArea.toFloat().withHeight (46.0f));
    g.setColour (Palette::dim);
    g.setFont (font (11.0f));
    g.drawText (tr ("by") + " " + juce::String (Branding::artist), logoArea.getX() + 58, logoArea.getBottom() + 2, 200, 14, juce::Justification::centredLeft);

    auto box = [&g] (juce::Rectangle<int> r)
    {
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (Palette::panelEdge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
    };
    drawPanel (g, tempoArea, {});
    g.setColour (Palette::accent);
    g.fillRoundedRectangle ((float) tempoArea.getX() + 12.0f, (float) tempoArea.getY() + 9.0f, 3.0f, 12.0f, 1.0f);
    g.setColour (Palette::text.withAlpha (0.92f));
    g.setFont (font (11.5f, true));
    g.drawFittedText (Localisation::upper (tr ("TEMPO")), { tempoArea.getX() + 21, tempoArea.getY() + 5, 122, 20 }, juce::Justification::centredLeft, 1, 0.7f);

    // The display: the groove playing, its genre, the section and where the playhead is.
    box (displayArea);
    {
        const auto settings = processor.currentSettings();
        const auto base = fgl::GrooveEngine::basePattern (settings);
        auto r = displayArea.reduced (16, 10);
        auto top = r.removeFromTop (36);
        g.setColour (Palette::accent);
        g.setFont (font (13.0f, true));
        const juce::String section (fgl::sectionName ((int) settings.section));
        g.drawText (Localisation::upper (tr (section)), top.removeFromRight (90), juce::Justification::topRight);
        g.setColour (Palette::text);
        g.setFont (font (22.0f, true));
        g.drawFittedText (settings.pattern == fgl::userPatternIndex() ? tr ("User pattern") : settings.pattern == fgl::randomPatternIndex() ? tr ("Random groove")
                                                                                                                       : juce::String (base.name),
                          top, juce::Justification::centredLeft, 1, 0.6f);

        const double ppq = processor.displayPpq();
        const auto where = ppq >= 0.0 ? tr ("Bar") + " " + juce::String ((int) std::floor (ppq / 4.0) + 1) + "  " + tr ("Beat") + " " + juce::String ((int) std::floor (std::fmod (ppq, 4.0)) + 1)
                                      : tr ("Stopped");
        const auto genre = settings.pattern == fgl::userPatternIndex() ? tr ("Your pattern")
                         : base.category.empty() ? juce::String (fgl::grooveCategories()[(size_t) juce::jlimit (0, 12, settings.randomGenre)])
                                                 : juce::String (base.category);
        g.setColour (Palette::dim);
        g.setFont (font (12.0f));
        g.drawText (genre + "   " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "   " + (processor.followsHeldChord() ? ChordPanel::nameOf (processor.heldChord())
                                                                         : ChordPanel::nameOf (processor.chordForBar (ppq >= 0.0 ? (long long) std::floor (ppq / 4.0) : 0))),
                    r, juce::Justification::centredLeft);
        g.drawText (where, r, juce::Justification::centredRight);   // (the tempo is beside, in TEMPO)
    }

    // ---- Panels ----
    drawPanel (g, { 20, 100, 1140, 120 }, "Groove");
    drawPanel (g, { 20, 230, 290, 186 }, "Lead part");
    drawPanel (g, { 320, 230, 260, 186 }, "Chords");
    drawPanel (g, { 590, 230, 300, 186 }, "Groove feel & song");
    drawPanel (g, { 900, 230, 260, 186 }, "Guide sound & MIDI out");
    drawPanel (g, { 20, 426, 830, 276 }, {});
    drawPanel (g, { 860, 426, 300, 276 }, "Export MIDI & audio");

    auto caption = [&g] (const juce::String& text, int x, int y, int w = 160)
    {
        g.setColour (Palette::dim);
        g.setFont (font (11.0f, true));
        g.drawFittedText (Localisation::upper (tr (text)), { x, y, w, 15 }, juce::Justification::centredLeft, 1, 0.7f);
    };
    caption ("Groove (click: browser)", 34, 119, 214);
    caption ("Density", 310, 169);
    caption (processor.steps() > 32 ? "Parts A-D (one bar each: play them in any order)" : "Parts A-D (each half a bar: play them in any order)", 430, 104, 400);
    caption ("Lane", 34, 252);
    caption ("Octave", 34, 302);
    caption ("Global octave", 34, 383, 138);
    caption ("Level", 186, 300, 54);
    caption ("Volume", 242, 300, 58);
    caption ("Attack delay", 1034, 299, 116);
    caption ("Edit", 34, 434, 112);
    caption ("View", 156, 434);
    caption ("New rhythm", 338, 434);
    caption ("Groove", 432, 434);
    caption ("Template", 626, 434, 96);
    caption ("Notes", 874, 456);
    caption ("Clip length", 1012, 456);

    // The notes the picked lane plays over the chord of the bar.
    {
        const double ppq = processor.displayPpq();
        const long long bar = ppq >= 0.0 ? (long long) std::floor (ppq / 4.0) : 0;
        const auto context = processor.currentSettings();
        const auto harmony = processor.chordForBar (bar);
        fgl::NoteList notes;
        const int step = ppq >= 0.0 ? (int) std::fmod (std::floor (ppq / fgl::stepLength), 32.0) : 0;
        fgl::laneNotes ((fgl::Lane) selectedLane, harmony, fgl::voiceChord (harmony), step,
                        processor.laneOctave (selectedLane) + context.globalOctave, notes, context.roomForBand, false,
                        context.pattern == fgl::randomPatternIndex() ? fgl::lickContourFor (context.randomGenre) : 0,
                        context.key, context.scale, context.riffSeed);
        juce::StringArray names;
        for (int n : notes)
            names.add (juce::MidiMessage::getMidiNoteName (n, true, true, 4));
        g.setColour (Palette::dim);
        g.setFont (font (10.5f, true));
        g.drawText (Localisation::upper (tr ("Plays")), juce::Rectangle<int> (34, 352, 146, 14), juce::Justification::centredLeft);
        g.setColour (laneColour (selectedLane));
        g.setFont (font (12.5f, true));
        g.drawFittedText (names.joinIntoString (" "), { 34, 368, 146, 34 }, juce::Justification::topLeft, 2, 0.7f);
    }

    g.setColour (Palette::dim);
    g.setFont (font (10.5f));
    g.drawFittedText (tr ("Drop it on a lead or keys track (your own instrument)"), { 874, 672, 272, 16 }, juce::Justification::centredLeft, 1, 0.7f);

    // A short message (a saved preset, an imported file...)
    if (messageTicks > 0)
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, 440.0f, 34.0f).withCentre ({ designWidth * 0.5f, 560.0f });
        g.setColour (Palette::accent);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Palette::background);
        g.setFont (font (13.0f, true));
        g.drawFittedText (message, r.reduced (10.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }
}

void FaseGrooveLeadEditor::setMainTab (bool volumes)
{
    showVolumes = volumes;
    mainGrooveTab.setToggleState (! volumes, juce::dontSendNotification);
    mainVolumeTab.setToggleState (volumes, juce::dontSendNotification);
    overview.setVisible (! volumes);
    miniMixer.setVisible (volumes);
}

void FaseGrooveLeadEditor::openEditor (int view)
{
    if (view >= 0)
        setView (view);
    editOverlay.setVisible (true);
    editOverlay.toFront (false);
    grid.refresh();
}

void FaseGrooveLeadEditor::setView (int mode)
{
    viewMode = juce::jlimit (0, 3, mode);
    gridTab.setToggleState (viewMode == 0, juce::dontSendNotification);
    velocityTab.setToggleState (viewMode == 1, juce::dontSendNotification);
    mixerTab.setToggleState (viewMode == 2, juce::dontSendNotification);
    fillTab.setToggleState (viewMode == 3, juce::dontSendNotification);
    grid.setVisible (viewMode == 0);
    velocityView.setVisible (viewMode == 1);
    mixerView.setVisible (viewMode == 2);
    fillView.setVisible (viewMode == 3);
    timingButton.setVisible (viewMode == 2);
    templateButton.setVisible (viewMode != 2);
    velocityView.setLane (selectedLane);
    content.repaint();
    editOverlay.repaint();
}

void FaseGrooveLeadEditor::selectLane (int lane)
{
    selectedLane = juce::jlimit (0, fgl::numLanes - 1, lane);
    levelAttachment.reset();
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::level (selectedLane), levelKnob);
    volumeAttachment.reset();
    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::laneVolume (selectedLane), volumeKnob);
    volumeKnob.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    levelKnob.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    if (velocityView.getLane() != selectedLane)
        velocityView.setLane (selectedLane);
    refreshKitPanel();
    content.repaint();
}

void FaseGrooveLeadEditor::refreshKitPanel()
{
    showInMenu (laneBox, selectedLane + 1);
    showInMenu (octaveBox, juce::jlimit (-2, 2, processor.laneOctave (selectedLane)) + 3);
    const bool muted = processor.isLaneMuted (selectedLane);
    muteButton.setToggleState (muted, juce::dontSendNotification);
    muteButton.setColour (juce::TextButton::buttonOnColourId, Palette::toms);
}

void FaseGrooveLeadEditor::stepGroove (int delta)
{
    // Through the menu's order (by genre).
    const auto& order = fgl::patternsByCategory();
    const int current = juce::roundToInt (processor.raw (ParamIDs::pattern));
    auto it = std::find (order.begin(), order.end(), current);
    int index = it == order.end() ? 0 : (int) (it - order.begin()) + delta;
    index = (index % (int) order.size() + (int) order.size()) % (int) order.size();
    processor.selectGroove (order[(size_t) index]);
}

void FaseGrooveLeadEditor::showPlayPosition()
{
    if (miniMode)
        return;
    const int step = processor.displayStep();
    overview.setPlayStep (step);
    if (editOverlay.isVisible())
    {
        if (viewMode == 0)
            grid.setPlayStep (step);
        else if (viewMode == 1)
            velocityView.setPlayStep (step);
    }
    // A new window of bars: the overview shows what they play.
    const int len = juce::jmax (1, processor.steps());
    const double ppq = processor.displayPpq();
    const long long window = ppq < 0.0 ? -1 : (long long) std::floor (ppq / fgl::stepLength) / len;
    if (window != shownWindow)
    {
        shownWindow = window;
        overview.refresh();
    }
    // The part playing lights up when the sound gets there, not at the next slow refresh.
    const int segment = step < 0 ? -1 : step / juce::jmax (1, processor.steps() / fgl::numSegments);
    if (segment != shownSegment)
    {
        shownSegment = segment;
        const auto shown = processor.displayedPattern();
        for (auto& card : partCards)
            card->refresh (shown, step);
    }
}

void FaseGrooveLeadEditor::timerCallback()
{
    if (miniMode)
    {
        // Safety net (as Fase Groove Piano): back to full size once the drag is over, even if
        // the drop was never reported.
        if (juce::Time::getMillisecondCounter() - miniStartedAt > 800
            && ! juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown())
            exitMiniMode();
        return;
    }
    // The light, cheap things 30 times a second; the rest 10 times.
    const bool slowTick = (++uiTicks % 3) == 0;
    if (slowTick)
    for (auto* c : combos)
        c->refresh();

    // The lights follow the hits and fade out.
    for (int l = 0; l < fgl::numLanes; ++l)
        lights[(size_t) l] = juce::jmax (processor.laneFlash[(size_t) l].exchange (0.0f), lights[(size_t) l] * 0.8f);
    grid.setLights (lights, selectedLane);
    chordPanel.refresh();
    if (progressionBrowser.isVisible())
        progressionBrowser.refresh();

    if (slowTick)
        overview.refresh();
    miniMixer.refresh();
    const bool playingNow = processor.previewOn.load();
    editOverlay.playButton.setIcon (playingNow ? IconButton::Icon::pause : IconButton::Icon::play);
    editOverlay.playButton.setToggleState (playingNow, juce::dontSendNotification);
    if (editOverlay.isVisible())   // the views only while the editor is open
    {
        if (viewMode == 1)
            velocityView.refresh();
        else if (viewMode == 2)
            mixerView.setLights (lights);
        else if (viewMode == 3)
            fillView.repaint();
        else
            grid.refresh();
    }
    linkButton.setToggleState (processor.isLinked(), juce::dontSendNotification);
    keyLinkButton.setToggleState (processor.isKeyLinked(), juce::dontSendNotification);
    {
        // KEY shows the key itself: "KEY  A Dorian".
        const int key = juce::roundToInt (processor.raw (ParamIDs::key));
        const int scale = juce::roundToInt (processor.raw (ParamIDs::scale));
        const int language = (int) Localisation::current();
        if (key != shownKey || scale != shownScale || language != shownLanguage)
        {
            shownKeyText = tr ("KEY") + "  " + fgl::pitchClassName (key) + " " + tr (fgl::scaleName (scale));
            shownKey = key;
            shownScale = scale;
            shownLanguage = language;
        }
        if (keyLinkButton.getButtonText() != shownKeyText)
            keyLinkButton.setButtonText (shownKeyText);
    }
    templateButton.setToggleState (processor.hasGrooveTemplate(), juce::dontSendNotification);
    mainTemplate.setToggleState (processor.hasGrooveTemplate(), juce::dontSendNotification);

    const int step = processor.displayStep();
    if (slowTick)
    {
        // (resolved only when the cards refresh: the groove is rebuilt from scratch each time)
        const auto shown = processor.displayedPattern();
        for (auto& card : partCards)
            card->refresh (shown, step);
    }
    refreshKitPanel();

    // The groove's name on its button.
    {
        const int pattern = juce::roundToInt (processor.raw (ParamIDs::pattern));
        const auto& presets = fgl::leadPatterns();
        const auto name = pattern == fgl::userPatternIndex() ? tr ("User pattern")
                        : pattern == fgl::randomPatternIndex() ? tr ("Random groove")
                                                               : juce::String (presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, pattern)].name);
        if (grooveButton.getButtonText() != name)
            grooveButton.setButtonText (name);
        // The browser's pictures follow the groove's settings: redrawn when the groove changes,
        // else with the slower refresh.
        if (browser.isVisible() && (slowTick || pattern != browserPattern))
            browser.repaint();
        browserPattern = pattern;
    }

    const bool playing = playingNow;
    playButton.setIcon (playing ? IconButton::Icon::pause : IconButton::Icon::play);
    playButton.setToggleState (playing || step >= 0, juce::dontSendNotification);
    {
        // As in Fase Groove Piano: while synced, the DAW's tempo in place of the slider.
        const bool standalone = processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
        const bool synced = processor.raw (ParamIDs::tempoSync) >= 0.5f && ! standalone;
        tempoSlider.setVisible (! synced);
        dawTempo.setVisible (synced);
        if (synced)
            dawTempo.setText (juce::String (processor.hostBpm.load(), 1) + " BPM  " + tr ("(DAW)"), juce::dontSendNotification);
    }
    fullScreenButton.setIcon (isFullScreen() ? IconButton::Icon::exitFullScreen : IconButton::Icon::fullScreen);
    {
        const auto notes = notesBox.getText(), length = lengthBox.getText();
        if (notes != shownNotes || length != shownLength)
        {
            shownNotes = notes;
            shownLength = length;
            dragTile.setSubtitle (notes + juce::String::fromUTF8 ("  \xc2\xb7  ") + length);
        }
    }
    if (const auto tip = tr (isFullScreen() ? "Exit full screen" : "Full screen"); fullScreenButton.getTooltip() != tip)
        fullScreenButton.setTooltip (tip);

    // Undo history: a snapshot once a mouse edit has settled; a change pushes the previous one.
    if (pendingSnapshot > 0 && --pendingSnapshot == 0)
    {
        const auto now = processor.snapshot();
        if (now != lastSnapshot)
        {
            undoStack.add (lastSnapshot);
            if (undoStack.size() > 80)
                undoStack.remove (0);
            redoStack.clear();
            lastSnapshot = now;
        }
    }
    undoButton.setEnabled (! undoStack.isEmpty());
    redoButton.setEnabled (! redoStack.isEmpty());

    if (messageTicks > 0 && --messageTicks == 0)
        content.repaint();

    content.repaint (displayArea);
    content.repaint (20, 340, 160, 70);
}

void FaseGrooveLeadEditor::undo()
{
    const auto now = processor.snapshot();
    if (now != lastSnapshot)   // a change not yet in the history: undo that first
    {
        undoStack.add (lastSnapshot);
        lastSnapshot = now;
    }
    if (undoStack.isEmpty())
        return;
    redoStack.add (lastSnapshot);
    lastSnapshot = undoStack[undoStack.size() - 1];
    undoStack.remove (undoStack.size() - 1);
    processor.restoreSnapshot (lastSnapshot);
    snapshotTicks = 0;
}

void FaseGrooveLeadEditor::redo()
{
    if (redoStack.isEmpty())
        return;
    undoStack.add (processor.snapshot());
    lastSnapshot = redoStack[redoStack.size() - 1];
    redoStack.remove (redoStack.size() - 1);
    processor.restoreSnapshot (lastSnapshot);
    snapshotTicks = 0;
}

void FaseGrooveLeadEditor::showMessage (const juce::String& text)
{
    message = text;
    messageTicks = 75;
    content.repaint();
}

void FaseGrooveLeadEditor::askName (const juce::String& title, const juce::String& suggestion, std::function<void (const juce::String&)> onName)
{
    auto* window = new juce::AlertWindow (tr (title), tr ("Name") + ":", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", tr (suggestion));
    window->addButton (tr ("SAVE"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton (tr ("CANCEL"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->enterModalState (true, juce::ModalCallbackFunction::create ([window, onName] (int result)
    {
        if (result == 1 && onName != nullptr)
            onName (window->getTextEditorContents ("name").trim());
    }), true);
}

void FaseGrooveLeadEditor::showPresetsMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, tr ("Save preset..."));
    const auto files = FaseGrooveLeadProcessor::presetFiles();
    if (! files.isEmpty())
    {
        menu.addSectionHeader (tr ("Your presets"));
        for (int i = 0; i < files.size(); ++i)
            menu.addItem (100 + i, files[i].getFileNameWithoutExtension());
    }
    menu.addSeparator();
    menu.addItem (2, tr ("Open the presets folder"));

    juce::Component::SafePointer<FaseGrooveLeadEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetsButton), [safe, files] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        auto& p = safe->processor;
        if (result == 1)
            safe->askName ("Save preset", "My preset", [safe] (const juce::String& name)
            {
                if (safe != nullptr && name.isNotEmpty())
                    safe->showMessage (safe->processor.savePreset (name) ? tr ("Preset saved") + ": " + name : tr ("Could not save the preset"));
            });
        else if (result == 2)
        {
            FaseGrooveLeadProcessor::presetFolder().createDirectory();
            FaseGrooveLeadProcessor::presetFolder().startAsProcess();
        }
        else if (juce::isPositiveAndBelow (result - 100, files.size()))
            safe->showMessage (p.loadPreset (files[result - 100]) ? tr ("Preset") + ": " + files[result - 100].getFileNameWithoutExtension()
                                                                  : tr ("Could not read the preset"));
    });
}

void FaseGrooveLeadEditor::showGroovesMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, tr ("Save this groove..."));
    menu.addItem (3, tr ("Import a MIDI file..."));
    const auto files = FaseGrooveLeadProcessor::grooveFiles();
    if (! files.isEmpty())
    {
        menu.addSectionHeader (tr ("My grooves"));
        for (int i = 0; i < files.size(); ++i)
            menu.addItem (100 + i, files[i].getFileNameWithoutExtension());
    }
    menu.addSeparator();
    menu.addItem (2, tr ("Open the grooves folder"));

    juce::Component::SafePointer<FaseGrooveLeadEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuTarget (myGroovesButton, mainMyGrooves)), [safe, files] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        if (result == 1)
            safe->askName ("Save groove", "My groove", [safe] (const juce::String& name)
            {
                if (safe != nullptr && name.isNotEmpty())
                    safe->showMessage (safe->processor.saveGroove (name) ? tr ("Groove saved") + ": " + name : tr ("Could not save the groove"));
            });
        else if (result == 2)
        {
            FaseGrooveLeadProcessor::grooveFolder().createDirectory();
            FaseGrooveLeadProcessor::grooveFolder().startAsProcess();
        }
        else if (result == 3)
            safe->importMidi();
        else if (juce::isPositiveAndBelow (result - 100, files.size()))
            safe->showMessage (safe->processor.loadGroove (files[result - 100]) ? tr ("Groove") + ": " + files[result - 100].getFileNameWithoutExtension()
                                                                                 : tr ("Could not read the groove"));
    });
}

void FaseGrooveLeadEditor::importMidi()
{
    chooser = std::make_unique<juce::FileChooser> (tr ("Import a MIDI file"), juce::File(), "*.mid;*.midi");
    juce::Component::SafePointer<FaseGrooveLeadEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe != nullptr && fc.getResult().existsAsFile())
            safe->importFile (fc.getResult());
    });
}

void FaseGrooveLeadEditor::importFile (const juce::File& file)
{
    if (FaseGrooveLeadProcessor::isAudioFile (file))
    {
        // An audio loop: its groove (timing and accents) as a template.
        const auto report = processor.grooveTemplateFromAudio (file);
        showMessage (report.isNotEmpty() ? tr ("Groove template") + ": " + report : tr ("No groove found in") + " " + file.getFileName());
        return;
    }
    showMessage (processor.importMidiGroove (file) ? tr ("Imported") + ": " + file.getFileNameWithoutExtension()
                                                   : tr ("No notes found in") + " " + file.getFileName());
    setView (0);
}

void FaseGrooveLeadEditor::showPlayingMenu()
{
    // How the lead is played, and how a sampled lead (with keyswitches) is told.
    auto isOn = [this] (const juce::String& id) { return processor.raw (id) > 0.5f; };
    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("How the lead is played"));
    menu.addItem (1, tr ("Hold hooks until the next lead note"), true, isOn (ParamIDs::letRing));
    menu.addItem (2, tr ("Upper register (leave room for bass and piano)"), true, isOn (ParamIDs::roomForBand));
    menu.addItem (3, tr ("Push chord changes (the last 8th plays the next chord)"), true, isOn (ParamIDs::pushChanges));
    menu.addSectionHeader (tr ("Sampled lead"));
    menu.addItem (4, tr ("Articulation keyswitches and expression swells"), true, isOn (ParamIDs::keyswitches));
    for (int a = 0; a < fgl::numArticulations; ++a)
    {
        juce::PopupMenu notes;
        const int current = juce::roundToInt (processor.raw (ParamIDs::keyswitch (a)));
        for (int n = 0; n < 40; ++n)
            notes.addItem (100 + a * 64 + n, juce::MidiMessage::getMidiNoteName (n, true, true, 3), true, n == current);
        menu.addSubMenu (tr ("Keyswitch") + " " + tr (fgl::articulationName (a)) + ": " + juce::MidiMessage::getMidiNoteName (current, true, true, 3),
                         notes, isOn (ParamIDs::keyswitches));
    }
    juce::Component::SafePointer<FaseGrooveLeadEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&playingButton), [safe, isOn] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        auto& p = safe->processor;
        const char* toggles[] { ParamIDs::letRing, ParamIDs::roomForBand, ParamIDs::pushChanges, ParamIDs::keyswitches };
        if (result <= 4)
            p.setParameterFromUI (toggles[result - 1], isOn (toggles[result - 1]) ? 0.0f : 1.0f);
        else if (result >= 100)
            p.setParameterFromUI (ParamIDs::keyswitch ((result - 100) / 64), (float) ((result - 100) % 64));
    });
}

void FaseGrooveLeadEditor::showTemplateMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("Groove template: timing and accents of every 16th"));
    menu.addItem (1, tr ("From an audio loop (WAV, AIFF...)..."));
    menu.addItem (2, tr ("From a MIDI file..."));
    if (processor.grooveTemplateName().isNotEmpty())
    {
        menu.addSeparator();
        menu.addItem (3, tr ("Template on") + ": " + processor.grooveTemplateName(), true, processor.hasGrooveTemplate());
        menu.addItem (4, tr ("Remove the template"));
    }
    juce::Component::SafePointer<FaseGrooveLeadEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuTarget (templateButton, mainTemplate)), [safe] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        auto& p = safe->processor;
        if (result == 3) { p.setGrooveTemplateActive (! p.hasGrooveTemplate()); return; }
        if (result == 4) { p.clearGrooveTemplate(); return; }
        const bool audio = result == 1;
        safe->chooser = std::make_unique<juce::FileChooser> (tr ("Groove template"), juce::File(), audio ? "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg" : "*.mid;*.midi");
        safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe, audio] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || ! fc.getResult().existsAsFile())
                return;
            const auto report = audio ? safe->processor.grooveTemplateFromAudio (fc.getResult()) : safe->processor.grooveTemplateFromMidi (fc.getResult());
            safe->showMessage (report.isNotEmpty() ? tr ("Groove template") + ": " + report : tr ("No groove found in") + " " + fc.getResult().getFileName());
        });
    });
}

void FaseGrooveLeadEditor::showAudioExportMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("The guide sound as audio (WAV)"));
    menu.addItem (1, tr ("This clip (loops seamlessly)..."));
    menu.addItem (2, tr ("The whole song..."));
    menu.addItem (3, tr ("One WAV per lane (stems)..."));
    juce::Component::SafePointer<FaseGrooveLeadEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&wavButton), [safe] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        const auto mode = (FaseGrooveLeadProcessor::AudioExport) (result - 1);
        const int bars = safe->selectedClipBars();
        auto folder = FaseGrooveLeadProcessor::userFolder().getChildFile ("Audio");
        folder.createDirectory();
        const auto suggested = folder.getChildFile (safe->processor.audioFileName (mode, bars));
        const bool stems = mode == FaseGrooveLeadProcessor::AudioExport::stems;
        safe->chooser = std::make_unique<juce::FileChooser> (tr ("Save the audio"), suggested, stems ? "" : "*.wav");
        safe->chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                        | (stems ? 0 : juce::FileBrowserComponent::warnAboutOverwriting),
                                    [safe, mode, bars, stems] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File())
                return;
            const auto target = stems ? fc.getResult() : fc.getResult().withFileExtension ("wav");
            const auto files = safe->processor.writeAudio (mode, bars, target);
            safe->showMessage (files.isEmpty() ? tr ("Could not write") + " " + target.getFileName()
                                               : tr ("Saved") + ": " + (stems ? juce::String (files.size()) + " WAV" : target.getFileName()));
        });
    });
}

bool FaseGrooveLeadEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi") || FaseGrooveLeadProcessor::isAudioFile (juce::File (f)))
            return true;
    return false;
}

void FaseGrooveLeadEditor::filesDropped (const juce::StringArray& files, int, int)
{
    // A MIDI file: its rhythm and chords. An audio loop: its groove as a template.
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi") || FaseGrooveLeadProcessor::isAudioFile (juce::File (f)))
        {
            importFile (juce::File (f));
            return;
        }
}

int FaseGrooveLeadEditor::selectedClipBars() const
{
    return clipBarChoices[juce::jlimit (0, (int) std::size (clipBarChoices) - 1, lengthBox.getSelectedId() - 1)];
}

FaseGrooveLeadProcessor::ClipPart FaseGrooveLeadEditor::selectedClipPart() const
{
    return notesBox.getSelectedId() == 2 ? FaseGrooveLeadProcessor::ClipPart::tracks : FaseGrooveLeadProcessor::ClipPart::all;
}

void FaseGrooveLeadEditor::saveMidiFile()
{
    const auto bars = selectedClipBars();
    const auto part = selectedClipPart();
    auto folder = FaseGrooveLeadProcessor::clipFolder();
    folder.createDirectory();

    chooser = std::make_unique<juce::FileChooser> (tr ("Save the MIDI clip"), folder.getChildFile (processor.clipFileName (bars, part)), "*.mid");
    juce::Component::SafePointer<FaseGrooveLeadEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, bars, part] (const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        auto file = fc.getResult();
        if (file == juce::File())
            return;
        file = file.withFileExtension ("mid");
        FaseGrooveLeadProcessor::writeMidiFileSafely (safe->processor.createMidiClip (bars, part), file);
    });
}
