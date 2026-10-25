// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// Lead sequencer. Turns a musical-time window (in quarter notes, "ppq") into
// note on/off events. Pure C++ (no JUCE).

#include "Lead.h"
#include "Generators.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fgl
{

/** How a hit is played, for a sampled lead with keyswitches: let ring, palm muted, a dead
    (muted) note, a slide. */
enum class Articulation { sustain = 0, palmMute, dead, slide };
constexpr int numArticulations = 4;
const char* articulationName (int articulation) noexcept;   // "Normal", "Short" ...

/** The lane of a keyswitch note in the note stream (not a string of the lead). */
constexpr int keyswitchLane = numLanes;

struct LeadSettings
{
    LeadSettings() noexcept;

    int key = 0;
    int scale = 0;
    int pattern = 0;
    int grooveBars = 2;                    // 2 (parts A-D of half a bar) or 4 (parts of one bar)
    float swing = 0.15f;                   // 0 = straight, 1 = triplet shuffle, up to 1.5 (a dotted 16th: the heaviest)
    int velocity = 100;
    float humanize = 0.2f;
    float accentGroove = 0.85f;             // up-repeats, scratches, chanks, picking: louder on the offbeat, softer 16ths (0 = flat)
    std::uint32_t humanizeSeed = 0;        // NEW FEEL: another, repeatable humanization

    int fillEveryBars = 0;                 // 0 = no fills
    FillStyle fillStyle = FillStyle::motifBuild;
    bool resolveAfterFill = true;            // a big strum on the bar after a fill (and every 8 bars of the drop)
    int variationEvery = 0;                // 0 = off, else every 4 or 8 bars the last bar varies

    Section section = Section::main;
    long long sectionStartBar = 0;         // where the section began (the drop's crash counts from there)

    std::uint32_t riffSeed = 1;            // used when pattern == randomPatternIndex()
    float density = 0.5f;
    int randomGenre = 0;                   // the genre NEW GROOVE writes in (grooveCategories() index)

    // The chords: one per bar, numChords bars round. When liveChord is set (notes held on the MIDI
    // input, e.g. from Fase Groove Piano), every bar plays that chord instead.
    std::array<Chord, maxChords> chords {};
    int numChords = 4;                     // the chords used, one per bar (1 to maxChords)
    bool useLiveChord = false;
    ChordTones liveChord {};
    float attackMs = 12.0f;                 // the time between two strings of a strum

    std::array<int, numLanes> octaves {};  // each lane up or down by octaves (0 = as voiced)
    std::array<float, numLanes> levels {}; // velocity scale of each lane (1 = as written)
    std::array<bool, numLanes> muted {};

    // The feel of each lane: how much of the swing it takes (1 = as the Swing control) and how
    // far it sits ahead (< 0) or behind (> 0) the beat, in milliseconds.
    std::array<float, numLanes> laneSwing {};
    std::array<float, numLanes> laneShift {};
    double tempo = 120.0;                  // turns the milliseconds into beats

    // A groove template (e.g. taken from an audio loop): when active, the timing and the level of
    // every 16th of the bar come from it instead of the swing and the accent groove.
    struct Template
    {
        bool active = false;
        std::array<float, 16> timing {};   // in 16ths, < 0 early, > 0 late
        std::array<float, 16> level {};    // 1 = as written
    };
    Template grooveTemplate;

    // Used when pattern == userPatternIndex(): the hand-edited grid, and the velocity set by
    // hand on each of its steps (0 = automatic, from the symbol).
    std::array<std::array<char, userPatternLength>, numLanes> user {};
    std::array<std::array<unsigned char, userPatternLength>, numLanes> userVelocity {};
    // ...and the chance each of those steps plays (0 or 100 = always) and its roll (0 or 1 = one
    // hit, 2-4 = that many quick repeats in the 16th).
    std::array<std::array<unsigned char, userPatternLength>, numLanes> userChance {};
    std::array<std::array<unsigned char, userPatternLength>, numLanes> userRoll {};

    // The fill drawn by hand (Fill style: Custom): one bar. It takes over from its first written
    // step; the lanes it doesn't use keep playing the groove.
    std::array<std::array<char, 16>, numLanes> customFill {};

    std::array<int, numSegments> segmentOrder { 0, 1, 2, 3 };   // A B C D
    std::uint32_t sectionVariation = 0;    // a song section's own version of the groove (0: as it is)

    // A song section with its own fill: when forcedFillBar >= 0 that is the only bar with a fill,
    // in forcedFillStyle (< 0: no fill at all).
    long long forcedFillBar = -1;
    int forcedFillStyle = -1;

    // How the lead is played.
    bool letRing = false;                  // repeats ring on until the next chord hit stops them
    bool roomForBand = false;              // the low strings left to the bass, the stabs above the piano
    bool pushChanges = true;               // the hits on the last 8th of a bar play the next chord
    bool keyswitches = false;              // articulation keyswitches (and the swell's volume) in the MIDI
    std::array<int, numArticulations> keyswitch { 24, 25, 26, 27 };   // their notes (below the lead)
};

struct NoteEvent
{
    double ppq;
    int note;
    int velocity;   // 0 for note-off
    int lane = 0;
    int controller = -1;   // >= 0: a controller change (CC number in `note`, value in `velocity`)
    int articulation = 0;  // how a note-on is played (Articulation)
    bool isNoteOn() const noexcept { return velocity > 0 && controller < 0; }
    bool isController() const noexcept { return controller >= 0; }
};

//==============================================================================
class GrooveEngine
{
public:
    GrooveEngine();

    /** Appends (time-ordered) events falling in [startPpq, endPpq). */
    void render (double startPpq, double endPpq, const LeadSettings& settings, std::vector<NoteEvent>& out);

    /** Releases everything currently sounding at the given time. */
    void allNotesOff (double ppq, std::vector<NoteEvent>& out);

    bool anyNoteSounding() const noexcept;

    /** The groove before the section is applied: a preset, the random groove or the user pattern. */
    static LeadPattern basePattern (const LeadSettings& s);

    /** The groove actually played: the base pattern, in the A-D order, its section's own
        version, then rewritten for the song section. */
    static LeadPattern resolvePattern (const LeadSettings& s);
    static int patternLength (const LeadSettings& s) noexcept;

    /** Length (in 16th steps) after which groove, chords, variations and fills all repeat. */
    static int loopLengthInSteps (const LeadSettings& s) noexcept;

    /** Offline render of a whole clip starting at bar 1, with every note closed. */
    static std::vector<NoteEvent> renderClip (const LeadSettings& s, double lengthInBeats);

    /** The groove and its turnaround for some settings, worked out ahead (off the audio thread)
        and handed to render with setPrepared; render builds its own when it doesn't match. */
    struct Prepared
    {
        LeadSettings key;
        LeadPattern pattern, variation;
    };
    static void prepare (Prepared&, const LeadSettings&);
    static bool samePattern (const LeadSettings& a, const LeadSettings& b) noexcept;
    void setPrepared (const Prepared* p) noexcept { prepared = p; }

    /** Renders a whole song (intro, main, break...) with section markers. */
    struct Marker { double ppq; std::string name; };
    static std::vector<NoteEvent> renderArrangement (const LeadSettings& s, const std::vector<ArrangementPart>& parts,
                                                     std::vector<Marker>* markers = nullptr);

    /** The fill style a bar plays ("Mix" picks one per bar). `genre` (a grooveCategories()
        index, -1 for the old house mix) weights the mix toward that style. */
    static FillStyle effectiveFillStyle (FillStyle style, long long bar, int genre = -1) noexcept;

    /** The chord tones of a bar: the chord of its slot, or the one held on the MIDI input. */
    static ChordTones chordAt (const LeadSettings& s, long long bar) noexcept;

    /** After how many bars the chords come round (fewer than numChords when they repeat inside). */
    static int chordPeriod (const LeadSettings& s) noexcept;

    /** The shapes of a bar's chord on the neck, each moving as little as it can from the one
        before (the progression's chain, the same every time round). */
    const ChordVoicing& voicingAt (const LeadSettings& s, long long bar);

private:
    struct PendingOn { double ppq; int note; int velocity; double duration; int lane; bool choke = false; std::size_t order = 0;
                       int articulation = 0; bool grace = false; int controller = -1; };

    const Prepared& patternFor (const LeadSettings& s);
    void collectStep (long long step, const LeadSettings& s, const LeadPattern& pattern, const LeadPattern& variation);
    void flushOffsUpTo (double ppq, bool inclusive, std::vector<NoteEvent>& out);

    std::array<double, 128> offTime {};   // < 0 when the note is not sounding
    std::array<int, 128> laneOf {};
    std::array<double, 128> onTime {};
    std::array<bool, 128> graceOf {};    // a slide's grace note: a new chord doesn't stop it
    int lastArticulation = -1;

    // The shapes of the chords, worked out when the chords change.
    std::array<ChordVoicing, maxChords> slotVoicings {};
    std::array<Chord, maxChords> voicedChords {};
    int voicedCount = -1;
    bool voicedBand = false;
    int voicedColour = -1;
    ChordTones liveVoiced { 0, 0 };
    ChordVoicing liveVoicing {};
    bool liveBand = false;
    int liveColour = -1;
    std::vector<PendingOn> pending;
    std::vector<int> scratch;

    // The resolved pattern: the one prepared ahead when it matches, else built here (and kept,
    // rebuilt only when its inputs change).
    const Prepared* prepared = nullptr;
    Prepared cache;
    bool cacheValid = false;
};

} // namespace fgl
