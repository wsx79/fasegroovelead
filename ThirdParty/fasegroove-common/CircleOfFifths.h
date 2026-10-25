// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// Interactive circle of fifths: outer ring = major keys, inner ring = relative
// minors. Shows the current key, the chords of its scale, and the path the
// progression draws around the circle, coloured by how well the chords sound
// together. Click a key to select it; drag a dot of the progression onto
// another chord to change that chord.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

class CircleOfFifths : public juce::Component, public juce::SettableTooltipClient
{
public:
    struct ChordMark
    {
        int rootPc;
        bool minor;
        juce::String numeral {};   // scale chords: "ii", "V"...
        float harmony = 1.0f;      // progression chords: how well it sounds with its neighbours (0..1)
    };

    /** Called with the clicked root and whether the inner (minor) ring was clicked. */
    std::function<void (int pitchClass, bool minor)> onPick;

    /** Called when the dot of progression slot `slot` is dropped: on the chord of scale degree
        `degree` (>= 0), or, in free mode, on any other chord: then degree is -1 and
        `rootPc` / `minor` describe it. */
    std::function<void (int slot, int degree, int rootPc, bool minor)> onMoveChord;

    /** Draws its texts with the family typeface (Inter) instead of the default one (the arranger). */
    void setFamilyFont (bool shouldUseFamilyFont)   { familyFont = shouldUseFamilyFont; repaint(); }

    /** Free mode: every chord of the circle is a drop target, even outside the scale. */
    void setFreeMode (bool shouldBeFree)   { freeMode = shouldBeFree; repaint(); }

    /** `diatonic` holds the chord on each scale degree (0..6); `transitions[i]` scores the move
        from progression chord i to the next one (0..1). */
    void setState (int keyRoot, bool minorKey, const juce::String& scaleName,
                   const std::vector<ChordMark>& progression, const std::vector<float>& transitions,
                   int playingIndex, const std::vector<ChordMark>& diatonic);

    /** Colour for a harmony score: dull grey (clashes) to vivid (sounds great). */
    static juce::Colour harmonyColour (float score);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    struct Hit { int index = -1; bool inner = false; };

    Hit segmentAt (juce::Point<float>) const;
    juce::Point<float> segmentCentre (int index, bool inner) const;
    juce::Point<float> centre() const;

    juce::Point<float> dotPosition (int slot) const;
    int dotAt (juce::Point<float>) const;
    /** Scale degree whose chord sits on that segment (same ring first, then same root), or -1. */
    int degreeAt (Hit) const;
    void updateTooltip (juce::Point<float>);

    float outerRadius() const;
    juce::Font font (float h, bool bold) const;
    float ringSplit() const { return outerRadius() * 0.68f; }
    float innerRadius() const { return outerRadius() * 0.38f; }

    int key = 0;
    bool minor = false;
    juce::String modeName;
    std::vector<ChordMark> chords, scaleChords;
    std::vector<float> moves;
    int dragging = -1;
    bool freeMode = false;
    bool familyFont = false;
    juce::Point<float> dragPos;
    int playing = -1;
    Hit hover;
};
