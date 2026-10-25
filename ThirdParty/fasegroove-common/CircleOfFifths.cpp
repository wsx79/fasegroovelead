// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "CircleOfFifths.h"
#include "FamilyTheme.h"
#include "Localisation.h"

namespace
{
    // Clockwise from the top, one fifth apart.
    const char* const majorNames[12] { "C", "G", "D", "A", "E", "B", "F#", "Db", "Ab", "Eb", "Bb", "F" };
    const char* const minorNames[12] { "Am", "Em", "Bm", "F#m", "C#m", "G#m", "D#m", "Bbm", "Fm", "Cm", "Gm", "Dm" };

    constexpr float segment = juce::MathConstants<float>::twoPi / 12.0f;
    constexpr float legendHeight = 16.0f;

    int mod12 (int v) { return ((v % 12) + 12) % 12; }

    /** Position on the circle of a major key (or of the relative major of a minor one). */
    int indexOfMajor (int pc) { return mod12 (pc * 7); }          // 7 is its own inverse mod 12
    int indexOfMinor (int pc) { return mod12 ((pc - 9) * 7); }
    int majorAt (int index)   { return mod12 (index * 7); }
    int minorAt (int index)   { return mod12 (index * 7 + 9); }

    float angleOf (int index) { return -juce::MathConstants<float>::halfPi + index * segment; }

}

//==============================================================================
juce::Colour CircleOfFifths::harmonyColour (float score)
{
    // Most real progressions score 0.45..0.9: spread that range over the colours.
    const float t = juce::jlimit (0.0f, 1.0f, (score - 0.45f) / 0.42f);
    const juce::Colour dull (0xff6b6170), mid (0xffd9a25f), vivid (0xff6fe0a0);
    return t < 0.5f ? dull.interpolatedWith (mid, t * 2.0f) : mid.interpolatedWith (vivid, (t - 0.5f) * 2.0f);
}

void CircleOfFifths::setState (int keyRoot, bool minorKey, const juce::String& scaleName,
                               const std::vector<ChordMark>& progression, const std::vector<float>& transitions,
                               int playingIndex, const std::vector<ChordMark>& diatonic)
{
    auto sameMarks = [] (const std::vector<ChordMark>& a, const std::vector<ChordMark>& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].rootPc != b[i].rootPc || a[i].minor != b[i].minor || a[i].numeral != b[i].numeral
                || std::abs (a[i].harmony - b[i].harmony) > 0.005f)
                return false;
        return true;
    };

    bool sameMoves = transitions.size() == moves.size();
    for (size_t i = 0; sameMoves && i < moves.size(); ++i)
        sameMoves = std::abs (transitions[i] - moves[i]) <= 0.005f;

    if (keyRoot == key && minorKey == minor && playingIndex == playing && scaleName == modeName && sameMoves
        && sameMarks (chords, progression) && sameMarks (scaleChords, diatonic))
        return;

    key = keyRoot;
    minor = minorKey;
    modeName = scaleName;
    chords = progression;
    moves = transitions;
    scaleChords = diatonic;
    playing = playingIndex;
    repaint();
}

float CircleOfFifths::outerRadius() const
{
    // Room around the wheel for the progression dots.
    return juce::jmin ((float) getWidth(), (float) getHeight() - legendHeight) * 0.5f - 11.0f;
}

juce::Point<float> CircleOfFifths::centre() const
{
    return { getWidth() * 0.5f, (getHeight() - legendHeight) * 0.5f };
}

juce::Point<float> CircleOfFifths::segmentCentre (int index, bool inner) const
{
    const float r = inner ? (innerRadius() + ringSplit()) * 0.5f : (ringSplit() + outerRadius()) * 0.5f;
    return centre().getPointOnCircumference (r, angleOf (index) + juce::MathConstants<float>::halfPi);
}

juce::Point<float> CircleOfFifths::dotPosition (int slot) const
{
    const auto& ch = chords[(size_t) slot];
    const auto c = centre();
    // Dots sit just outside their ring (around the wheel for major chords, around the
    // centre for minor ones), so the chord names stay readable.
    const int index = ch.minor ? indexOfMinor (ch.rootPc) : indexOfMajor (ch.rootPc);
    const float radius = ch.minor ? innerRadius() - 3.0f : outerRadius() + 3.0f;
    auto pos = c.getPointOnCircumference (radius, angleOf (index) + juce::MathConstants<float>::halfPi);

    // Spread dots that share a chord so each one can be seen and grabbed.
    int before = 0;
    for (int i = 0; i < slot; ++i)
        if (chords[(size_t) i].rootPc == ch.rootPc && chords[(size_t) i].minor == ch.minor)
            ++before;

    if (before > 0)
    {
        const float angle = std::atan2 (pos.y - c.y, pos.x - c.x) + juce::MathConstants<float>::halfPi;
        pos = pos.getPointOnCircumference (9.0f * (float) before, angle + juce::MathConstants<float>::halfPi);
    }

    return pos;
}

int CircleOfFifths::dotAt (juce::Point<float> p) const
{
    // Topmost (last drawn) first.
    for (int i = (int) chords.size(); --i >= 0;)
        if (dotPosition (i).getDistanceFrom (p) <= 10.0f)
            return i;
    return -1;
}

int CircleOfFifths::degreeAt (Hit h) const
{
    if (h.index < 0)
        return -1;

    const int pc = h.inner ? minorAt (h.index) : majorAt (h.index);
    for (int d = 0; d < (int) scaleChords.size(); ++d)
        if (scaleChords[(size_t) d].rootPc == pc && scaleChords[(size_t) d].minor == h.inner)
            return d;

    // Same root on the other ring (e.g. dropping on "D" when the scale has Dm).
    for (int d = 0; d < (int) scaleChords.size(); ++d)
        if (scaleChords[(size_t) d].rootPc == pc)
            return d;

    return -1;
}

CircleOfFifths::Hit CircleOfFifths::segmentAt (juce::Point<float> p) const
{
    const auto c = centre();
    const float d = p.getDistanceFrom (c);

    if (d < innerRadius() || d > outerRadius())
        return {};

    const float angle = std::atan2 (p.y - c.y, p.x - c.x) + juce::MathConstants<float>::halfPi;
    return { mod12 (juce::roundToInt (angle / segment)), d < ringSplit() };
}

//==============================================================================
void CircleOfFifths::updateTooltip (juce::Point<float> p)
{
    if (const int dot = dotAt (p); dot >= 0)
    {
        setTooltip (tr ("Chord") + " " + juce::String (dot + 1) + " - " + tr ("drag it onto another chord"));
        return;
    }

    if (p.getDistanceFrom (centre()) < innerRadius())
    {
        setTooltip (tr ("Outer ring: major chords. Inner ring: minor chords. Bright slices are the chords of the scale."));
        return;
    }

    const auto h = segmentAt (p);
    if (h.index < 0)
    {
        setTooltip ({});
        return;
    }

    const juce::String name = h.inner ? minorNames[h.index] : majorNames[h.index];
    const int pc = h.inner ? minorAt (h.index) : majorAt (h.index);

    juce::String text = name;
    bool inScale = false;
    for (const auto& c : scaleChords)
        if (c.rootPc == pc && c.minor == h.inner)
        {
            text << "  (" << c.numeral << ")  -  " << tr ("in the scale");
            inScale = true;
            break;
        }

    if (! inScale)
        text << "  -  " << tr ("outside the scale");

    setTooltip (text + ".  " + tr ("Click: change key"));
}

void CircleOfFifths::mouseDown (const juce::MouseEvent& e)
{
    dragging = dotAt (e.position);
    if (dragging >= 0)
    {
        dragPos = e.position;
        repaint();
        return;
    }

    const auto h = segmentAt (e.position);
    if (h.index >= 0 && onPick != nullptr)
        onPick (h.inner ? minorAt (h.index) : majorAt (h.index), h.inner);
}

void CircleOfFifths::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    dragPos = e.position;
    hover = segmentAt (e.position);
    repaint();
}

void CircleOfFifths::mouseUp (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    const int slot = dragging;
    dragging = -1;

    const auto h = segmentAt (e.position);
    const int degree = degreeAt (h);
    const int pc = h.inner ? minorAt (h.index) : majorAt (h.index);

    if (onMoveChord != nullptr && h.index >= 0)
    {
        // In free mode only an exact match (same root and ring) counts as a scale chord.
        const bool exact = degree >= 0 && scaleChords[(size_t) degree].rootPc == pc && scaleChords[(size_t) degree].minor == h.inner;

        if (freeMode && ! exact)
            onMoveChord (slot, -1, pc, h.inner);
        else if (degree >= 0)
            onMoveChord (slot, degree, pc, h.inner);
    }

    repaint();
}

void CircleOfFifths::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (dotAt (e.position) >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::PointingHandCursor);
    updateTooltip (e.position);

    const auto h = segmentAt (e.position);
    if (h.index != hover.index || h.inner != hover.inner)
    {
        hover = h;
        repaint();
    }
}

void CircleOfFifths::mouseExit (const juce::MouseEvent&)
{
    hover = {};
    repaint();
}

//==============================================================================
void CircleOfFifths::paint (juce::Graphics& g)
{
    const auto c = centre();
    const float rOut = outerRadius(), rMid = ringSplit(), rIn = innerRadius();
    const int keyIndex = minor ? indexOfMinor (key) : indexOfMajor (key);

    // Which segments the progression uses (and the one playing now).
    auto usedBy = [this] (int index, bool inner)
    {
        for (int i = 0; i < (int) chords.size(); ++i)
            if (chords[(size_t) i].minor == inner && (inner ? indexOfMinor (chords[(size_t) i].rootPc) : indexOfMajor (chords[(size_t) i].rootPc)) == index)
                return i;
        return -1;
    };

    // 1. The rings: flat colours. Chords of the scale stand out, the others stay in the background.
    for (int i = 0; i < 12; ++i)
    {
        const float a0 = angleOf (i) - segment * 0.5f + juce::MathConstants<float>::halfPi;
        const float a1 = a0 + segment;

        for (const bool inner : { false, true })
        {
            const float rA = inner ? rMid : rOut, rB = inner ? rIn : rMid;
            juce::Path seg;
            seg.addPieSegment (c.x - rA, c.y - rA, rA * 2.0f, rA * 2.0f, a0, a1, rB / rA);

            const bool isKey = i == keyIndex && inner == minor;
            const int degree = degreeAt ({ i, inner });
            const bool exactInScale = degree >= 0 && scaleChords[(size_t) degree].minor == inner;
            const bool hovered = hover.index == i && hover.inner == inner;
            const bool target = dragging >= 0 && (degree >= 0 || freeMode);
            const int used = usedBy (i, inner);
            const bool playingHere = used >= 0 && playing >= 0 && chords[(size_t) playing].minor == inner
                                     && (inner ? indexOfMinor (chords[(size_t) playing].rootPc) : indexOfMajor (chords[(size_t) playing].rootPc)) == i;

            juce::Colour fill = Palette::background.interpolatedWith (Palette::panel, 0.35f);
            if (exactInScale)  fill = Palette::panelEdge.interpolatedWith (Palette::panel, inner ? 0.35f : 0.1f);
            if (used >= 0)     fill = fill.interpolatedWith (harmonyColour (chords[(size_t) used].harmony), 0.28f);
            if (isKey)         fill = Palette::accent;
            if (playingHere)   fill = fill.interpolatedWith (Palette::text, 0.35f);
            if (hovered)       fill = fill.brighter (0.18f);
            if (dragging >= 0 && ! target)
                fill = fill.withMultipliedAlpha (0.4f);

            g.setColour (fill);
            g.fillPath (seg);
            g.setColour (Palette::panel);
            g.strokePath (seg, juce::PathStrokeType (2.0f));

            if (target)
            {
                g.setColour (Palette::accent2.withAlpha (hovered ? 1.0f : exactInScale ? 0.6f : 0.22f));
                g.strokePath (seg, juce::PathStrokeType (hovered ? 2.5f : 1.5f));
            }
        }
    }

    // 2. The progression's path: soft curves bending towards the centre, under the names.
    std::vector<juce::Point<float>> points;
    for (int i = 0; i < (int) chords.size(); ++i)
        points.push_back (i == dragging ? dragPos : dotPosition (i));
    const int n = (int) points.size();
    if (n > 1)
    {
        const int lines = n > 2 ? n : n - 1;   // loop back to the first chord (two chords: one line)
        for (int i = 0; i < lines; ++i)
        {
            const auto from = points[(size_t) i], to = points[(size_t) ((i + 1) % n)];
            const float score = i < (int) moves.size() ? moves[(size_t) i] : 1.0f;
            const auto mid = (from + to) * 0.5f;
            const auto control = mid + (c - mid) * 0.45f;
            juce::Path curve;
            curve.startNewSubPath (from);
            curve.quadraticTo (control, to);
            g.setColour (harmonyColour (score).withAlpha (0.5f));
            g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // 3. Names: scale chords bold with their numeral, the others faint.
    for (int i = 0; i < 12; ++i)
        for (const bool inner : { false, true })
        {
            const bool isKey = i == keyIndex && inner == minor;
            const int degree = degreeAt ({ i, inner });
            const bool exactInScale = degree >= 0 && scaleChords[(size_t) degree].minor == inner;
            const auto mid = segmentCentre (i, inner);
            const juce::String name = inner ? minorNames[i] : majorNames[i];
            const float scale = juce::jlimit (0.8f, 1.3f, rOut / 110.0f);

            if (exactInScale)
            {
                g.setColour (isKey ? Palette::background : Palette::text);
                g.setFont (font ((inner ? 11.0f : 13.5f) * scale, true));
                g.drawText (name, juce::Rectangle<float> (48.0f, 15.0f).withCentre (mid.translated (0.0f, -4.5f * scale)), juce::Justification::centred);
                g.setColour (isKey ? Palette::background.withAlpha (0.8f) : Palette::accent);
                g.setFont (font ((inner ? 8.5f : 9.5f) * scale, true));
                g.drawText (scaleChords[(size_t) degree].numeral,
                            juce::Rectangle<float> (48.0f, 11.0f).withCentre (mid.translated (0.0f, (inner ? 6.5f : 7.5f) * scale)),
                            juce::Justification::centred);
            }
            else
            {
                g.setColour (Palette::dim.withAlpha (freeMode ? 0.75f : 0.45f));
                g.setFont (font ((inner ? 9.5f : 11.0f) * scale, false));
                g.drawText (name, juce::Rectangle<float> (48.0f, 15.0f).withCentre (mid), juce::Justification::centred);
            }
        }

    // Centre: key and mode.
    g.setColour (Palette::panel);
    g.fillEllipse (juce::Rectangle<float> (rIn * 2.0f - 4.0f, rIn * 2.0f - 4.0f).withCentre (c));
    g.setColour (Palette::text);
    g.setFont (font (juce::jlimit (14.0f, 20.0f, rIn * 0.45f), true));
    g.drawText (minor ? juce::String (minorNames[indexOfMinor (key)]) : juce::String (majorNames[indexOfMajor (key)]),
                juce::Rectangle<float> (rIn * 2.0f, 20.0f).withCentre (c.translated (0.0f, -6.0f)), juce::Justification::centred);
    g.setColour (Palette::dim);
    g.setFont (font (9.0f, false));
    g.drawFittedText (modeName, juce::Rectangle<float> (rIn * 1.7f, 12.0f).withCentre (c.translated (0.0f, 10.0f)).toNearestInt(),
                      juce::Justification::centred, 1, 0.7f);

    // 4. Numbered dots on the rim: drag one onto another chord to change it.
    for (int i = 0; i < n; ++i)
    {
        const bool now = i == playing;
        const bool held = i == dragging;
        const float d = held ? 20.0f : 15.0f;
        const auto r = juce::Rectangle<float> (d, d).withCentre (points[(size_t) i]);

        if (now || held)
        {
            g.setColour (Palette::text.withAlpha (0.3f));
            g.fillEllipse (r.expanded (3.0f));
        }
        g.setColour (harmonyColour (chords[(size_t) i].harmony));
        g.fillEllipse (r);
        g.setColour (now ? Palette::text : Palette::panel);
        g.drawEllipse (r, now ? 2.0f : 1.5f);
        g.setColour (Palette::background);
        g.setFont (font (held ? 11.5f : 9.5f, true));
        g.drawText (juce::String (i + 1), r, juce::Justification::centred);
    }

    // Legend: what the colours mean.
    auto legend = juce::Rectangle<float> (0.0f, getHeight() - legendHeight, (float) getWidth(), legendHeight);
    legend = legend.withSizeKeepingCentre (juce::jmin (legend.getWidth(), 220.0f), legend.getHeight());
    g.setFont (font (10.0f, false));

    auto item = [&g] (juce::Rectangle<float> area, juce::Colour colour, const juce::String& text)
    {
        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ area.getX() + 5.0f, area.getCentreY() }));
        g.setColour (Palette::dim);
        g.drawText (text, area.withTrimmedLeft (13.0f), juce::Justification::centredLeft);
    };
    item (legend.removeFromLeft (legend.getWidth() * 0.55f), harmonyColour (1.0f), tr ("sounds great"));
    item (legend, harmonyColour (0.0f), tr ("clashes"));
}

juce::Font CircleOfFifths::font (float h, bool bold) const
{
    if (familyFont)
        return FamilyTheme::font (h, bold);
    return juce::Font (juce::FontOptions (h).withStyle (bold ? "Bold" : "Regular"));
}
