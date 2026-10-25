// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include "LinkClient.h"
#include "LinkFile.h"
#include "PlayClock.h"
#include "RawParameters.h"
#include "RetireBin.h"

#include "GuideLead.h"
#include "engine/GrooveEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <memory>

/** What a grid-cell menu id does. Chance "Always" is id 200; rolls start at 201. */
struct StepMenuChoice
{
    enum class Kind { none, symbol, moveLeft, moveRight, chance, roll };
    Kind kind = Kind::none;
    int value = 0;   // symbol as a char, or a chance percent, or a roll count
};

inline StepMenuChoice stepMenuChoice (int result) noexcept
{
    static constexpr char symbols[] { 'X', 'x', 'g', 'f', '.' };
    if (result >= 1 && result <= 5)
        return { StepMenuChoice::Kind::symbol, (int) symbols[result - 1] };
    if (result == 6)
        return { StepMenuChoice::Kind::moveLeft, 0 };
    if (result == 7)
        return { StepMenuChoice::Kind::moveRight, 0 };
    if (result >= 201 && result <= 204)
        return { StepMenuChoice::Kind::roll, result - 200 };
    if (result >= 100 && result <= 200)
        return { StepMenuChoice::Kind::chance, result - 100 };
    return {};
}

/** Source chord slot of "the same chord as bar N", or -1. Slots run to maxChords, not four. */
inline int chordMenuCopySlot (int result) noexcept
{
    return result >= 300 && result < 300 + fgl::maxChords ? result - 300 : -1;
}

namespace ParamIDs
{
    inline constexpr const char* pattern     = "pattern";
    inline constexpr const char* swing       = "swing";
    inline constexpr const char* velocity    = "velocity";
    inline constexpr const char* humanize    = "humanize";
    inline constexpr const char* humanizeSeed = "humanizeSeed";
    inline constexpr const char* accents     = "accents";       // the accent groove of up-repeats, scratches, chanks and picking
    inline constexpr const char* fills       = "fills";
    inline constexpr const char* fillStyle   = "fillStyle";
    inline constexpr const char* resolveNote    = "resolveNote";      // a big strum after every fill
    inline constexpr const char* variation   = "variation";
    inline constexpr const char* section     = "section";
    inline constexpr const char* tempoSync   = "tempoSync";
    inline constexpr const char* tempo       = "tempo";
    inline constexpr const char* sequencer   = "sequencer";     // off: the groove doesn't play with the DAW
    inline constexpr const char* riffSeed    = "riffSeed";
    inline constexpr const char* density     = "density";
    inline constexpr const char* partLength  = "partLength";    // the parts A-D: half a bar (2-bar groove) or 1 bar (4-bar groove)
    inline constexpr const char* genre       = "genre";         // the genre NEW GROOVE writes in
    inline constexpr const char* midiChannel = "midiChannel";
    inline constexpr const char* sound       = "sound";         // the guide lead on / off
    inline constexpr const char* kit         = "lead";        // the guide lead's sound
    inline constexpr const char* key         = "key";           // the key of the progressions (0 = C ... 11 = B)
    inline constexpr const char* progression = "progression";   // the last progression picked
    inline constexpr const char* chordInput  = "chordInput";    // the chords held on the MIDI input take over
    inline constexpr const char* chordCount  = "chordCount";    // how many chord slots play (1..8, one bar each)
    inline constexpr const char* chordColour = "chordColour";   // triads, 7ths or 9ths (the progressions)
    inline constexpr const char* scale       = "scale";         // the scale the progressions are built on
    inline constexpr const char* attackDelay = "attackDelay";         // ms between two strings of a strum
    inline constexpr const char* tone        = "tone";
    inline constexpr const char* reverb      = "reverb";
    inline constexpr const char* volume      = "volume";
    inline constexpr const char* letRing     = "letRing";       // repeats ring on until the next chord hit
    inline constexpr const char* roomForBand = "roomForBand";   // the low strings left to the bass, the stabs above the piano
    inline constexpr const char* pushChanges = "pushChanges";   // the last 8th of a bar plays the next chord
    inline constexpr const char* keyswitches = "keyswitches";   // articulation keyswitches (and the swell's CC 11) in the MIDI

    inline juce::String keyswitch (int articulation) { return "keyswitch" + juce::String (articulation + 1); }   // its note

    inline juce::String segment (int index) { return "segment" + juce::String (index + 1); }
    inline juce::String mute (int lane)  { return "mute" + juce::String (lane + 1); }
    inline const juce::String globalOctave { "globalOctave" };
    inline juce::String octave (int lane) { return "octave" + juce::String (lane + 1); }        // a lane up or down by octaves
    inline juce::String chordRoot (int slot) { return "chordRoot" + juce::String (slot + 1); }
    inline juce::String chordType (int slot) { return "chordType" + juce::String (slot + 1); }
    inline juce::String level (int lane) { return "level" + juce::String (lane + 1); }
    inline juce::String laneVolume (int lane) { return "volume" + juce::String (lane + 1); }   // the guide sound's volume of a lane, in dB
    inline juce::String push (int lane) { return "push" + juce::String (lane + 1); }             // ms ahead (< 0) or behind (> 0) the beat
    inline juce::String laneSwing (int lane) { return "laneSwing" + juce::String (lane + 1); }   // the share of the swing a lane takes
}

class FaseGrooveLeadProcessor : public juce::AudioProcessor,
                                  private FaseGroove::LinkClient::Product
{
public:
    FaseGrooveLeadProcessor();
    ~FaseGrooveLeadProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /** Current parameter values as engine settings (safe from any thread). */
    fgl::LeadSettings currentSettings() const;

    /** Sets a parameter from the UI, notifying the host. */
    void setParameterFromUI (const juce::String& id, float plainValue);
    /** A parameter's current value, by its ID (a cached lookup: no String built, safe on any thread). */
    float raw (juce::StringRef id) const { return rawParams (id); }

    //==============================================================================
    // The grid (message thread). Steps are as displayed (two bars); edits go into the user
    // pattern, which then becomes the groove.
    static constexpr int gridSteps = fgl::userPatternLength;   // the most steps a groove can have (4 bars)
    /** The steps of the groove now: 32 (parts of half a bar) or 64 (parts of one bar). */
    int steps() const { return juce::roundToInt (raw (ParamIDs::partLength)) == 1 ? 64 : 32; }
    char stepSymbol (int lane, int step) const;
    void editStep (int lane, int step, char symbol);
    void clearLane (int lane);
    void clearGrid();
    /** The groove shown on the grid: as you hear it in the A-D order (before the song section). */
    fgl::LeadPattern displayedPattern() const;
    /** What plays in the `numSteps` 16ths from `firstStep` (variations, fills, the section, humanize...):
        the note-ons the same render as the playback gives, for the views. The step of a note is
        stepOf (its time). */
    std::vector<fgl::NoteEvent> playedNotes (long long firstStep, int numSteps) const;
    /** The same as a groove (one cell per lane and step; X loud, g soft), for the overview. */
    fgl::LeadPattern playedPattern (long long firstStep, int numSteps) const;
    static long long stepOf (double ppq) noexcept { return (long long) std::floor (ppq / fgl::stepLength + 0.3); }

    // A B C D: the four half bars of the groove, played in any order; each can get new random
    // content (its die) or be locked so the dice leave it alone.
    int segmentAt (int position) const;            // the part (0..3) played at a position
    void setSegmentAt (int position, int part);
    bool isSegmentLocked (int part) const          { return segmentLocked[(size_t) juce::jlimit (0, 3, part)]; }
    void setSegmentLocked (int part, bool locked)  { segmentLocked[(size_t) juce::jlimit (0, 3, part)] = locked; }
    void randomizeSegment (int part);
    /** A new line for one lane only (in the NEW GROOVE genre); locked parts stay. */
    void randomizeLane (int lane);

    /** Velocity set by hand on a displayed step (1..127, 100 = the Velocity control), 0 = automatic. */
    int stepVelocity (int lane, int step) const;
    void setStepVelocity (int lane, int step, int level);
    void resetStepVelocities (int lane = -1);   // one lane, or all (-1)
    /** The velocity a displayed step plays at (set by hand, or automatic from its symbol). */
    int effectiveVelocity (int lane, int step) const;

    /** The chance a displayed step plays (100 = always) and its roll (1 = one hit, 2-4 quick repeats). */
    int stepChance (int lane, int step) const;
    void setStepChance (int lane, int step, int percent);
    int stepRoll (int lane, int step) const;
    void setStepRoll (int lane, int step, int hits);

    // The fill drawn by hand (Fill style: Custom): one bar of 16ths.
    char customFillSymbol (int lane, int step) const;
    void editCustomFill (int lane, int step, char symbol);
    void clearCustomFill();
    /** Starts the custom fill from what a fill style plays (and makes Custom the fill style). */
    void customFillFromStyle (int style);

    // The feel of each lane (MIXER): its push / pull and its share of the swing.
    static juce::StringArray timingFeelNames();
    void applyTimingFeel (int feel);

    // Groove templates: the timing and accents of every 16th of the bar, taken from an audio
    // loop or a MIDI file and applied to any groove. Returns a short report, empty on failure.
    juce::String grooveTemplateFromAudio (const juce::File&);
    juce::String grooveTemplateFromMidi (const juce::File&);
    void clearGrooveTemplate();
    bool hasGrooveTemplate() const { return templateActive.load(); }
    juce::String grooveTemplateName() const { return templateName; }
    void setGrooveTemplateActive (bool on) { templateActive = on && templateName.isNotEmpty(); }

    // The chords: up to eight slots, one bar each (the key transposes them all).
    fgl::Chord chordAt (int slot) const;
    void setChord (int slot, fgl::Chord);
    /** The four chords of a progression, in the key. */
    void applyProgression (int index);
    /** Sets the chords (and how many play). */
    void setChords (const std::vector<fgl::Chord>&);
    int chordCount() const;
    void setChordCount (int);
    /** A new key: the chords move with it. */
    void setKey (int key);
    /** A new key and scale at once. Between relative keys (C major and A minor...) the chords
        stay where they are: the same notes, only the tonic changes. */
    void setKeyAndScale (int key, int scale);
    /** A random progression of the NEW GROOVE genre. */
    void randomizeChords();
    /** The chord a bar plays now: the held chord when the MIDI input has one, else its slot. */
    fgl::ChordTones chordForBar (long long bar) const;
    /** The chord held on the MIDI input (mask 0: none). */
    fgl::ChordTones heldChord() const { return { liveRoot.load(), (unsigned) liveMask.load() }; }
    bool followsHeldChord() const { return raw (ParamIDs::chordInput) > 0.5f && liveMask.load() != 0; }

    static bool isAudioFile (const juce::File&);

    // Audio export of the guide sound.
    enum class AudioExport { clip = 0, song, stems };
    /** Writes WAV file(s) into `target` (a file, or a folder for the stems); returns the files written. */
    juce::Array<juce::File> writeAudio (AudioExport, int bars, const juce::File& target) const;
    juce::String audioFileName (AudioExport, int bars) const;

    // LINK: genre, swing, tempo and play shared with Fase Groove Piano and Drums (and other Fase Groove instances).
    bool isLinked() const { return linkOn.load(); }
    void setLinked (bool on);
    static juce::File linkFile();
    /** Writes the genre and swing for the others, or takes theirs (message thread, a few times a second). */
    void updateLink();

    /** KEY (with LINK on): the same key and scale in every linked plug-in that has KEY on. */
    void setKeyLinked (bool on);
    bool isKeyLinked() const noexcept { return keyLinked.load(); }

    /** Velocity shapes for one lane (what makes a strum groove): the levels of its hits are set by hand. */
    static juce::StringArray velocityShapeNames();
    void applyVelocityShape (int lane, int shape);
    /** The velocity dice: a random shape with a little life on every hit. */
    void randomizeVelocities (int lane);

    bool isLaneMuted (int lane) const { return raw (ParamIDs::mute (lane)) > 0.5f; }
    void setLaneMuted (int lane, bool muted) { setParameterFromUI (ParamIDs::mute (lane), muted ? 1.0f : 0.0f); }
    int laneOctave (int lane) const { return juce::roundToInt (raw (ParamIDs::octave (lane))); }

    /** Picks a groove; a preset brings its genre's swing (and makes its genre the dice's genre). */
    void selectGroove (int pattern);

    /** NEW GROOVE: a brand new random groove in the chosen genre (with the genre's swing). */
    void rollNewBeat();
    /** NEW FEEL: another humanization (repeatable: it is saved with the project). */
    void newFeel();

    /** TOTAL RANDOM: a random style, then a new groove, chords, swing, feel, fills and sound; always musical. */
    void totalRandom();

    /** One-click setups by genre: a groove of that genre, its chords, swing, feel, fills and sound. */
    static juce::StringArray styleNames();
    void applyStyle (int index);
    /** Every setting back to its default. */
    void resetToDefaults();

    //==============================================================================
    enum class ClipPart { all = 0, tracks };   // tracks: one MIDI file, one track per lane
    static juce::String sectionClipName (int partIndex);
    /** Pass as `bars` to export the whole song: intro, main, break, build-up, drop, outro. */
    static constexpr int songArrangement = -1;

    /** Renders the groove as a standard MIDI file. bars == 0 means "one full loop". */
    juce::MidiFile createMidiClip (int bars, ClipPart part) const;
    /** Writes a .mid whole or not at all: a temporary file first, the one already there replaced
        only once the new one is complete (a full disk or no permission keeps the old clip). */
    static bool writeMidiFileSafely (const juce::MidiFile&, const juce::File& target);
    /** Writes the clip into Documents/Fase Groove Lead/MIDI clips and returns the file. */
    juce::File writeMidiClip (int bars, ClipPart part) const;
    juce::String clipFileName (int bars, ClipPart part) const;
    int clipLengthInBars (int bars) const;

    // Song arrangement: Intro, Main, Break, Build-up, Drop, Outro (the order of fgl::defaultArrangement).
    static constexpr int numSongParts = 6;
    std::vector<fgl::ArrangementPart> songArrangementParts() const;
    void setSongBars (int part, int bars);
    void setSongFill (int part, int fill);   // -1 as the Song panel, -2 none, else a fill style (last bar of the section)
    void regenerateSongPart (int part);      // this section plays its own version of the groove
    void resetSongPart (int part);           // back to the groove as it is
    void resetSong();
    /** One section of the song as a clip, named after it (empty file if the section is left out). */
    juce::File writeSongSectionClip (int partIndex, ClipPart part) const;
    /** Writes every section of the song as its own .mid file; returns how many were written. */
    int saveSongSections (const juce::File& folder, ClipPart) const;

    /** Documents/Fase Groove Lead: MIDI clips, presets and grooves, easy to find and back up. */
    static juce::File userFolder();
    static juce::File clipFolder();
    static juce::File presetFolder();
    static juce::File grooveFolder();
    static juce::Array<juce::File> presetFiles();
    static juce::Array<juce::File> grooveFiles();
    bool savePreset (const juce::String& name);
    bool loadPreset (const juce::File&);
    /** MY GROOVES: the grid (with its velocities) as a .fglgroove file. */
    bool saveGroove (const juce::String& name);
    bool loadGroove (const juce::File&);

    /** Reads a MIDI file (a lead part, chords, a piano riff) into the grid: its rhythm on two bars of
        16ths, each hit in the lane it sounds like (a strum, a stab, octaves, a single note...), and the
        chords of its first four bars into the chord slots. Returns false when it has no notes. */
    bool importMidiGroove (const juce::File&);

    // Undo / redo: snapshots of everything but the window size.
    juce::String snapshot() const;
    void restoreSnapshot (const juce::String&);

    // Editor preferences, saved with the project.
    float editorScale = 0.0f;   // 0: as big as suits the screen (the first time)

    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    /** Plays one hit of a lane now (over the chord of the bar), through the guide lead and the MIDI out. */
    void audition (int lane, int velocity = 110)   { auditionRequest = juce::jlimit (0, fgl::numLanes - 1, lane) * 128 + juce::jlimit (1, 127, velocity); }

    std::atomic<bool> previewOn { false };   // play the groove without the host transport
    std::atomic<bool> previewFromStart { true }, previewRewind { false };
    // (With LINK on, play / pause / stop / back to the start are shared with the linked plug-ins.)
    void previewPlay();
    void previewPause();
    void previewStop();
    void previewToStart();
    void previewToggle()  { previewOn.load() ? previewPause() : previewPlay(); }

    std::atomic<double> lastBpm { 120.0 };   // tempo in use (DAW or manual)
    std::atomic<double> hostBpm { 120.0 };   // what the DAW reports
    /** The tempo exported files use: the manual BPM, or the DAW's (as last reported). */
    float outputLevel() const;
    static void softLimit (juce::AudioBuffer<float>&, int numSamples);
    double exportBpm() const;

    // Read by the editor for display.
    std::atomic<int> uiStep { -1 };       // current 16th within the pattern, -1 = stopped
    std::atomic<double> uiPpq { -1.0 };   // playing position in beats, -1 = stopped
    std::atomic<int> uiSteps { 16 };     // the pattern's length in steps, for displayStep()
    FaseGroove::PlayClock playClock;     // where the sound is now (PlayClock.h)
    /** Message thread: the position and the step heard now, moved on from the last block (-1 = stopped). */
    double displayPpq() const noexcept { return playClock.position(); }
    int displayStep() const noexcept
    {
        const double ppq = playClock.position();
        if (ppq < 0.0)
            return -1;
        const auto step = (long long) std::floor (ppq / fgl::stepLength);
        const int n = std::max (1, uiSteps.load());
        return (int) (((step % n) + n) % n);
    }
    std::array<std::atomic<float>, fgl::numLanes> laneFlash {};   // last velocity per lane (for the grid's lights)

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    FaseGroove::RawParameters rawParams;   // every parameter by ID, filled once (RawParameters.h)
    // The parameters with a numbered ID (one per lane, chord slot...), looked up once: the audio
    // thread reads them every block without building their names.
    struct LaneParams { std::atomic<float>* octave = nullptr; std::atomic<float>* level = nullptr; std::atomic<float>* mute = nullptr;
                        std::atomic<float>* push = nullptr; std::atomic<float>* swing = nullptr; std::atomic<float>* volume = nullptr; };
    std::array<LaneParams, fgl::numLanes> laneParams {};
    std::array<std::atomic<float>*, fgl::maxChords> chordRootParams {}, chordTypeParams {};
    std::array<std::atomic<float>*, fgl::numArticulations> keyswitchParams {};
    std::array<std::atomic<float>*, fgl::numSegments> segmentParams {};
    static float load (const std::atomic<float>* p) noexcept { return p != nullptr ? p->load() : 0.0f; }

    void setUserPattern (const fgl::LeadPattern&);
    void resetStepExtras (int lane, int index) { userChance[(size_t) lane][(size_t) index] = 0; userRoll[(size_t) lane][(size_t) index] = 0; }
    /** A 2-bar groove is stored twice, so the 4-bar view repeats the bars just edited. */
    void mirrorUserHalf (int lane, int index);
    struct LoopAnalysis;
    std::unique_ptr<LoopAnalysis> analyseLoop (const juce::File&) const;
    void transposeChords (int semitones);
    void renderGuideAudio (const std::vector<fgl::NoteEvent>&, double beats, juce::AudioBuffer<float>&, double sampleRate, int onlyLane) const;
    static std::uint32_t freshSeed();

    // The user pattern, readable from the audio thread without locks.
    std::array<std::array<std::atomic<char>, fgl::userPatternLength>, fgl::numLanes> userGrid;
    std::array<std::array<std::atomic<int>, fgl::userPatternLength>, fgl::numLanes> userVelocity;   // 0 = automatic
    std::array<std::array<std::atomic<int>, fgl::userPatternLength>, fgl::numLanes> userChance, userRoll;   // 0 = always / one hit
    std::array<std::array<std::atomic<char>, 16>, fgl::numLanes> customFill;
    std::atomic<bool> templateActive { false };
    std::array<std::atomic<float>, 16> templateTiming, templateLevel;
    juce::String templateName;
    void setTemplate (const juce::String& name, const std::array<float, 16>& timing, const std::array<float, 16>& level);

    // The notes held on the MIDI input (audio thread), and the chord they make (for the UI too).
    std::array<bool, 128> heldNotes {};
    std::array<std::uint8_t, 128> heldCount {};   // note-ons not yet released, per pitch
    std::atomic<int> liveRoot { 0 }, liveMask { 0 };

    std::atomic<bool> linkOn { false };
    using LinkState = FaseGroove::LinkState;
    std::atomic<double> linkAnchor { -1.0 };   // the shared clock while playing, -1 = none
    FaseGroove::LinkClient link { *this, previewOn, previewFromStart, linkAnchor, true };   // Link.xml (LinkClient.h)
    LinkState localLinkState() const override;
    juce::String linkGenreName (int genre) const override;
    void followLinkedGroove (const juce::XmlElement& theirs) override;
    void setLinkedParameter (const char* id, float plainValue) override { setParameterFromUI (id, plainValue); }
    int currentGenre() const;
    juce::TimedCallback linkTimer { [this] { followKeyParameter(); updateLink(); } };
    /** The key the chord slots are in: a key moved by the host (automation, a generic editor)
        takes the chords with it too. */
    void followKeyParameter();
    std::atomic<int> appliedKey { -1 };
    std::atomic<bool> keyLinked { false };
    int baseStep (int displayedStep) const;   // the step of the user pattern shown at a displayed step
    /** Makes the groove showing the user pattern (as written: before the A-D order); true if it already was. */
    bool makeUserPattern();
    std::array<bool, fgl::numSegments> segmentLocked {};
    std::array<std::atomic<int>, numSongParts> songBars;                 // per song section (0 = left out)
    std::array<std::atomic<std::uint32_t>, numSongParts> songVariation;  // 0 = the groove as it is
    std::array<std::atomic<int>, numSongParts> songFill;                 // see setSongFill
    juce::ValueTree stateTree() const;
    void applyStateTree (const juce::ValueTree&);

    juce::MidiFile midiFromEvents (const std::vector<fgl::NoteEvent>&, double beats,
                                   const std::vector<fgl::GrooveEngine::Marker>*, ClipPart, const juce::String& name) const;

    fgl::GrooveEngine engine;
    // The groove worked out ahead on the message thread (the audio thread only reads it), and the
    // old ones kept until the audio thread has let go of them.
    std::shared_ptr<const fgl::GrooveEngine::Prepared> preparedPattern;
    FaseGroove::RetireBin<fgl::GrooveEngine::Prepared> retiredPatterns;   // RetireBin.h
    void preparePattern();
    juce::TimedCallback patternTimer { [this] { preparePattern(); } };
    std::vector<fgl::NoteEvent> events;
    juce::MidiBuffer outMidi;
    std::array<int, 128> noteChannels {};   // channel used by each generated note-on
    std::atomic<int> auditionRequest { -1 };   // lane * 128 + velocity
    std::array<int, 8> auditionNotes {};       // the notes of the lane played by hand, until auditionEnd
    int auditionCount = 0, auditionLane = 0, auditionChannel = 1, auditionVelocity = 100;
    juce::int64 auditionStart = 0, auditionGap = 0, auditionDuration = 0;

    GuideLead lead;
    std::vector<GuideLead::Event> guideEvents;
    juce::Reverb reverbFx;
    juce::SmoothedValue<float> outputGain;

    // MIDI clock (the standalone app has no DAW: a DAW can send it its clock).
    juce::int64 samplesProcessed = 0, lastClockSample = -1;
    double clockInterval = 0.0, clockPpq = 0.0;
    bool clockRunning = false;

    double internalPpq = 0.0;
    double expectedNextPpq = 0.0;
    bool wasRunning = false;
    bool soundWasOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaseGrooveLeadProcessor)
};
