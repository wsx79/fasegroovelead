// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// Lead phrase lanes, harmonic context and family rhythm presets. Pure C++ (no JUCE).

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fgl
{

constexpr double stepLength = 0.25;   // a 16th note, in quarter notes

//==============================================================================
/** One row of the grid: one way of playing the chord of the bar. */
enum class Lane { hook = 0, answer, stab, chank, scratch, slide, swell, arp, lick, octave, power, root };
constexpr int numLanes = 12;

const char* laneName (int lane) noexcept;        // "Hook", "Ghost" ...
const char* laneShortName (int lane) noexcept;   // "DOWN", "SCRATCH" ... (the grid's row labels)

/** Every lead lane competes for the same monophonic voice. */
bool isChordLane (int lane) noexcept;

//==============================================================================
/** Chord qualities, as the chord slots show them. */
enum class ChordType { maj7 = 0, m7, dom7, m9, maj9, dom9, m11, dom13, sus4, sixNine, major, minor,
                       halfDim, dim, aug };   // (new types are appended: saved projects keep theirs)
constexpr int numChordTypes = 15;
const char* chordTypeName (int type) noexcept;   // "maj7", "m7", "7" ... ("" for a major triad)
/** The intervals of a chord type, as a bit mask over the 12 semitones above the root. */
unsigned chordTypeMask (int type) noexcept;

const char* pitchClassName (int pitchClass) noexcept;   // C, C#, D, Eb ...

struct Chord
{
    int root = 9;                          // pitch class: 0 = C ... 9 = A
    int type = (int) ChordType::m7;
    bool operator== (const Chord& o) const noexcept { return root == o.root && type == o.type; }
    bool operator!= (const Chord& o) const noexcept { return ! (*this == o); }
};
std::string chordName (const Chord&);           // "Am7", "Fmaj9", "C"

/** Chord tones: a root and the intervals above it (a bit mask, bit 0 = the root). A chord from
    the slots, or the notes held on the MIDI input. */
struct ChordTones
{
    int root = 9;
    unsigned mask = 0x489;                 // root, minor 3rd, 5th, minor 7th
    bool operator== (const ChordTones& o) const noexcept { return root == o.root && mask == o.mask; }
};
ChordTones tonesOf (const Chord&) noexcept;
/** The chord a set of held notes makes: the root is the note the others make the best known chord
    over (an inversion keeps its own root), the lowest one when nothing fits. mask == 0: nothing held. */
ChordTones tonesOfNotes (const std::vector<int>& notes) noexcept;
/** The same from the 128 MIDI notes, true where held: no allocation (for the audio thread).
    `count` gets how many notes are held. */
ChordTones tonesOfHeld (const std::array<bool, 128>& held, int& count) noexcept;

/** A few MIDI notes, lowest first, without allocating (for the audio thread). */
struct NoteList
{
    std::array<int, 8> notes {};
    int count = 0;

    void add (int n) noexcept           { if (count < (int) notes.size()) notes[(size_t) count++] = n; }
    void clear() noexcept               { count = 0; }
    int size() const noexcept           { return count; }
    bool empty() const noexcept         { return count == 0; }
    int front() const noexcept          { return notes[0]; }
    int back() const noexcept           { return notes[(size_t) std::max (0, count - 1)]; }
    int operator[] (int i) const noexcept { return notes[(size_t) i]; }
    int* begin() noexcept               { return notes.data(); }
    int* end() noexcept                 { return notes.data() + count; }
    const int* begin() const noexcept   { return notes.data(); }
    const int* end() const noexcept     { return notes.data() + count; }
    bool operator== (const NoteList& o) const noexcept { return count == o.count && std::equal (begin(), end(), o.begin()); }
};

/** Harmonic pitch palette used by the family phrase API. */
struct ChordVoicing
{
    NoteList full;      // chord tones in the lead register
    NoteList stab;      // melodic harmonic palette
    NoteList four;      // arpeggio palette
    NoteList stabAlt;   // answering phrase palette
};

/** Family genre palette categories, retained for harmonic context. */
enum class LeadColour { classic = 0, shell, triad, funkGrip };
constexpr int numLeadColours = 4;
/** The voicing a genre (grooveCategories() index) uses for a random groove. Presets stay classic. */
LeadColour leadColourFor (int genre) noexcept;
/** Run phrase of a genre: 0 is the preset contour. */
int lickContourFor (int genre) noexcept;

/** Builds the harmonic pitch palette. */
ChordVoicing voiceChord (const ChordTones& chord, const ChordVoicing* previous = nullptr, bool roomForBand = false,
                         LeadColour colour = LeadColour::classic);

/** Emits exactly one melodic pitch for a phrase position, tonal context and seed. */
void laneNotes (Lane lane, const ChordTones& chord, const ChordVoicing& voicing, int stepInBar, int octave,
                NoteList& out, bool roomForBand = false, bool alternative = false, int contour = 0, int key = 0, int scale = 0, std::uint32_t seed = 1) noexcept;

/** The same, from the chord's own shapes (no chord before it). */
std::vector<int> laneNotes (Lane lane, const ChordTones& chord, int stepInBar, int octave = 0);

//==============================================================================
/** The scales of Fase Groove Piano (the same order): Major, Dorian, Minor, Mixolydian, Lydian,
    Phrygian, Harmonic minor, Melodic minor, Locrian, Harmonic major, Lydian dominant,
    Mixolydian b6, Phrygian dominant. */
constexpr int numScales = 13;
const char* scaleName (int scale) noexcept;
const std::array<int, 7>& scaleIntervals (int scale) noexcept;
bool scaleIsMinor (int scale) noexcept;

/** How rich the chords built on a scale are: triads, 7ths or 9ths (a 9th only where it is a
    whole tone above the root, as a keyboard player tidies them up). */
enum class ChordColour { triads = 0, sevenths, ninths };
constexpr int numChordColours = 3;
const char* chordColourName (int colour) noexcept;

/** The chord on a degree (0..6) of a scale in a key. */
Chord diatonicChord (int key, int scale, int degree, int colour);

/** A chord outside the scale, as Fase Groove Piano stores it: semitones above the key + 12 for a
    minor chord, + 24 for a dominant one. */
Chord codeChord (int key, int code, int colour);

/** The most chords a progression has: one per bar, round and round. */
constexpr int maxChords = 8;

/** Chord progressions, by category: degrees of a scale (as Fase Groove Piano's), chords from the
    circle of fifths outside it (codes), or chords set one by one (fixed: semitones above the key
    and a type). */
struct Progression
{
    const char* name;
    const char* category;
    int scale = 0;
    std::vector<int> degrees {};
    std::vector<int> codes {};
    std::vector<std::pair<int, ChordType>> fixed {};
};
const std::vector<Progression>& progressions();
/** The progression browser's headings, in display order. */
const std::vector<const char*>& progressionCategories();
/** The chords of a progression in a key (1 to 8 of them). */
std::vector<Chord> progressionChords (int progression, int key, int colour = (int) ChordColour::ninths);

/** A new progression that walks the circle of fifths in a scale (each chord a fifth below the one
    before, landing on the tonic or the dominant), now and then with a secondary dominant on the
    way. Same seed = same progression. */
std::vector<Chord> circleOfFifthsProgression (std::uint32_t seed, int key, int scale, int length, int colour);

/** How well chord b sounds after chord a, 0 (clashes) to 1: shared notes and the root's move
    (down a fifth is the strongest). */
float transitionScore (const Chord& a, const Chord& b) noexcept;

//==============================================================================
/** A lead groove: one line per lane, one character per 16th step:
      'X' accent   'x' hit   'g' ghost (very soft)   'f' two-note repeat   '.' rest
    All lines have the same length (16 = one bar, 32 = two bars). */
struct LeadPattern
{
    std::string name;
    std::string category {};   // genre heading in the groove menu
    std::array<std::string, numLanes> lanes {};

    int length() const noexcept { return (int) lanes[0].size(); }
    std::string& operator[] (Lane l) noexcept             { return lanes[(size_t) l]; }
    const std::string& operator[] (Lane l) const noexcept { return lanes[(size_t) l]; }
};

inline bool isHit (char c) noexcept { return c == 'X' || c == 'x' || c == 'g' || c == 'f'; }

/** Every lane the same length (the longest one, shorter lines repeated), unknown symbols
    as rests: what every pattern goes through before it plays. */
void normalisePattern (LeadPattern&, int minimumLength = 16);

//==============================================================================
/** The preset grooves, grouped by genre (the same 13 genres as Fase Groove Piano and Drums). */
const std::vector<LeadPattern>& leadPatterns();

/** Genre headings of the groove menu, in display order. */
const std::vector<const char*>& grooveCategories();
int categoryIndex (const std::string& category) noexcept;   // -1 when unknown
/** The presets of a genre (grooveCategories() index), worked out once; empty for an unknown genre. */
const std::vector<int>& patternsOfCategory (int category);
/** Every preset of a known genre, genre by genre (the groove menu's order). */
const std::vector<int>& patternsByCategory();

/** Swing that suits a genre (0 = straight, 1 = full triplet shuffle). */
float genreSwing (int category) noexcept;

/** The progression that suits a genre (an index into progressions()). */
int genreProgression (int category) noexcept;
/** The progression category whose chords suit a genre (the dice pick there). */
const char* genreProgressionCategory (int category) noexcept;

/** Index of the "Random groove" entry, right after the preset grooves. */
int randomPatternIndex() noexcept;

/** Index of the "User pattern" entry (editable grid), after "Random groove". */
int userPatternIndex() noexcept;

constexpr int userPatternLength = 64;   // the user pattern: up to four bars of 16ths
constexpr int randomBeatLength = 32;    // a random groove: two bars

/** Every lane exactly `length` steps: longer lines are cut, shorter ones repeated. */
void fitPattern (LeadPattern&, int length);

} // namespace fgl
