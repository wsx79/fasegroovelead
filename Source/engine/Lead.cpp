// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Lead.h"

#include <algorithm>

namespace fgl
{

namespace
{
    int mod12 (int v) noexcept { return ((v % 12) + 12) % 12; }
}

const char* laneName (int lane) noexcept
{
    static const char* names[] { "Hook", "Answer", "Staccato", "Pulse", "Ghost", "Approach",
                                 "Long note", "Arpeggio", "Run", "High motif", "Low motif", "Tonic" };
    return lane >= 0 && lane < numLanes ? names[lane] : "";
}

const char* laneShortName (int lane) noexcept
{
    static const char* names[] { "HOOK", "ANSWER", "SHORT", "PULSE", "GHOST", "APPROACH",
                                 "LONG", "ARP", "RUN", "HIGH", "LOW", "TONIC" };
    return lane >= 0 && lane < numLanes ? names[lane] : "";
}

bool isChordLane (int lane) noexcept
{
    return lane >= 0 && lane < numLanes;
}

//==============================================================================
const char* chordTypeName (int type) noexcept
{
    static const char* names[] { "maj7", "m7", "7", "m9", "maj9", "9", "m11", "13", "7sus4", "6/9", "", "m", "m7b5", "dim", "aug" };
    return type >= 0 && type < numChordTypes ? names[type] : "";
}

unsigned chordTypeMask (int type) noexcept
{
    auto bits = [] (std::initializer_list<int> intervals)
    {
        unsigned m = 0;
        for (int i : intervals)
            m |= 1u << (unsigned) mod12 (i);
        return m;
    };
    switch ((ChordType) type)
    {
        case ChordType::maj7:    return bits ({ 0, 4, 7, 11 });
        case ChordType::dom7:    return bits ({ 0, 4, 7, 10 });
        case ChordType::m9:      return bits ({ 0, 3, 7, 10, 14 });
        case ChordType::maj9:    return bits ({ 0, 4, 7, 11, 14 });
        case ChordType::dom9:    return bits ({ 0, 4, 7, 10, 14 });
        case ChordType::m11:     return bits ({ 0, 3, 7, 10, 14, 17 });
        case ChordType::dom13:   return bits ({ 0, 4, 10, 14, 21 });
        case ChordType::sus4:    return bits ({ 0, 5, 7, 10 });
        case ChordType::sixNine: return bits ({ 0, 4, 7, 9, 14 });
        case ChordType::major:   return bits ({ 0, 4, 7 });
        case ChordType::minor:   return bits ({ 0, 3, 7 });
        case ChordType::halfDim: return bits ({ 0, 3, 6, 10 });
        case ChordType::dim:     return bits ({ 0, 3, 6 });
        case ChordType::aug:     return bits ({ 0, 4, 8 });
        case ChordType::m7:
        default:                 return bits ({ 0, 3, 7, 10 });
    }
}

const char* pitchClassName (int pitchClass) noexcept
{
    static const char* names[] { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return names[mod12 (pitchClass)];
}

std::string chordName (const Chord& c)
{
    return std::string (pitchClassName (c.root)) + chordTypeName (c.type);
}

ChordTones tonesOf (const Chord& c) noexcept
{
    return { mod12 (c.root), chordTypeMask (c.type) };
}

namespace
{
    int popCount (unsigned v) noexcept
    {
        int n = 0;
        for (; v != 0; v &= v - 1)
            ++n;
        return n;
    }

    /** The chord a set of pitch classes makes: the root is the held note the others make the
        best known chord over (an inversion keeps its own root), the lowest one on a tie. */
    ChordTones chordOfPitchClasses (unsigned pitchClasses, int lowest) noexcept
    {
        ChordTones best { mod12 (lowest), 0 };
        int bestScore = -1000000;
        for (int r = 0; r < 12; ++r)
        {
            if (((pitchClasses >> (unsigned) r) & 1u) == 0)
                continue;
            unsigned mask = 0;
            for (int i = 0; i < 12; ++i)
                if ((pitchClasses >> (unsigned) mod12 (r + i)) & 1u)
                    mask |= 1u << (unsigned) i;
            int fit = -1000;
            for (int t = 0; t < numChordTypes; ++t)
            {
                const unsigned type = chordTypeMask (t);
                fit = std::max (fit, 3 * popCount (mask & type) - 3 * popCount (mask & ~type) - 2 * popCount (type & ~mask));
            }
            const int score = 2 * fit + (r == mod12 (lowest) ? 1 : 0);
            if (score > bestScore)
            {
                bestScore = score;
                best = { r, mask };
            }
        }
        return best;
    }
}

ChordTones tonesOfNotes (const std::vector<int>& notes) noexcept
{
    if (notes.empty())
        return { 0, 0 };
    unsigned pitchClasses = 0;
    for (int n : notes)
        pitchClasses |= 1u << (unsigned) mod12 (n);
    return chordOfPitchClasses (pitchClasses, *std::min_element (notes.begin(), notes.end()));
}

ChordTones tonesOfHeld (const std::array<bool, 128>& held, int& count) noexcept
{
    count = 0;
    int lowest = -1;
    unsigned pitchClasses = 0;
    for (int n = 0; n < 128; ++n)
    {
        if (! held[(size_t) n])
            continue;
        if (lowest < 0)
            lowest = n;
        pitchClasses |= 1u << (unsigned) mod12 (n);
        ++count;
    }
    if (lowest < 0)
        return { 0, 0 };
    return chordOfPitchClasses (pitchClasses, lowest);
}

//==============================================================================
ChordVoicing voiceChord (const ChordTones& c, const ChordVoicing*, bool upper, LeadColour)
{
    ChordVoicing v;
    for (int i = 0; i < 12; ++i)
        if ((c.mask & (1u << i)) != 0) v.full.add (60 + mod12 (c.root) + i + (upper ? 12 : 0));
    if (v.full.empty()) v.full.add (60 + mod12 (c.root));
    v.stab = v.four = v.stabAlt = v.full;
    return v;
}

void laneNotes (Lane lane, const ChordTones& chord, const ChordVoicing&, int stepInBar, int octave,
                NoteList& out, bool roomForBand, bool alternative, int contour,
                int key, int scale, std::uint32_t seed) noexcept
{
    out.clear();
    const int step = ((stepInBar % 32) + 32) % 32;
    // A repeatable two-bar motif with an answering second half. Every lane is one voice.
    static constexpr int motifs[][8] {
        {0, 2, 4, 3, 2, 4, 1, 0}, {4, 3, 2, 0, 1, 2, 1, 0},
        {0, 0, 2, 0, 4, 2, 1, 0}, {0, 2, 1, 3, 2, 4, 2, 0},
        {2, 1, 0, 2, 3, 2, 1, 0}, {0, 1, 2, 3, 4, 3, 2, 0}
    };
    const auto hash = seed * 1664525u + 1013904223u;
    const int motif = (int) ((hash >> 16) % 6u);
    const int phase = (step / 2 + (int) ((hash >> 24) & 3u)) % 8;
    int degree = motifs[(motif + std::max (0, contour)) % 6][phase];
    switch (lane)
    {
        case Lane::answer: degree = 4 - degree; break;
        case Lane::stab: degree = (step / 4) % 3 * 2; break;
        case Lane::chank: degree = step % 4 < 2 ? 0 : 2; break;
        case Lane::scratch: degree = (step / 2) % 2; break;
        case Lane::swell: degree = step < 16 ? 2 : 0; break;
        case Lane::arp: degree = (step / 2) % 6; break;
        case Lane::lick: degree = step % 8; break;
        case Lane::octave: degree += 7; break;
        case Lane::power: degree -= 3; break;
        case Lane::root: degree = 0; break;
        default: break;
    }
    if (alternative) degree += 2;
    const auto& intervals = scaleIntervals (scale);
    const int root = 60 + mod12 (chord.root);
    // Strong beats resolve on the current harmony; passing notes follow the selected scale.
    int note = root;
    if (lane == Lane::root || lane == Lane::arp || step % 4 == 0)
    {
        std::array<int, 12> tones {}; int count = 0;
        for (int i = 0; i < 12; ++i)
            if ((chord.mask & (1u << i)) != 0) tones[(size_t) count++] = i;
        if (count == 0) { tones[0] = 0; count = 1; }
        const int index = ((degree % count) + count) % count;
        const int oct = (degree - index) / count;
        note += tones[(size_t) index] + 12 * oct;
    }
    else
    {
        int nearest = 0;
        for (int i = 1; i < 7; ++i)
            if (std::abs (intervals[(size_t) i] - mod12 (chord.root - key))
                < std::abs (intervals[(size_t) nearest] - mod12 (chord.root - key))) nearest = i;
        const int d = nearest + degree;
        const int index = ((d % 7) + 7) % 7;
        note = 60 + mod12 (key) + intervals[(size_t) index] + 12 * ((d - index) / 7);
    }
    // Fold the octave shift back into the playable range: clamping to 108 turned a
    // high hook pushed up four octaves into one repeated pedal note.
    int shifted = note + 12 * (octave + (roomForBand ? 1 : 0));
    while (shifted > 96)
        shifted -= 12;
    while (shifted < 36)
        shifted += 12;
    out.add (shifted);
}

std::vector<int> laneNotes (Lane lane, const ChordTones& chord, int stepInBar, int octave)
{
    NoteList notes;
    laneNotes (lane, chord, voiceChord (chord), stepInBar, octave, notes);
    return { notes.begin(), notes.end() };
}

//==============================================================================
//==============================================================================
const char* scaleName (int scale) noexcept
{
    static const char* names[numScales] { "Major (Ionian)", "Dorian", "Minor (Aeolian)", "Mixolydian", "Lydian", "Phrygian",
                                          "Harmonic Minor", "Melodic Minor", "Locrian", "Harmonic Major", "Lydian Dominant",
                                          "Mixolydian b6", "Phrygian Dominant" };
    return names[std::clamp (scale, 0, numScales - 1)];
}

const std::array<int, 7>& scaleIntervals (int scale) noexcept
{
    static const std::array<std::array<int, 7>, numScales> scales {{
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 4, 5, 7, 9, 10 },
        { 0, 2, 4, 6, 7, 9, 11 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 2, 3, 5, 7, 9, 11 },
        { 0, 1, 3, 5, 6, 8, 10 }, { 0, 2, 4, 5, 7, 8, 11 }, { 0, 2, 4, 6, 7, 9, 10 }, { 0, 2, 4, 5, 7, 8, 10 },
        { 0, 1, 4, 5, 7, 8, 10 } }};
    return scales[(size_t) std::clamp (scale, 0, numScales - 1)];
}

bool scaleIsMinor (int scale) noexcept { return scaleIntervals (scale)[2] == 3; }

const char* chordColourName (int colour) noexcept
{
    static const char* names[numChordColours] { "Triads", "7ths", "9ths" };
    return names[std::clamp (colour, 0, numChordColours - 1)];
}

namespace
{
    /** The type of a chord from its 3rd, 5th, 7th and 9th (semitones above the root, -1: none). */
    ChordType typeOf (int third, int fifth, int seventh, int ninth, int colour)
    {
        const bool sevenths = colour >= (int) ChordColour::sevenths, ninths = colour >= (int) ChordColour::ninths && ninth == 2;
        if (third == 4 && fifth == 8) return ChordType::aug;
        if (third == 3 && fifth == 6) return sevenths && seventh == 10 ? ChordType::halfDim : ChordType::dim;
        if (third == 3)
            return ! sevenths || seventh != 10 ? ChordType::minor : ninths ? ChordType::m9 : ChordType::m7;
        if (! sevenths) return ChordType::major;
        if (seventh == 11) return ninths ? ChordType::maj9 : ChordType::maj7;
        if (seventh == 10) return ninths ? ChordType::dom9 : ChordType::dom7;
        return ChordType::major;
    }
}

Chord diatonicChord (int key, int scale, int degree, int colour)
{
    const auto& s = scaleIntervals (scale);
    degree = ((degree % 7) + 7) % 7;
    auto above = [&] (int steps) { return mod12 (s[(size_t) ((degree + steps) % 7)] - s[(size_t) degree]); };
    return { mod12 (key + s[(size_t) degree]), (int) typeOf (above (2), above (4), above (6), above (1), colour) };
}

Chord codeChord (int key, int code, int colour)
{
    const int root = mod12 (key + code % 12), kind = std::clamp (code / 12, 0, 2);
    const bool sevenths = colour >= (int) ChordColour::sevenths, ninths = colour >= (int) ChordColour::ninths;
    ChordType t;
    if (kind == 1)      t = ! sevenths ? ChordType::minor : ninths ? ChordType::m9 : ChordType::m7;
    else if (kind == 2) t = ninths ? ChordType::dom9 : ChordType::dom7;   // a dominant keeps its 7th
    else                t = ! sevenths ? ChordType::major : ninths ? ChordType::maj9 : ChordType::maj7;
    return { root, (int) t };
}

std::vector<Chord> circleOfFifthsProgression (std::uint32_t seed, int key, int scale, int length, int colour)
{
    // Small, repeatable random numbers.
    auto next = [&seed] { seed = seed * 1664525u + 1013904223u; return (int) (seed >> 8); };
    length = std::clamp (length, 1, maxChords);
    // Each chord a fifth below the one before (three scale steps up), landing on the tonic or,
    // now and then, on the dominant (an open end that turns round).
    const int last = next() % 4 == 0 ? 4 : 0;
    const int first = ((last - 3 * (length - 1)) % 7 + 7 * length) % 7;
    std::vector<Chord> chords;
    for (int i = 0; i < length; ++i)
        chords.push_back (diatonicChord (key, scale, first + 3 * i, colour));
    // A secondary dominant: the chord before another one becomes its V7 (a fifth above it).
    if (length >= 3 && next() % 3 != 0)
    {
        const int at = next() % (length - 1);
        chords[(size_t) at] = { mod12 (chords[(size_t) at + 1].root + 7), (int) (colour >= (int) ChordColour::ninths ? ChordType::dom9 : ChordType::dom7) };
    }
    return chords;
}

float transitionScore (const Chord& a, const Chord& b) noexcept
{
    const auto ta = tonesOf (a), tb = tonesOf (b);
    int shared = 0, sizeB = 0;
    for (int pc = 0; pc < 12; ++pc)
    {
        const bool inA = (ta.mask >> (unsigned) mod12 (pc - ta.root)) & 1u, inB = (tb.mask >> (unsigned) mod12 (pc - tb.root)) & 1u;
        shared += inA && inB ? 1 : 0;
        sizeB += inB ? 1 : 0;
    }
    const int move = mod12 (b.root - a.root);
    const float root = move == 5 ? 1.0f : move == 7 ? 0.75f : move == 2 || move == 10 ? 0.6f : move == 3 || move == 4 || move == 8 || move == 9 ? 0.55f
                     : move == 0 ? 0.5f : move == 6 ? 0.1f : 0.35f;
    return std::clamp (0.55f * root + 0.45f * (float) shared / (float) std::max (1, sizeB) + 0.15f, 0.0f, 1.0f);
}

const std::vector<Progression>& progressions()
{
    // New progressions are always appended: a project stores its chords, the browser the index.
    using T = ChordType;
    static const std::vector<Progression> list {
        { "i7 - iv7 (minor vamp)", "Deep house", 1, {}, {}, { { 0, T::m7 }, { 5, T::m7 }, { 0, T::m7 }, { 5, T::m7 } } },
        { "im9 - bVImaj9 - bVII6/9 - im9", "Deep house", 2, {}, {}, { { 0, T::m9 }, { 8, T::maj9 }, { 10, T::sixNine }, { 0, T::m9 } } },
        { "ii9 - V13 - Imaj9 - vi9", "Soulful & Gospel", 0, {}, {}, { { 2, T::m9 }, { 7, T::dom13 }, { 0, T::maj9 }, { 9, T::m9 } } },
        { "IVmaj7 - V7 - iii7 - vi7", "Soulful & Gospel", 0, {}, {}, { { 5, T::maj7 }, { 7, T::dom7 }, { 4, T::m7 }, { 9, T::m7 } } },
        { "vi7 - IVmaj7 - I - V", "Soulful & Gospel", 0, {}, {}, { { 9, T::m7 }, { 5, T::maj7 }, { 0, T::major }, { 7, T::major } } },
        { "i7 - IV9 (Dorian disco)", "Jackin' & funky", 1, {}, {}, { { 0, T::m7 }, { 5, T::dom9 }, { 0, T::m7 }, { 5, T::dom9 } } },
        { "I9 (one-chord funk)", "Jackin' & funky", 3, {}, {}, { { 0, T::dom9 }, { 0, T::dom9 }, { 0, T::dom9 }, { 0, T::dom9 } } },
        { "i7 - i7 - bVII7 - bVII7", "Jackin' & funky", 2, {}, {}, { { 0, T::m7 }, { 0, T::m7 }, { 10, T::dom7 }, { 10, T::dom7 } } },
        { "im9 - bVIImaj9 - bVImaj9 - v7", "Garage & UK", 2, {}, {}, { { 0, T::m9 }, { 10, T::maj9 }, { 8, T::maj9 }, { 7, T::m7 } } },
        { "i7 - i7 - ii7 - i7 (modal)", "Afro & Latin", 1, {}, {}, { { 0, T::m7 }, { 0, T::m7 }, { 2, T::m7 }, { 0, T::m7 } } },
        { "vi - IV - I - V", "Disco & '90s", 0, {}, {}, { { 9, T::minor }, { 5, T::major }, { 0, T::major }, { 7, T::major } } },
        { "IVmaj9 - iii7 - ii9 - V13", "Neo soul & jazz", 0, {}, {}, { { 5, T::maj9 }, { 4, T::m7 }, { 2, T::m9 }, { 7, T::dom13 } } },
        { "Imaj7 - IVmaj7 - Imaj9 - IVmaj7", "Deep house", 0, {}, {}, { { 0, T::maj7 }, { 5, T::maj7 }, { 0, T::maj9 }, { 5, T::maj7 } } },
        { "i - bVI - bIII - bVII", "Disco & '90s", 2, {}, {}, { { 0, T::minor }, { 8, T::major }, { 3, T::major }, { 10, T::major } } },
        { "I7sus4 - I7 - I7sus4 - I9", "Jackin' & funky", 3, {}, {}, { { 0, T::sus4 }, { 0, T::dom7 }, { 0, T::sus4 }, { 0, T::dom9 } } },
        { "i7 - IV7 (jack bounce)", "Jackin' & funky", 1, {}, {}, { { 0, T::m7 }, { 5, T::dom7 }, { 0, T::m7 }, { 5, T::dom7 } } },
        { "im11 - im11 - bVIImaj9 - ivm9", "Afro & Latin", 1, {}, {}, { { 0, T::m11 }, { 0, T::m11 }, { 10, T::maj9 }, { 5, T::m9 } } },
        { "Imaj9 - vi9 - ii9 - V7sus4", "Soulful & Gospel", 0, {}, {}, { { 0, T::maj9 }, { 9, T::m9 }, { 2, T::m9 }, { 7, T::sus4 } } },
        // Fase Groove Piano's progressions (degrees of a scale, or codes from the circle of fifths).
        { "Dorian Vamp  i - IV", "Deep house", 1, { 0, 3 }, {  } },
        { "Deep Minor  i - v", "Deep house", 1, { 0, 4 }, {  } },
        { "Soulful Descent  IV - iii - ii - I", "Soulful & Gospel", 0, { 3, 2, 1, 0 }, {  } },
        { "Jazzy  ii - V", "Soulful & Gospel", 0, { 1, 4 }, {  } },
        { "Turnaround  I - vi - ii - V", "Soulful & Gospel", 0, { 0, 5, 1, 4 }, {  } },
        { "Royal Road  IV - V - iii - vi", "Soulful & Gospel", 0, { 3, 4, 2, 5 }, {  } },
        { "Minor Flow  i - VI - III - VII", "Deep house", 2, { 0, 5, 2, 6 }, {  } },
        { "Late Night  i - VII - VI - VII", "Deep house", 2, { 0, 6, 5, 6 }, {  } },
        { "Gospel Lift  IV - V - vi - I", "Soulful & Gospel", 0, { 3, 4, 5, 0 }, {  } },
        { "Mixo Groove  I - VII - IV - I", "Jackin' & funky", 3, { 0, 6, 3, 0 }, {  } },
        { "Lydian Float  I - II", "Modal & exotic", 4, { 0, 1 }, {  } },
        { "8-Bar Journey", "Soulful & Gospel", 0, { 0, 5, 3, 4, 2, 5, 1, 4 }, {  } },
        { "Circle of 5ths  vi - ii - V - I", "Circle of 5ths", 0, { 5, 1, 4, 0 }, {  } },
        { "Circle of 5ths  iii - vi - ii - V", "Circle of 5ths", 0, { 2, 5, 1, 4 }, {  } },
        { "Full Circle  I-IV-vii-iii-vi-ii-V-I", "Circle of 5ths", 0, { 0, 3, 6, 2, 5, 1, 4, 0 }, {  } },
        { "Minor Circle  i - iv - VII - III", "Circle of 5ths", 2, { 0, 3, 6, 2 }, {  } },
        { "Dorian Circle  i - iv - VII - III", "Circle of 5ths", 1, { 0, 3, 6, 2 }, {  } },
        { "Jackin' Minor  i - iv", "Jackin' & funky", 2, { 0, 3 }, {  } },
        { "Jackin' Groove  i - VII - VI - v", "Jackin' & funky", 2, { 0, 6, 5, 4 }, {  } },
        { "Soulful Gospel  I - iii - IV - V", "Soulful & Gospel", 0, { 0, 2, 3, 4 }, {  } },
        { "Deep Dorian  i - ii - IV - i", "Deep house", 1, { 0, 1, 3, 0 }, {  } },
        { "Funky Mixo  I - IV - VII - IV", "Jackin' & funky", 3, { 0, 3, 6, 3 }, {  } },
        { "Gospel Plagal  I - iv - V - I", "Soulful & Gospel", 9, { 0, 3, 4, 0 }, {  } },
        { "Lydian Dom Funk  I7#11 - II", "Modal & exotic", 10, { 0, 1 }, {  } },
        { "Soul Borrowed  I - iv - bVII - I", "Soulful & Gospel", 11, { 0, 3, 6, 0 }, {  } },
        { "Dark Deep  I - II (Phrygian dom.)", "Modal & exotic", 12, { 0, 1 }, {  } },
        { "Phrygian Deep  i - II - i - vii", "Modal & exotic", 5, { 0, 1, 0, 6 }, {  } },
        { "Deep House Classic  vi - IV - I - V", "Deep house", 0, { 5, 3, 0, 4 }, {  } },
        { "Jackin' Dorian  i - IV - i - v", "Jackin' & funky", 1, { 0, 3, 0, 4 }, {  } },
        { "Neo Soul  ii - iii - IV - V", "Soulful & Gospel", 0, { 1, 2, 3, 4 }, {  } },
        { "Jackin' Staccato  i - III - IV", "Jackin' & funky", 1, { 0, 2, 3, 3 }, {  } },
        { "Jack Swing  i - v - VII - IV", "Jackin' & funky", 1, { 0, 4, 6, 3 }, {  } },
        { "Funky Dominant  I7 - IV7", "Jackin' & funky", 3, { 0, 3 }, {  } },
        { "Disco Jack  ii - V - I - vi", "Jackin' & funky", 0, { 1, 4, 0, 5 }, {  } },
        { "Ghetto Funk  i - iv - v - iv", "Jackin' & funky", 2, { 0, 3, 4, 3 }, {  } },
        { "Phrygian Jack  i - bII", "Jackin' & funky", 5, { 0, 1 }, {  } },
        { "Lydian Jack  I7 - bVII+", "Jackin' & funky", 10, { 0, 6, 0, 6 }, {  } },
        { "Chicago Bounce  i - VII - i - IV", "Jackin' & funky", 1, { 0, 6, 0, 3 }, {  } },
        { "Maj9 Drift  I - bIII - bVI - bVII", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 3, 8, 10 } },
        { "Deep Planing  I - bVII - bVI - bVII", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 10, 8, 10 } },
        { "Borrowed iv  I - IV - iv - I", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 5, 17, 0 } },
        { "Minor Plagal  vi - IV - iv - I", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 21, 5, 17, 0 } },
        { "Backdoor  ii - bVII7 - I", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 14, 34, 0, 0 } },
        { "Dorian Lift  i - bIII - IV - i", "Chromatic & borrowed", 1, { 0, 0, 0, 0 }, { 12, 3, 5, 12 } },
        { "Neapolitan Deep  i - bII - i - v", "Chromatic & borrowed", 2, { 0, 0, 0, 0 }, { 12, 1, 12, 19 } },
        { "Minor Dominant  vi - III7 - vi - IV", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 21, 28, 21, 5 } },
        { "Floating Maj7  IV - V - iii - bIII", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 5, 7, 16, 3 } },
        { "Mediant Shift  I - iii - bIII - IV", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 16, 3, 5 } },
        { "Lydian Turn  I - II - iv - I", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 2, 17, 0 } },
        { "Picardy House  i - iv - V7 - I", "Chromatic & borrowed", 2, { 0, 0, 0, 0 }, { 12, 17, 31, 0 } },
        { "Deep Maj9 Walk  I - bVI - IV - bVII", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 8, 5, 10 } },
        { "Gospel Flat VII  IV - bVII7 - I - vi", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 5, 34, 0, 21 } },
        { "Minor Glide  i - bVII - bVI - V7", "Chromatic & borrowed", 2, { 0, 0, 0, 0 }, { 12, 10, 8, 31 } },
        { "Secondary Soul  I - VI7 - ii - V7", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 33, 14, 31 } },
        { "Soul Cycle  I - iii - vi - IV", "Soulful & Gospel", 0, { 0, 2, 5, 3 }, {  } },
        { "Sunday Gospel  IV - iii - vi - ii - V", "Soulful & Gospel", 0, { 3, 2, 5, 1, 4 }, {  } },
        { "Uplift  vi - V - IV - V", "Soulful & Gospel", 0, { 5, 4, 3, 4 }, {  } },
        { "Two-Five Lift  ii - V - iii - vi", "Soulful & Gospel", 0, { 1, 4, 2, 5 }, {  } },
        { "Gospel Walk-up  I - ii - iii - IV", "Soulful & Gospel", 0, { 0, 1, 2, 3 }, {  } },
        { "Classic Soul  I - IV - ii - V", "Soulful & Gospel", 0, { 0, 3, 1, 4 }, {  } },
        { "Sunrise  I - V - vi - IV", "Soulful & Gospel", 0, { 0, 4, 5, 3 }, {  } },
        { "Deep Soul  ii - V - I - IV", "Soulful & Gospel", 0, { 1, 4, 0, 3 }, {  } },
        { "Vocal Lift  IV - V - iii - vi - ii - V - I - I", "Soulful & Gospel", 0, { 3, 4, 2, 5, 1, 4, 0, 0 }, {  } },
        { "Church Turn  I - I7 - IV - iv", "Soulful & Gospel", 0, { 0, 0, 0, 0 }, { 0, 24, 5, 17 } },
        { "Gospel Secondary  I - III7 - vi - IV", "Soulful & Gospel", 0, { 0, 0, 0, 0 }, { 0, 28, 21, 5 } },
        { "Deep Minor Vamp  i - VI", "Deep house", 2, { 0, 5 }, {  } },
        { "Dorian Swing  i - IV - v - IV", "Deep house", 1, { 0, 3, 4, 3 }, {  } },
        { "Late Night Minor  i - v - VI - iv", "Deep house", 2, { 0, 4, 5, 3 }, {  } },
        { "Deep Float  ii - I", "Deep house", 0, { 1, 0 }, {  } },
        { "Deep 7ths  vi - iii - IV - I", "Deep house", 0, { 5, 2, 3, 0 }, {  } },
        { "Minor Two  i - ii", "Deep house", 1, { 0, 1 }, {  } },
        { "Midnight  iv - i - VII - i", "Deep house", 2, { 3, 0, 6, 0 }, {  } },
        { "Deep Soul Minor  i - iv - VII - VI", "Deep house", 2, { 0, 3, 6, 5 }, {  } },
        { "Dorian Night  i - III - IV - ii", "Deep house", 1, { 0, 2, 3, 1 }, {  } },
        { "Jack Vamp  I7 - IV7 - I7 - V7", "Jackin' & funky", 3, { 0, 0, 0, 0 }, { 24, 29, 24, 31 } },
        { "Funky Dorian  i - IV - VII - IV", "Jackin' & funky", 1, { 0, 3, 6, 3 }, {  } },
        { "Chicago Shuffle  I - vi - IV - V", "Jackin' & funky", 0, { 0, 5, 3, 4 }, {  } },
        { "Disco Climb  I - ii - iii - ii", "Jackin' & funky", 0, { 0, 1, 2, 1 }, {  } },
        { "Jackin' Two  i - VII", "Jackin' & funky", 2, { 0, 6 }, {  } },
        { "Ghetto Dorian  i - ii - i - IV", "Jackin' & funky", 1, { 0, 1, 0, 3 }, {  } },
        { "Funk Turn  I7 - bVII7 - IV7 - I7", "Jackin' & funky", 3, { 0, 0, 0, 0 }, { 24, 34, 29, 24 } },
        { "Long Circle  iii - vi - ii - V - I", "Circle of 5ths", 0, { 2, 5, 1, 4, 0 }, {  } },
        { "Minor Circle Down  i-iv-VII-III-VI-ii-v-i", "Circle of 5ths", 2, { 0, 3, 6, 2, 5, 1, 4, 0 }, {  } },
        { "Secondary Circle  III7 - VI7 - II7 - V7", "Circle of 5ths", 0, { 0, 0, 0, 0 }, { 28, 33, 26, 31 } },
        { "Soul Circle  vi - ii - V - I - IV", "Circle of 5ths", 0, { 5, 1, 4, 0, 3 }, {  } },
        { "Soul Minor iv  I - iii - IV - iv", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 16, 5, 17 } },
        { "Flat Six Seven Lift  IV - bVI - bVII - I", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 5, 8, 10, 0 } },
        { "Tritone Soul  ii - bII7 - I", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 14, 25, 0, 0 } },
        { "Deep Chromatic  i - bVII - bVI - v", "Chromatic & borrowed", 2, { 0, 0, 0, 0 }, { 12, 10, 8, 19 } },
        { "Major Minor Swap  I - i - IV - iv", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 12, 5, 17 } },
        { "Mixo Vamp  I - v - IV - I", "Modal & exotic", 3, { 0, 4, 3, 0 }, {  } },
        { "Lydian Glow  I - II - vii - I", "Modal & exotic", 4, { 0, 1, 6, 0 }, {  } },
        { "Phrygian Minor  i - bII - bIII - bII", "Modal & exotic", 5, { 0, 1, 2, 1 }, {  } },
        { "Minor Drama  i - iv - V7 - i", "Modal & exotic", 2, { 0, 0, 0, 0 }, { 12, 17, 31, 12 } },
        { "Dorian Climb  i - VII - ii - IV", "Deep house", 1, { 0, 6, 1, 3 }, {  } },
        { "Dorian Return  i - v - IV - i", "Deep house", 1, { 0, 4, 3, 0 }, {  } },
        { "Dorian Sway  i - VII - VII - IV", "Deep house", 1, { 0, 6, 6, 3 }, {  } },
        { "Dorian Turn  i - IV - ii - VII", "Deep house", 1, { 0, 3, 1, 6 }, {  } },
        { "Dorian Pull  i - ii - v", "Deep house", 1, { 0, 1, 4 }, {  } },
        { "Dorian Step Down  i - v - IV - III", "Deep house", 1, { 0, 4, 3, 2 }, {  } },
        { "Dorian Glide  i - VII - IV", "Deep house", 1, { 0, 6, 3 }, {  } },
        { "Dorian Lift  i - IV - ii - IV", "Deep house", 1, { 0, 3, 1, 3 }, {  } },
        { "Dorian Rise  i - ii - v - v", "Deep house", 1, { 0, 1, 4, 4 }, {  } },
        { "Dorian Walk  i - IV - ii - III", "Deep house", 1, { 0, 3, 1, 2 }, {  } },
        { "Dorian Bloom  i - VII - III - IV", "Deep house", 1, { 0, 6, 2, 3 }, {  } },
        { "Dorian Drift  i - III - VII - IV", "Deep house", 1, { 0, 2, 6, 3 }, {  } },
        { "Dorian Hold  i - VII - IV - IV", "Deep house", 1, { 0, 6, 3, 3 }, {  } },
        { "Dorian Stairs  i - ii - III - III", "Deep house", 1, { 0, 1, 2, 2 }, {  } },
        { "Dorian Arrival  III - IV - v - i", "Deep house", 1, { 2, 3, 4, 0 }, {  } },
        { "Dorian Fall  i - VII - v - IV", "Deep house", 1, { 0, 6, 4, 3 }, {  } },
        { "Dorian Echo  i - ii - IV - ii", "Deep house", 1, { 0, 1, 3, 1 }, {  } },
        { "Dorian Fold  IV - III - i", "Deep house", 1, { 3, 2, 0 }, {  } },
        // More for the lead.
        { "Italo Anthem  vi - IV - V - iii", "Disco & '90s", 0, { 5, 3, 4, 2 }, {  } },
        { "90s Rave  i - VI - VII - i", "Disco & '90s", 2, { 0, 5, 6, 0 }, {  } },
        { "Piano Anthem  I - V - vi - iii - IV", "Disco & '90s", 0, { 0, 4, 5, 2, 3 }, {  } },
        { "Boogie Down  i - IV9", "Disco & '90s", 0, { 0, 0 }, { 12, 29 } },
        { "Disco Strut  ii - V - ii - V", "Disco & '90s", 0, { 1, 4, 1, 4 }, {  } },
        { "Mirrorball  I - bIII - IV - I", "Disco & '90s", 0, { 0, 0, 0, 0 }, { 0, 3, 5, 0 } },
        { "Funky Town  I7 - bVII - IV - I7", "Disco & '90s", 0, { 0, 0, 0, 0 }, { 24, 10, 5, 24 } },
        { "Classic 90s  I - IV - vi - V", "Disco & '90s", 0, { 0, 3, 5, 4 }, {  } },
        { "Euro Minor  i - III - VII - VI", "Disco & '90s", 2, { 0, 2, 6, 5 }, {  } },
        { "Garage Minor  i - VII - v - VI", "Garage & UK", 2, { 0, 6, 4, 5 }, {  } },
        { "Speed Garage  i - iv - i - VII", "Garage & UK", 2, { 0, 3, 0, 6 }, {  } },
        { "2-Step Soul  ii - iii - vi - V", "Garage & UK", 0, { 1, 2, 5, 4 }, {  } },
        { "UK Funky  i - v - iv - i", "Garage & UK", 2, { 0, 4, 3, 0 }, {  } },
        { "Garage Lift  IV - V - iii - vi - ii", "Garage & UK", 0, { 3, 4, 2, 5, 1 }, {  } },
        { "Afro Modal  i - VII - i - VII", "Afro & Latin", 1, { 0, 6, 0, 6 }, {  } },
        { "Afro Lift  i - ii - III - ii", "Afro & Latin", 1, { 0, 1, 2, 1 }, {  } },
        { "Latin Tumbao  i - iv - V - i", "Afro & Latin", 6, { 0, 3, 4, 0 }, {  } },
        { "Son Montuno  I - IV - V - IV", "Afro & Latin", 0, { 0, 3, 4, 3 }, {  } },
        { "Highlife  I - IV - I - V", "Afro & Latin", 0, { 0, 3, 0, 4 }, {  } },
        { "Afro Phrygian  I - bII", "Afro & Latin", 12, { 0, 1, 0, 1 }, {  } },
        { "Bossa Turn  ii - V - I - VI7", "Afro & Latin", 0, { 0, 0, 0, 0 }, { 14, 31, 0, 33 } },
        { "Neo Soul Approach  IV - iii - vi - V/vi", "Neo soul & jazz", 0, { 0, 0, 0, 0 }, { 5, 16, 21, 28 } },
        { "Rhodes Drift  I - iv - iii - bIII", "Neo soul & jazz", 0, { 0, 0, 0, 0 }, { 0, 17, 16, 3 } },
        { "Dilla Turn  ii - V - iii - VI7", "Neo soul & jazz", 0, { 0, 0, 0, 0 }, { 14, 31, 16, 33 } },
        { "Minor Two-Five  ii - V - i", "Neo soul & jazz", 6, { 1, 4, 0, 0 }, {  } },
        { "Tritone Turn  I - bII7", "Neo soul & jazz", 0, { 0, 0 }, { 0, 25 } },
        { "Soul Ballad  I - vi - IV - iv", "Neo soul & jazz", 0, { 0, 0, 0, 0 }, { 0, 21, 5, 17 } },
        { "Lydian Soul  I - II - iii - II", "Neo soul & jazz", 4, { 0, 1, 2, 1 }, {  } },
        { "Jazz Waltz  I - vi - ii - V - iii - VI7 - ii - V", "Neo soul & jazz", 0, { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 21, 14, 31, 16, 33, 14, 31 } },
        { "Chill Sus  I7sus - IV - I7sus - bVII", "Deep house", 0, { 0, 0, 0, 0 }, { 24, 5, 24, 10 } },
        { "Deep Lydian  I - II - I - vii", "Deep house", 4, { 0, 1, 0, 6 }, {  } },
        { "Sunset  IV - I - V - vi", "Deep house", 0, { 3, 0, 4, 5 }, {  } },
        { "Moonlight  vi - iii - IV - V", "Deep house", 0, { 5, 2, 3, 4 }, {  } },
        { "Ambient Minor  i - VI - iv - v", "Deep house", 2, { 0, 5, 3, 4 }, {  } },
        { "Deep Mixo  I - v - VII - IV", "Deep house", 3, { 0, 4, 6, 3 }, {  } },
        { "Shout  IV - V - vi - V/vi", "Soulful & Gospel", 0, { 0, 0, 0, 0 }, { 5, 7, 21, 28 } },
        { "Praise  I - V/vi - vi - IV - V", "Soulful & Gospel", 0, { 0, 0, 0, 0, 0 }, { 0, 28, 21, 5, 7 } },
        { "Church Cadence  IV - iv - I - VI7 - ii - V - I", "Soulful & Gospel", 0, { 0, 0, 0, 0, 0, 0, 0 }, { 5, 17, 0, 33, 14, 31, 0 } },
        { "Gospel Descent  vi - V - IV - iii - ii - V - I", "Soulful & Gospel", 0, { 5, 4, 3, 2, 1, 4, 0 }, {  } },
        { "Circle 7 Down  I - IV - vii - iii - vi - ii - V", "Circle of 5ths", 0, { 0, 3, 6, 2, 5, 1, 4 }, {  } },
        { "Dominant Chain  I - VI7 - II7 - V7", "Circle of 5ths", 0, { 0, 0, 0, 0 }, { 0, 33, 26, 31 } },
        { "Minor Circle Five  i - iv - VII - III - VI", "Circle of 5ths", 2, { 0, 3, 6, 2, 5 }, {  } },
        { "Half Circle  ii - V - I", "Circle of 5ths", 0, { 1, 4, 0 }, {  } },
        { "Reverse Circle  I - V - ii - vi", "Circle of 5ths", 0, { 0, 4, 1, 5 }, {  } },
        // Dance stabs: minor vamps a power chord can sit on. Appended, so older indexes stay put.
        { "Power Minor  i - VII - VI - VII", "Dance stabs", 2, { 0, 6, 5, 6 }, {  } },
        { "Club Staccato  i - VI - VII - i", "Dance stabs", 2, { 0, 5, 6, 0 }, {  } },
        { "One Chord Drive  i", "Dance stabs", 2, { 0, 0, 0, 0 }, {  } },
        { "Drop Lift  i - iv - VII - i", "Dance stabs", 2, { 0, 3, 6, 0 }, {  } },
        { "Minor Power  i - VII - i - VI", "Dance stabs", 2, { 0, 6, 0, 5 }, {  } },
    };
    return list;
}

const std::vector<const char*>& progressionCategories()
{
    static const std::vector<const char*> categories { "Soulful & Gospel", "Deep house", "Jackin' & funky", "Disco & '90s", "Garage & UK",
                                                       "Afro & Latin", "Neo soul & jazz", "Circle of 5ths", "Modal & exotic",
                                                       "Chromatic & borrowed", "Dance stabs" };
    return categories;
}

std::vector<Chord> progressionChords (int progression, int key, int colour)
{
    const auto& list = progressions();
    const auto& p = list[(size_t) std::clamp (progression, 0, (int) list.size() - 1)];
    std::vector<Chord> chords;
    if (! p.fixed.empty())
        for (const auto& [semitones, type] : p.fixed)
            chords.push_back ({ mod12 (key + semitones), (int) type });
    else if (! p.codes.empty())
        for (size_t i = 0; i < p.codes.size(); ++i)
        {
            // A code that is a chord of the scale is built from the scale (it takes its 7th).
            const int code = p.codes[i];
            int degree = -1;
            for (int d = 0; d < 7 && degree < 0; ++d)
            {
                const auto c = diatonicChord (0, p.scale, d, (int) ChordColour::sevenths);
                const auto t = (ChordType) c.type;
                const int kind = code / 12;
                const bool same = c.root == code % 12
                               && (kind == 1 ? (t == ChordType::m7 || t == ChordType::minor)
                                 : kind == 2 ? t == ChordType::dom7
                                             : (t == ChordType::maj7 || t == ChordType::dom7 || t == ChordType::major));
                if (same)
                    degree = d;
            }
            chords.push_back (degree >= 0 ? diatonicChord (key, p.scale, degree, colour) : codeChord (key, code, colour));
        }
    else
        for (int d : p.degrees)
            chords.push_back (diatonicChord (key, p.scale, d, colour));
    if ((int) chords.size() > maxChords)
        chords.resize ((size_t) maxChords);
    return chords;
}

//==============================================================================
void normalisePattern (LeadPattern& p, int minimumLength)
{
    size_t length = (size_t) std::max (1, minimumLength);
    for (const auto& line : p.lanes)
        length = std::max (length, line.size());

    for (auto& line : p.lanes)
    {
        for (auto& c : line)
            if (! isHit (c))
                c = '.';

        if (line.empty())
            line.assign (length, '.');

        // A one-bar line in a two-bar groove plays twice.
        const std::string base = line;
        while (line.size() < length)
            line += base.substr (0, std::min (base.size(), length - line.size()));
    }
}

void fitPattern (LeadPattern& p, int length)
{
    length = std::max (1, length);
    for (auto& line : p.lanes)
    {
        for (auto& c : line)
            if (! isHit (c))
                c = '.';
        if (line.empty())
            line.assign ((size_t) length, '.');
        const std::string base = line;
        while ((int) line.size() < length)
            line += base.substr (0, std::min (base.size(), (size_t) length - line.size()));
        line.resize ((size_t) length);
    }
}

namespace
{
    std::string compact (const char* s)
    {
        std::string out;
        for (; *s != 0; ++s)
            if (*s != ' ')
                out += *s;
        return out;
    }

    struct Line { Lane lane; const char* steps; };

    LeadPattern groove (const char* name, const char* category, std::initializer_list<Line> lines)
    {
        LeadPattern p;
        p.name = name;
        p.category = category;
        for (const auto& l : lines)
            p[l.lane] = compact (l.steps);
        normalisePattern (p);
        return p;
    }

    constexpr auto DN = Lane::hook, UP = Lane::answer, ST = Lane::stab, CK = Lane::chank, SC = Lane::scratch,
                   SL = Lane::slide, SW = Lane::swell, AR = Lane::arp, LK = Lane::lick, OC = Lane::octave,
                   PW = Lane::power, RT = Lane::root;

    // The muted scratch fills the 16ths no other chord hit plays (the engine leaves it out
    // where one does): a funk player's hand never stops.
    constexpr const char* allScratch = "gggg gggg gggg gggg";
    constexpr const char* offbeats   = "..X. ..X. ..X. ..X.";
}

const std::vector<LeadPattern>& leadPatterns()
{
    static const std::vector<LeadPattern> patterns {
        // ---- Piano House: bright offbeat chords and an open strum ----
        groove ("Offbeat Chords", "Piano House",
                { { ST, offbeats }, { SC, "g... g... g... g.g." } }),
        groove ("House Hook", "Piano House",
                { { DN, "X... ..x. X... x.x." }, { UP, "...x .x.. ...x ...x" }, { SC, ".g.. g... .g.. .g.." } }),
        groove ("Piano House Pulse", "Piano House",
                { { CK, "..x. ..x. ..xx ..x." }, { ST, "X... .... .... ...." }, { RT, "X... .... X... ...." } }),
        groove ("Uplifting Chords", "Piano House",
                { { DN, "X... .... X... ...." }, { UP, "..x. ..x. ..x. .xx." }, { SC, ".... g... .... g..." } }),

        // ---- Soulful: warm repeats and chanks under the vocal ----
        groove ("Soulful Sunday", "Soulful",
                { { DN, "X... ..x. .... x..." }, { UP, "...x .... .x.. ...x" }, { SC, "..g. g... g.g. ..g." } }),
        groove ("Soul Pulse & Root", "Soulful",
                { { CK, "..x. ..x. ..x. ..x." }, { RT, "X... .... X..x ...." }, { SC, ".g.. .g.. .g.. .g.g" } }),
        groove ("Soulful Sixteenths", "Soulful",
                { { DN, "X.g. x.g. X.g. x.g." }, { UP, ".g.x .g.x .g.x .x.x" } }),
        groove ("Sunday Arp", "Soulful",
                { { AR, "x.xx x.xx x.xx x.xx" }, { SW, "X... .... .... ...." } }),

        // ---- Deep: dub chords that echo, swells and soft picking ----
        groove ("Deep Long note", "Deep",
                { { SW, "X... .... .... ...." }, { CK, ".... ..x. .... ..g." } }),
        groove ("Deep Chord Echo", "Deep",
                { { ST, "..X. .... ..x. ...g" }, { SC, ".... .... .... g..." } }),
        groove ("Deep Arp", "Deep",
                { { AR, "x.x. x.x. x.x. x.x." }, { SW, "X... .... .... .... .... .... .... ...." } }),
        groove ("Late Night Staccato", "Deep",
                { { ST, "...X .... ..x. .... ...X .... .... ..x." }, { RT, "X... .... .... .... X... .... .... ...." } }),

        // ---- Gospel: big eighth-note repeats and shouts ----
        groove ("Gospel Hook", "Gospel",
                { { DN, "X... X... X... X..." }, { UP, "..x. ..x. ..x. ..x." } }),
        groove ("Gospel Shout", "Gospel",
                { { ST, "X..x ..x. X..x ..x." }, { RT, "X... .... X... ...." } }),
        groove ("Church Arp", "Gospel",
                { { AR, "x.xx x.xx x.xx x.xx" }, { SW, "X... .... .... ...." } }),

        // ---- Jackin': raw stabs and chanks with the swing ----
        groove ("Jack Staccato", "Jackin'",
                { { ST, offbeats }, { SC, allScratch } }),
        groove ("Chicago Pulse", "Jackin'",
                { { CK, "x.xx .x.x x.xx .x.x" }, { ST, ".... .... .... ..X." } }),
        groove ("Jack High motif", "Jackin'",
                { { OC, "x..x ..x. x..x ..x." }, { ST, "..X. .... ..X. ...." } }),

        // ---- Jackin' Piano: bouncy stabs ----
        groove ("Bouncy Staccatos", "Jackin' Piano",
                { { ST, "X..X ..X. .X.. X.X." }, { SC, ".g.. g... g.g. .g.." } }),
        groove ("Jack Hook", "Jackin' Piano",
                { { DN, "X... X.x. X... X.x." }, { UP, "..x. ...x ..x. ...x" } }),
        groove ("Pumping Chords", "Jackin' Piano",
                { { ST, offbeats }, { PW, "X... X... X... X..." } }),

        // ---- Garage & UK: two-step chops and skanks ----
        groove ("Two-Step Chops", "Garage & UK",
                { { ST, "X... ..x. ...x ...." }, { SC, "..g. g... g... g.g." } }),
        groove ("Garage Skank", "Garage & UK",
                { { CK, "..x. ..x. .x.. ..x." }, { SL, "...X .... .... ...." } }),
        groove ("UK Long note & Staccato", "Garage & UK",
                { { SW, "X... .... .... ...." }, { ST, ".... ..X. .... ...x" } }),

        // ---- Afro & Latin: highlife picking, montunos and clave chanks ----
        groove ("Afro Highlife", "Afro & Latin",
                { { AR, "x.x. xx.x x.x. xx.x" }, { RT, "X... .... X... ...." } }),
        groove ("Afro Pulse", "Afro & Latin",
                { { CK, "x..x ..x. ..x. x..." }, { RT, "X... .... X... ...." }, { SC, ".g.. g..g .g.. .g.g" } }),
        groove ("Latin Montuno", "Afro & Latin",
                { { AR, "x.xx .xx. x.xx .xx." }, { OC, "X... .... .... ...." } }),

        // ---- Funky & Disco: the chicken scratch, octaves, sixteenths ----
        groove ("Disco Ghost", "Funky & Disco",
                { { DN, "X... ..x. ..x. x..." }, { UP, ".x.x ...x .x.. .x.x" }, { SC, "..g. g... g... ..g." } }),
        groove ("Disco High motif", "Funky & Disco",
                { { OC, "X.x. x.x. X.x. x.x." }, { SC, ".g.g .g.g .g.g .g.g" } }),
        groove ("Funk Sixteenths", "Funky & Disco",
                { { DN, "X... .... X... ...." }, { UP, ".... ..x. .... ..x." }, { SC, allScratch } }),
        groove ("Disco Pulse Line", "Funky & Disco",
                { { CK, "x.xx .xx. x.xx .x.x" }, { ST, ".... X... .... X..." } }),

        // ---- '90s Piano House: strummed anthems and power chords ----
        groove ("90s Anthem Hook", "'90s Piano House",
                { { DN, "X... ..x. ..x. X..." }, { UP, ".... x... .... ..x." }, { RT, "X... X... X... X..." } }),
        groove ("Rave Power", "'90s Piano House",
                { { PW, "X... ..X. ..X. X..." }, { ST, ".... .... .... ..x." } }),
        groove ("Italo Offbeat", "'90s Piano House",
                { { ST, offbeats }, { OC, "X... X... X... X..." } }),

        // ---- Neo Soul & Broken: lazy chords and licks ----
        groove ("Neo Soul Lazy", "Neo Soul & Broken",
                { { ST, "X... ...x ..x. ...." }, { LK, ".... ..x. .... .x.x" } }),
        groove ("Broken Hook", "Neo Soul & Broken",
                { { DN, "X... ..x. .... ...." }, { UP, ".... .... .x.. ...." }, { LK, ".... .... .... x.x." },
                  { SL, ".... .... .... .... X... ..x. .... ...." } }),
        groove ("Neo Arp", "Neo Soul & Broken",
                { { AR, "x.x. .x.x x.x. .x.x" }, { RT, "X... .... ..x. ...." } }),

        // ---- Chill & Pads: swells and soft picking ----
        groove ("Chill Long note", "Chill & Pads",
                { { SW, "X... .... .... .... X... .... .... ...." }, { LK, ".... .... ..x. .... .... ..x. .x.. x.x." } }),
        groove ("Chill Arpeggio", "Chill & Pads",
                { { AR, "x.x. x.x. x.x. x.x." }, { SW, "X... .... .... ...." } }),
        groove ("Sunset Chords", "Chill & Pads",
                { { DN, "f... .... ..x. ...." }, { UP, ".... ..x. .... .x.." } }),

        // ---- Dance Stabs: big straight club hits ----
        groove ("Dance Power", "Dance Stabs",
                { { PW, "X.x. X.x. X.x. X.x." } }),
        groove ("Big Staccatos", "Dance Stabs",
                { { ST, "X... ..X. ..X. X..." }, { SC, ".g.. g... g... .g.." } }),
        groove ("Offbeat Power", "Dance Stabs",
                { { PW, offbeats }, { RT, "X... X... X... X..." } }),
    };
    return patterns;
}

const std::vector<const char*>& grooveCategories()
{
    static const std::vector<const char*> categories { "Piano House", "Soulful", "Deep", "Gospel", "Jackin'", "Jackin' Piano", "Garage & UK",
                                                       "Afro & Latin", "Funky & Disco", "'90s Piano House", "Neo Soul & Broken",
                                                       "Chill & Pads", "Dance Stabs" };
    return categories;
}

int categoryIndex (const std::string& category) noexcept
{
    const auto& c = grooveCategories();
    for (size_t i = 0; i < c.size(); ++i)
        if (category == c[i])
            return (int) i;
    return -1;
}

namespace
{
    const std::vector<std::vector<int>>& categoryIndexTable()
    {
        static const auto table = []
        {
            std::vector<std::vector<int>> t (grooveCategories().size());
            const auto& presets = leadPatterns();
            for (size_t g = 0; g < t.size(); ++g)
                for (size_t i = 0; i < presets.size(); ++i)
                    if (presets[i].category == grooveCategories()[g])
                        t[g].push_back ((int) i);
            return t;
        }();
        return table;
    }
}

const std::vector<int>& patternsOfCategory (int category)
{
    static const std::vector<int> none;
    const auto& t = categoryIndexTable();
    return category >= 0 && category < (int) t.size() ? t[(size_t) category] : none;
}

const std::vector<int>& patternsByCategory()
{
    static const auto order = []
    {
        std::vector<int> o;
        for (const auto& list : categoryIndexTable())
            o.insert (o.end(), list.begin(), list.end());
        return o;
    }();
    return order;
}

float genreSwing (int category) noexcept
{
    // The same swing per genre as Fase Groove Drums, so the lead sits in the drums' pocket.
    static const float swing[] { 0.45f, 0.2f, 0.5f, 0.3f, 0.8f, 0.6f, 0.75f, 0.35f, 0.3f, 0.45f, 0.9f, 0.3f, 0.1f };
    return category >= 0 && category < (int) std::size (swing) ? swing[category] : 0.2f;
}

const char* genreProgressionCategory (int category) noexcept
{
    // Funky stays in the funk list (not Italo / '90s pop). Dance stabs stay on minor power vamps.
    static const char* categories[] { "Soulful & Gospel", "Soulful & Gospel", "Deep house", "Soulful & Gospel", "Jackin' & funky",
                                      "Jackin' & funky", "Garage & UK", "Afro & Latin", "Jackin' & funky", "Disco & '90s",
                                      "Neo soul & jazz", "Deep house", "Dance stabs" };
    return categories[std::clamp (category, 0, 12)];
}

LeadColour leadColourFor (int genre) noexcept
{
    switch (genre)
    {
        case 3: case 10:                 return LeadColour::shell;      // gospel, neo-soul
        case 0: case 5: case 9: case 12: return LeadColour::triad;       // piano house, jackin' piano, '90s, dance
        case 4: case 8:                  return LeadColour::funkGrip;   // jackin', funky
        default:                         return LeadColour::classic;
    }
}

int lickContourFor (int genre) noexcept
{
    switch (genre)
    {
        case 3:          return 1;   // gospel
        case 4: case 8:  return 2;   // jackin', funky
        case 10: case 11: return 3;  // neo-soul, chill
        case 7:          return 4;   // afro
        case 12:         return 5;   // dance
        default:         return 0;
    }
}

int genreProgression (int category) noexcept
{
    static const int progression[] { 4, 2, 1, 3, 7, 15, 8, 9, 5, 10, 11, 12, 13 };
    return category >= 0 && category < (int) std::size (progression) ? progression[category] : 0;
}

int randomPatternIndex() noexcept
{
    return (int) leadPatterns().size();
}

int userPatternIndex() noexcept
{
    return randomPatternIndex() + 1;
}

} // namespace fgl
