// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "LinkFile.h"
#include "MidiExport.h"
#include "FamilySoundProfiles.h"
#include "PluginEditor.h"
#include "Localisation.h"

#include <complex>
#include <map>

namespace
{
    // Choice lists. Items are only ever appended, so saved projects keep their values.
    const int fillBars[] { 0, 1, 2, 4, 8 };
    const int variationBars[] { 0, 4, 8 };

    juce::String percent (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
    int swingPercent (float v) { return juce::roundToInt (50.0f + v * 50.0f / 3.0f); }

    juce::StringArray patternNames()
    {
        juce::StringArray names;
        for (const auto& p : fgl::leadPatterns())
            names.add (p.name);
        names.add ("Random groove");
        names.add ("User pattern");
        return names;
    }

    template <typename Fn>
    juce::StringArray namesFrom (int count, Fn&& name)
    {
        juce::StringArray names;
        for (int i = 0; i < count; ++i)
            names.add (name (i));
        return names;
    }

    struct Style
    {
        const char* name;
        const char* groove;
        float humanize;
        int fills;       // index in fillBars
        fgl::FillStyle fillStyle;
        int kit;         // the guide lead's sound
        int variation;   // index in variationBars
    };

    // One per genre, in grooveCategories() order: the genre's swing comes from fgl::genreSwing,
    // its chords from fgl::genreProgression.
    const Style styles[] {
        { "Piano House",       "House Hook",     0.15f, 4, fgl::FillStyle::motifBuild,   0, 1 },
        { "Soulful",           "Soulful Sunday",     0.30f, 4, fgl::FillStyle::approachUp,      0, 1 },
        { "Deep",              "Deep Chord Echo",    0.25f, 0, fgl::FillStyle::longOut,     1, 2 },
        { "Gospel",            "Gospel Hook",       0.30f, 3, fgl::FillStyle::motifBuild,   1, 0 },
        { "Jackin'",           "Jack Staccato",          0.20f, 4, fgl::FillStyle::ghostBuild, 0, 1 },
        { "Jackin' Piano",     "Bouncy Staccatos",       0.20f, 4, fgl::FillStyle::mix,          0, 1 },
        { "Garage & UK",       "Two-Step Chops",     0.25f, 4, fgl::FillStyle::dropOut,      2, 1 },
        { "Afro & Latin",      "Afro Highlife",      0.30f, 4, fgl::FillStyle::arpeggioRun,       3, 2 },
        { "Funky & Disco",     "Disco Ghost",       0.30f, 3, fgl::FillStyle::highRun,    2, 0 },
        { "'90s Piano House",  "90s Anthem Hook",   0.10f, 4, fgl::FillStyle::motifBuild,   0, 1 },
        { "Neo Soul & Broken", "Neo Soul Lazy",      0.40f, 0, fgl::FillStyle::melodyRun,      1, 2 },
        { "Chill & Pads",      "Chill Arpeggio",      0.30f, 0, fgl::FillStyle::longOut,     3, 2 },
        { "Dance Stabs",       "Big Staccatos",          0.05f, 4, fgl::FillStyle::noteStutter,  0, 1 },
        // Explicit ecosystem presets, appended so legacy indexes remain stable.
        { fgfamily::names[0], "", 0,0, fgl::FillStyle::mix,0,0 },
        { fgfamily::names[1], "", 0,0, fgl::FillStyle::mix,0,0 },
        { fgfamily::names[2], "", 0,0, fgl::FillStyle::mix,0,0 },
        { fgfamily::names[3], "", 0,0, fgl::FillStyle::mix,0,0 },
        { fgfamily::names[4], "", 0,0, fgl::FillStyle::mix,0,0 },
        { fgfamily::names[5], "", 0,0, fgl::FillStyle::mix,0,0 },
        { fgfamily::names[6], "", 0,0, fgl::FillStyle::mix,0,0 },
        { fgfamily::names[7], "", 0,0, fgl::FillStyle::mix,0,0 },
        { fgfamily::names[8], "", 0,0, fgl::FillStyle::mix,0,0 },
        { fgfamily::names[9], "", 0,0, fgl::FillStyle::mix,0,0 },
        { fgfamily::names[10], "", 0,0, fgl::FillStyle::mix,0,0 },
        { fgfamily::names[11], "", 0,0, fgl::FillStyle::mix,0,0 },
        { fgfamily::names[12], "", 0,0, fgl::FillStyle::mix,0,0 },
    };

    int indexOfGroove (const char* name)
    {
        const auto& presets = fgl::leadPatterns();
        for (size_t i = 0; i < presets.size(); ++i)
            if (presets[i].name == name)
                return (int) i;
        return 0;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout FaseGrooveLeadProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Parameters are grouped (Groove, Chords, Feel, Song, Tempo & MIDI, Sound, Lanes) so the DAW's
    // automation list is easy to browse. Groups don't change the parameter IDs.
    std::vector<std::unique_ptr<RangedAudioParameter>> current;
    auto add = [&current] (auto param) { current.push_back (std::move (param)); };
    auto group = [&layout, &current] (const String& id, const String& name)
    {
        auto g = std::make_unique<AudioProcessorParameterGroup> (id, name, " | ");
        for (auto& p : current)
            g->addChild (std::move (p));
        current.clear();
        layout.add (std::move (g));
    };
    auto choice = [&add] (const String& id, const String& name, const StringArray& items, int def)
    {
        add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto unitFloat = [&add] (const String& id, const String& name, float lo, float hi, float def)
    {
        add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi),
                                                    def, AudioParameterFloatAttributes().withStringFromValueFunction (percent)));
    };
    auto toggle = [&add] (const String& id, const String& name, bool def)
    {
        add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    // Groove
    choice (ParamIDs::pattern, "Groove", patternNames(), 0);
    choice (ParamIDs::genre, "New groove genre", namesFrom ((int) fgl::grooveCategories().size(),
                                                          [] (int i) { return String (fgl::grooveCategories()[(size_t) i]); }), 0);
    add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::riffSeed, 1 }, "Groove seed", 1, 100000, 1));
    unitFloat (ParamIDs::density, "Density", 0.0f, 1.0f, 0.5f);
    choice (ParamIDs::partLength, "Part length", { "1/2 bar", "1 bar" }, 0);
    for (int i = 0; i < fgl::numSegments; ++i)
        choice (ParamIDs::segment (i), "Part " + String (i + 1), { "A", "B", "C", "D" }, i);
    group ("groove", "Groove");

    // Chords: the key, the last progression picked, up to eight chord slots (one bar each), how many
    // play, how rich they are and the scale they are built on
    StringArray keys, chordTypes, progressionNames;
    for (int k = 0; k < 12; ++k)
        keys.add (fgl::pitchClassName (k));
    for (int t = 0; t < fgl::numChordTypes; ++t)
        chordTypes.add (t == (int) fgl::ChordType::major ? String ("maj") : String (fgl::chordTypeName (t)));
    for (const auto& prog : fgl::progressions())
        progressionNames.add (prog.name);
    const auto defaultChords = fgl::progressionChords (fgl::genreProgression (0), 9);
    choice (ParamIDs::key, "Key", keys, 9);
    choice (ParamIDs::progression, "Progression", progressionNames, fgl::genreProgression (0));
    for (int i = 0; i < fgl::maxChords; ++i)
    {
        const auto& c = defaultChords[(size_t) i % defaultChords.size()];
        choice (ParamIDs::chordRoot (i), "Chord " + String (i + 1) + " root", keys, c.root);
        choice (ParamIDs::chordType (i), "Chord " + String (i + 1) + " type", chordTypes, c.type);
    }
    choice (ParamIDs::chordCount, "Chords", { "1", "2", "3", "4", "5", "6", "7", "8" }, (int) defaultChords.size() - 1);
    choice (ParamIDs::chordColour, "Chord colour", namesFrom (fgl::numChordColours, fgl::chordColourName), (int) fgl::ChordColour::ninths);
    choice (ParamIDs::scale, "Scale", namesFrom (fgl::numScales, fgl::scaleName), 0);
    toggle (ParamIDs::chordInput, "Chords from MIDI in", true);
    group ("chords", "Chords");

    // Feel
    // The swing, shown as drum machines show it: 50% straight, 67% a triplet shuffle, 75% the heaviest.
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::swing, 1 }, "Swing", NormalisableRange<float> (0.0f, 1.5f), fgl::genreSwing (0),
                                                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                    { return String (swingPercent (v)) + "%"; })));
    add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::velocity, 1 }, "Velocity", 1, 127, 100));
    unitFloat (ParamIDs::humanize, "Humanize", 0.0f, 1.0f, 0.3f);
    add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::humanizeSeed, 1 }, "Feel seed", 0, 100000, 0));
    unitFloat (ParamIDs::accents, "Accents", 0.0f, 1.0f, 0.85f);
    group ("feel", "Feel");

    // Song
    choice (ParamIDs::section, "Section", namesFrom (fgl::numSections, fgl::sectionName), 0);
    choice (ParamIDs::fills, "Fills", { "Off", "Every bar", "Every 2 bars", "Every 4 bars", "Every 8 bars" }, 4);
    choice (ParamIDs::fillStyle, "Fill style", namesFrom (fgl::numFillStyles, fgl::fillStyleName), 0);
    toggle (ParamIDs::resolveNote, "Resolution after fills", true);
    choice (ParamIDs::variation, "Variation", { "Off", "Every 4 bars", "Every 8 bars" }, 1);
    group ("song", "Song");

    // Tempo & MIDI
    toggle (ParamIDs::sequencer, "Sequencer", true);
    toggle (ParamIDs::tempoSync, "Sync to DAW", true);
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::tempo, 1 }, "Tempo", NormalisableRange<float> (60.0f, 180.0f, 0.1f), 124.0f,
                                                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                    { return String (v, 1) + " BPM"; })));
    add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::midiChannel, 1 }, "MIDI channel", 1, 16, 1));
    group ("tempoMidi", "Tempo & MIDI");

    // Sound (the guide lead) and the strum
    toggle (ParamIDs::sound, "Guide sound", true);
    choice (ParamIDs::kit, "Lead", namesFrom (GuideLead::numTones, GuideLead::toneName), 0);
    unitFloat (ParamIDs::tone, "Tone", 0.0f, 1.0f, 0.7f);
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::attackDelay, 1 }, "Attack delay", NormalisableRange<float> (0.0f, 40.0f, 0.5f), 12.0f,
                                                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                    { return String (v, 1) + " ms"; })));
    unitFloat (ParamIDs::reverb, "Reverb", 0.0f, 1.0f, 0.12f);
    add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::volume, 1 }, "Volume", NormalisableRange<float> (-36.0f, 6.0f, 0.1f), -3.0f,
                                                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                    { return String (v, 1) + " dB"; })));
    group ("sound", "Sound");

    add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::globalOctave, 1 }, "Global octave", -2, 2, 0));

    // Lanes: per lane, mute, octave and level
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        const String name (fgl::laneName (l));
        toggle (ParamIDs::mute (l), name + " mute", false);
        add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::octave (l), 1 }, name + " octave", -2, 2, 0));
        unitFloat (ParamIDs::level (l), name + " level", 0.0f, 1.5f, 1.0f);
        add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::laneVolume (l), 1 }, name + " volume",
                                                    NormalisableRange<float> (-48.0f, 12.0f, 0.1f, 2.0f), 0.0f,
                                                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                        { return v <= -47.9f ? String ("-inf") : String (v, 1) + " dB"; })));
        add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::push (l), 1 }, name + " push",
                                                    NormalisableRange<float> (-40.0f, 40.0f, 0.5f), 0.0f,
                                                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                        { return std::abs (v) < 0.25f ? String ("0 ms") : (v < 0.0f ? "-" : "+") + String (std::abs (v), 1) + " ms"; })));
        unitFloat (ParamIDs::laneSwing (l), name + " swing", 0.0f, 2.0f, 1.0f);
    }
    group ("lanes", "Lanes");

    // Playing: how the lead is played, and how a sampled lead is told (appended: older
    // projects load with these defaults)
    toggle (ParamIDs::letRing, "Let ring", false);
    toggle (ParamIDs::roomForBand, "Room for bass & piano", false);
    toggle (ParamIDs::pushChanges, "Push chord changes", true);
    toggle (ParamIDs::keyswitches, "Keyswitches", false);
    for (int a = 0; a < fgl::numArticulations; ++a)
        add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::keyswitch (a), 1 }, String ("Keyswitch ") + fgl::articulationName (a),
                                                  0, 39, 24 + a, AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                                      { return MidiMessage::getMidiNoteName (v, true, true, 3); })));
    group ("playing", "Playing");

    return layout;
}

FaseGrooveLeadProcessor::FaseGrooveLeadProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FaseGrooveLead", createLayout())
{
    rawParams.cache (apvts);
    for (auto& lane : userGrid)
        for (auto& c : lane)
            c = '.';
    for (auto* grid : { &userVelocity, &userChance, &userRoll })
        for (auto& lane : *grid)
            for (auto& v : lane)
                v = 0;
    for (auto& lane : customFill)
        for (auto& c : lane)
            c = '.';
    for (int i = 0; i < 16; ++i)
    {
        templateTiming[(size_t) i] = 0.0f;
        templateLevel[(size_t) i] = 1.0f;
    }
    for (auto& f : laneFlash)
        f = 0.0f;
    for (int l = 0; l < fgl::numLanes; ++l)
        laneParams[(size_t) l] = { apvts.getRawParameterValue (ParamIDs::octave (l)), apvts.getRawParameterValue (ParamIDs::level (l)),
                                   apvts.getRawParameterValue (ParamIDs::mute (l)), apvts.getRawParameterValue (ParamIDs::push (l)),
                                   apvts.getRawParameterValue (ParamIDs::laneSwing (l)), apvts.getRawParameterValue (ParamIDs::laneVolume (l)) };
    for (int i = 0; i < fgl::maxChords; ++i)
    {
        chordRootParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::chordRoot (i));
        chordTypeParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::chordType (i));
    }
    for (int a = 0; a < fgl::numArticulations; ++a)
        keyswitchParams[(size_t) a] = apvts.getRawParameterValue (ParamIDs::keyswitch (a));
    for (int i = 0; i < fgl::numSegments; ++i)
        segmentParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::segment (i));
    resetSong();
    appliedKey = juce::roundToInt (raw (ParamIDs::key));
    linkTimer.startTimer (150);
    preparePattern();
    patternTimer.startTimer (40);
}

void FaseGrooveLeadProcessor::preparePattern()
{
    // The groove (and its turnaround) for the settings as they are now, built here so the audio
    // thread never has to; the one it replaces is freed once the audio thread lets go of it.
    const auto settings = currentSettings();
    const auto current = std::atomic_load (&preparedPattern);
    if (current != nullptr && fgl::GrooveEngine::samePattern (current->key, settings))
        return;
    auto next = std::make_shared<fgl::GrooveEngine::Prepared>();
    fgl::GrooveEngine::prepare (*next, settings);
    std::atomic_store (&preparedPattern, std::shared_ptr<const fgl::GrooveEngine::Prepared> (std::move (next)));
    retiredPatterns.retire (current);
}

FaseGrooveLeadProcessor::~FaseGrooveLeadProcessor()
{
    patternTimer.stopTimer();
    linkTimer.stopTimer();
}

bool FaseGrooveLeadProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void FaseGrooveLeadProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lead.prepare (sampleRate);
    // Room for the largest blocks (offline rendering too): no growing on the audio thread.
    guideEvents.reserve ((size_t) juce::jmax (8192, samplesPerBlock / 2));
    reverbFx.setSampleRate (sampleRate);
    reverbFx.reset();
    outputGain.reset (sampleRate, 0.05);
    outputGain.setCurrentAndTargetValue (outputLevel());
    events.reserve ((size_t) juce::jmax (8192, samplesPerBlock / 2));
}

juce::AudioProcessorEditor* FaseGrooveLeadProcessor::createEditor()
{
    return new FaseGrooveLeadEditor (*this);
}

void FaseGrooveLeadProcessor::setParameterFromUI (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

//==============================================================================
fgl::LeadSettings FaseGrooveLeadProcessor::currentSettings() const
{
    fgl::LeadSettings s;
    s.globalOctave = juce::roundToInt (raw (ParamIDs::globalOctave));
    s.key = juce::roundToInt (raw (ParamIDs::key));
    s.scale = juce::roundToInt (raw (ParamIDs::scale));
    s.pattern = juce::roundToInt (raw (ParamIDs::pattern));
    s.grooveBars = juce::roundToInt (raw (ParamIDs::partLength)) == 1 ? 4 : 2;
    s.swing = raw (ParamIDs::swing);
    s.velocity = juce::roundToInt (raw (ParamIDs::velocity));
    s.humanize = raw (ParamIDs::humanize);
    s.humanizeSeed = (std::uint32_t) juce::roundToInt (raw (ParamIDs::humanizeSeed));
    s.accentGroove = raw (ParamIDs::accents);
    s.fillEveryBars = fillBars[juce::jlimit (0, 4, juce::roundToInt (raw (ParamIDs::fills)))];
    s.fillStyle = (fgl::FillStyle) juce::jlimit (0, fgl::numFillStyles - 1, juce::roundToInt (raw (ParamIDs::fillStyle)));
    s.resolveAfterFill = raw (ParamIDs::resolveNote) > 0.5f;
    s.variationEvery = variationBars[juce::jlimit (0, 2, juce::roundToInt (raw (ParamIDs::variation)))];
    s.section = (fgl::Section) juce::jlimit (0, fgl::numSections - 1, juce::roundToInt (raw (ParamIDs::section)));
    s.riffSeed = (std::uint32_t) juce::roundToInt (raw (ParamIDs::riffSeed));
    s.density = raw (ParamIDs::density);
    s.randomGenre = juce::roundToInt (raw (ParamIDs::genre));

    for (int l = 0; l < fgl::numLanes; ++l)
    {
        const auto& lp = laneParams[(size_t) l];
        s.octaves[(size_t) l] = juce::roundToInt (load (lp.octave));
        s.levels[(size_t) l] = load (lp.level);
        s.muted[(size_t) l] = load (lp.mute) > 0.5f;
        s.laneShift[(size_t) l] = load (lp.push);
        s.laneSwing[(size_t) l] = load (lp.swing);
        for (int i = 0; i < fgl::userPatternLength; ++i)
        {
            s.user[(size_t) l][(size_t) i] = userGrid[(size_t) l][(size_t) i].load();
            s.userVelocity[(size_t) l][(size_t) i] = (unsigned char) juce::jlimit (0, 127, userVelocity[(size_t) l][(size_t) i].load());
            s.userChance[(size_t) l][(size_t) i] = (unsigned char) juce::jlimit (0, 100, userChance[(size_t) l][(size_t) i].load());
            s.userRoll[(size_t) l][(size_t) i] = (unsigned char) juce::jlimit (0, 4, userRoll[(size_t) l][(size_t) i].load());
        }
        for (int i = 0; i < 16; ++i)
            s.customFill[(size_t) l][(size_t) i] = customFill[(size_t) l][(size_t) i].load();
    }
    s.tempo = lastBpm.load();
    for (int i = 0; i < fgl::maxChords; ++i)
        s.chords[(size_t) i] = chordAt (i);
    s.numChords = chordCount();
    s.useLiveChord = followsHeldChord();
    s.liveChord = heldChord();
    s.attackMs = raw (ParamIDs::attackDelay);
    s.letRing = raw (ParamIDs::letRing) > 0.5f;
    s.roomForBand = raw (ParamIDs::roomForBand) > 0.5f;
    s.pushChanges = raw (ParamIDs::pushChanges) > 0.5f;
    s.keyswitches = raw (ParamIDs::keyswitches) > 0.5f;
    for (int a = 0; a < fgl::numArticulations; ++a)
        s.keyswitch[(size_t) a] = juce::jlimit (0, 39, juce::roundToInt (load (keyswitchParams[(size_t) a])));
    s.grooveTemplate.active = templateActive.load();
    for (int i = 0; i < 16; ++i)
    {
        s.grooveTemplate.timing[(size_t) i] = templateTiming[(size_t) i].load();
        s.grooveTemplate.level[(size_t) i] = templateLevel[(size_t) i].load();
    }
    for (int i = 0; i < fgl::numSegments; ++i)
        s.segmentOrder[(size_t) i] = segmentAt (i);
    return s;
}

//==============================================================================
fgl::LeadPattern FaseGrooveLeadProcessor::displayedPattern() const
{
    const auto settings = currentSettings();
    return fgl::applySegmentOrder (fgl::GrooveEngine::basePattern (settings), settings.segmentOrder);
}

std::vector<fgl::NoteEvent> FaseGrooveLeadProcessor::playedNotes (long long firstStep, int numSteps) const
{
    auto settings = currentSettings();
    settings.tempo = lastBpm.load();   // as processBlock plays it
    fgl::GrooveEngine renderer;
    const auto ready = std::atomic_load (&preparedPattern);
    renderer.setPrepared (ready.get());
    std::vector<fgl::NoteEvent> events, played;
    const double start = (double) firstStep * fgl::stepLength;
    // From a beat earlier: a hit pushed ahead of the window's first step still belongs to it.
    renderer.render (start - 1.0, start + numSteps * fgl::stepLength, settings, events);
    for (const auto& e : events)
    {
        if (! e.isNoteOn() || e.lane == fgl::keyswitchLane)
            continue;
        const auto step = stepOf (e.ppq);
        if (step >= firstStep && step < firstStep + numSteps)
            played.push_back (e);
    }
    return played;
}

fgl::LeadPattern FaseGrooveLeadProcessor::playedPattern (long long firstStep, int numSteps) const
{
    fgl::LeadPattern shown;
    for (auto& lane : shown.lanes)
        lane.assign ((size_t) juce::jmax (0, numSteps), '.');
    for (const auto& e : playedNotes (firstStep, numSteps))
    {
        auto& cell = shown.lanes[(size_t) juce::jlimit (0, fgl::numLanes - 1, e.lane)][(size_t) (stepOf (e.ppq) - firstStep)];
        const char symbol = e.velocity >= 105 ? 'X' : e.velocity < 55 ? 'g' : 'x';
        if (cell == '.' || symbol == 'X' || (symbol == 'x' && cell == 'g'))
            cell = symbol;   // the loudest hit of the step
    }
    return shown;
}

int FaseGrooveLeadProcessor::segmentAt (int position) const
{
    return juce::jlimit (0, fgl::numSegments - 1, juce::roundToInt (load (segmentParams[(size_t) juce::jlimit (0, fgl::numSegments - 1, position)])));
}

void FaseGrooveLeadProcessor::setSegmentAt (int position, int part)
{
    setParameterFromUI (ParamIDs::segment (juce::jlimit (0, fgl::numSegments - 1, position)), (float) juce::jlimit (0, fgl::numSegments - 1, part));
}

int FaseGrooveLeadProcessor::baseStep (int displayedStep) const
{
    const int size = steps() / fgl::numSegments;
    const int step = juce::jlimit (0, steps() - 1, displayedStep);
    return segmentAt (step / size) * size + step % size;
}

char FaseGrooveLeadProcessor::stepSymbol (int lane, int step) const
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes) || ! juce::isPositiveAndBelow (step, steps()))
        return '.';
    if (juce::roundToInt (raw (ParamIDs::pattern)) == fgl::userPatternIndex())
        return userGrid[(size_t) lane][(size_t) baseStep (step)].load();
    return displayedPattern().lanes[(size_t) lane][(size_t) step];
}

void FaseGrooveLeadProcessor::setUserPattern (const fgl::LeadPattern& source)
{
    auto p = source;
    fgl::fitPattern (p, gridSteps);   // a two-bar groove fills the four bars twice
    for (int l = 0; l < fgl::numLanes; ++l)
        for (int i = 0; i < gridSteps; ++i)
        {
            userGrid[(size_t) l][(size_t) i] = p.lanes[(size_t) l][(size_t) i];
            userVelocity[(size_t) l][(size_t) i] = 0;
            resetStepExtras (l, i);
        }
    setParameterFromUI (ParamIDs::pattern, (float) fgl::userPatternIndex());
}

bool FaseGrooveLeadProcessor::makeUserPattern()
{
    if (juce::roundToInt (raw (ParamIDs::pattern)) == fgl::userPatternIndex())
        return true;
    // The groove as written (the A-D order stays as it is, on top of it).
    setUserPattern (fgl::GrooveEngine::basePattern (currentSettings()));
    return false;
}

void FaseGrooveLeadProcessor::editStep (int lane, int step, char symbol)
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes) || ! juce::isPositiveAndBelow (step, steps()))
        return;

    // The first edit of a preset (or random groove) copies it into the user pattern.
    makeUserPattern();
    const int at = baseStep (step);
    userGrid[(size_t) lane][(size_t) at] = fgl::isHit (symbol) ? symbol : '.';
    if (! fgl::isHit (symbol))
    {
        userVelocity[(size_t) lane][(size_t) at] = 0;
        resetStepExtras (lane, at);
    }
    mirrorUserHalf (lane, at);
}

void FaseGrooveLeadProcessor::mirrorUserHalf (int lane, int index)
{
    if (steps() != 32 || ! juce::isPositiveAndBelow (lane, fgl::numLanes) || index < 0 || index >= 32)
        return;
    const int to = index + 32;
    userGrid[(size_t) lane][(size_t) to] = userGrid[(size_t) lane][(size_t) index].load();
    userVelocity[(size_t) lane][(size_t) to] = userVelocity[(size_t) lane][(size_t) index].load();
    userChance[(size_t) lane][(size_t) to] = userChance[(size_t) lane][(size_t) index].load();
    userRoll[(size_t) lane][(size_t) to] = userRoll[(size_t) lane][(size_t) index].load();
}

void FaseGrooveLeadProcessor::clearLane (int lane)
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes))
        return;
    makeUserPattern();
    for (int i = 0; i < gridSteps; ++i)
    {
        userGrid[(size_t) lane][(size_t) i] = '.';
        userVelocity[(size_t) lane][(size_t) i] = 0;
        resetStepExtras (lane, i);
    }
}

void FaseGrooveLeadProcessor::clearGrid()
{
    for (int l = 0; l < fgl::numLanes; ++l)
        for (int i = 0; i < gridSteps; ++i)
        {
            userGrid[(size_t) l][(size_t) i] = '.';
            userVelocity[(size_t) l][(size_t) i] = 0;
            resetStepExtras (l, i);
        }
    setParameterFromUI (ParamIDs::pattern, (float) fgl::userPatternIndex());
}

void FaseGrooveLeadProcessor::randomizeSegment (int part)
{
    if (! juce::isPositiveAndBelow (part, fgl::numSegments) || isSegmentLocked (part))
        return;
    makeUserPattern();
    const auto settings = currentSettings();
    auto fresh = fgl::makeRandomPattern (freshSeed(), settings.density, settings.randomGenre);
    fgl::fitPattern (fresh, steps());
    const int size = steps() / fgl::numSegments;
    for (int l = 0; l < fgl::numLanes; ++l)
        for (int i = part * size; i < (part + 1) * size; ++i)
        {
            userGrid[(size_t) l][(size_t) i] = fresh.lanes[(size_t) l][(size_t) i];
            userVelocity[(size_t) l][(size_t) i] = 0;
            resetStepExtras (l, i);
            mirrorUserHalf (l, i);
        }
}

void FaseGrooveLeadProcessor::randomizeLane (int lane)
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes))
        return;
    makeUserPattern();
    const auto settings = currentSettings();
    const auto line = fgl::makeRandomLane ((fgl::Lane) lane, freshSeed(), settings.density, settings.randomGenre);
    const int size = steps() / fgl::numSegments;
    for (int i = 0; i < steps(); ++i)
        if (! isSegmentLocked (i / size))
        {
            userGrid[(size_t) lane][(size_t) i] = line[(size_t) (i % (int) line.size())];
            userVelocity[(size_t) lane][(size_t) i] = 0;
            resetStepExtras (lane, i);
            mirrorUserHalf (lane, i);
        }
}

int FaseGrooveLeadProcessor::stepVelocity (int lane, int step) const
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes) || ! juce::isPositiveAndBelow (step, steps())
        || juce::roundToInt (raw (ParamIDs::pattern)) != fgl::userPatternIndex())
        return 0;
    return userVelocity[(size_t) lane][(size_t) baseStep (step)].load();
}

void FaseGrooveLeadProcessor::setStepVelocity (int lane, int step, int level)
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes) || ! juce::isPositiveAndBelow (step, steps()))
        return;
    makeUserPattern();
    const int at = baseStep (step);
    userVelocity[(size_t) lane][(size_t) at] = juce::jlimit (0, 127, level);
    mirrorUserHalf (lane, at);
}

void FaseGrooveLeadProcessor::resetStepVelocities (int lane)
{
    for (int l = 0; l < fgl::numLanes; ++l)
        if (lane < 0 || l == lane)
            for (auto& v : userVelocity[(size_t) l])
                v = 0;
}

int FaseGrooveLeadProcessor::effectiveVelocity (int lane, int step) const
{
    if (const int set = stepVelocity (lane, step); set > 0)
        return set;
    const char symbol = stepSymbol (lane, step);
    const double level = symbol == 'X' ? 1.0 : symbol == 'f' ? 0.9 : symbol == 'g' ? 0.42 : 0.8;
    return juce::jlimit (1, 127, juce::roundToInt (100.0 * level));
}

int FaseGrooveLeadProcessor::stepChance (int lane, int step) const
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes) || ! juce::isPositiveAndBelow (step, steps())
        || juce::roundToInt (raw (ParamIDs::pattern)) != fgl::userPatternIndex())
        return 100;
    const int c = userChance[(size_t) lane][(size_t) baseStep (step)].load();
    return c <= 0 || c >= 100 ? 100 : c;
}

void FaseGrooveLeadProcessor::setStepChance (int lane, int step, int percent)
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes) || ! juce::isPositiveAndBelow (step, steps()))
        return;
    makeUserPattern();
    const int at = baseStep (step);
    userChance[(size_t) lane][(size_t) at] = percent >= 100 ? 0 : juce::jlimit (1, 99, percent);
    mirrorUserHalf (lane, at);
}

int FaseGrooveLeadProcessor::stepRoll (int lane, int step) const
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes) || ! juce::isPositiveAndBelow (step, steps())
        || juce::roundToInt (raw (ParamIDs::pattern)) != fgl::userPatternIndex())
        return 1;
    return juce::jmax (1, userRoll[(size_t) lane][(size_t) baseStep (step)].load());
}

void FaseGrooveLeadProcessor::setStepRoll (int lane, int step, int hits)
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes) || ! juce::isPositiveAndBelow (step, steps()))
        return;
    makeUserPattern();
    const int at = baseStep (step);
    userRoll[(size_t) lane][(size_t) at] = hits <= 1 ? 0 : juce::jmin (4, hits);
    mirrorUserHalf (lane, at);
}

char FaseGrooveLeadProcessor::customFillSymbol (int lane, int step) const
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes) || ! juce::isPositiveAndBelow (step, 16))
        return '.';
    return customFill[(size_t) lane][(size_t) step].load();
}

void FaseGrooveLeadProcessor::editCustomFill (int lane, int step, char symbol)
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes) || ! juce::isPositiveAndBelow (step, 16))
        return;
    customFill[(size_t) lane][(size_t) step] = fgl::isHit (symbol) ? symbol : '.';
    if (juce::roundToInt (raw (ParamIDs::fillStyle)) != (int) fgl::FillStyle::custom)
        setParameterFromUI (ParamIDs::fillStyle, (float) (int) fgl::FillStyle::custom);
}

void FaseGrooveLeadProcessor::clearCustomFill()
{
    for (auto& lane : customFill)
        for (auto& c : lane)
            c = '.';
}

void FaseGrooveLeadProcessor::customFillFromStyle (int style)
{
    if (! juce::isPositiveAndBelow (style, fgl::numFillStyles) || style == (int) fgl::FillStyle::custom)
        return;

    // What the style plays on an empty groove: the fill alone.
    fgl::LeadSettings s;
    s.pattern = fgl::userPatternIndex();
    s.swing = 0.0f;
    s.humanize = 0.0f;
    s.accentGroove = 0.0f;
    s.resolveAfterFill = false;
    s.fillEveryBars = 1;
    s.fillStyle = (fgl::FillStyle) style;
    if (style == (int) fgl::FillStyle::mix)
        s.fillStyle = fgl::GrooveEngine::effectiveFillStyle (fgl::FillStyle::mix, 0);

    clearCustomFill();
    for (const auto& e : fgl::GrooveEngine::renderClip (s, 4.0))
    {
        if (! e.isNoteOn())
            continue;
        const int step = juce::roundToInt (e.ppq / fgl::stepLength);
        if (! juce::isPositiveAndBelow (step, 16) || ! juce::isPositiveAndBelow (e.lane, fgl::numLanes))
            continue;
        auto& c = customFill[(size_t) e.lane][(size_t) step];
        if (c.load() == '.')
            c = e.velocity >= 100 ? 'X' : e.velocity <= 50 ? 'g' : 'x';
    }
    setParameterFromUI (ParamIDs::fillStyle, (float) (int) fgl::FillStyle::custom);
}

juce::StringArray FaseGrooveLeadProcessor::timingFeelNames()
{
    return { "On the grid", "Laid-back repeats", "Pushed scratches", "Jackin' (stabs ahead, scratches swing)",
             "Lazy neo soul (everything behind)", "Straight picking" };
}

void FaseGrooveLeadProcessor::applyTimingFeel (int feel)
{
    // push (ms) and share of the swing per lane; the low strings stay with the kick.
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        const auto lane = (fgl::Lane) l;
        const bool light = lane == fgl::Lane::scratch || lane == fgl::Lane::answer || lane == fgl::Lane::chank;
        const bool chords = lane == fgl::Lane::hook || lane == fgl::Lane::stab || lane == fgl::Lane::slide || lane == fgl::Lane::swell;
        const bool picking = lane == fgl::Lane::arp || lane == fgl::Lane::lick;
        const bool low = lane == fgl::Lane::root || lane == fgl::Lane::power || lane == fgl::Lane::octave;
        float push = 0.0f, share = 1.0f;
        switch (feel)
        {
            case 1: push = chords || light ? 10.0f : 0.0f; break;
            case 2: push = light ? -6.0f : 0.0f; break;
            case 3: push = chords ? -6.0f : light ? 3.0f : 0.0f; share = light ? 1.2f : chords ? 0.6f : 1.0f; break;
            case 4: push = low ? 4.0f : 14.0f; share = 1.1f; break;
            case 5: share = picking ? 0.0f : 1.0f; break;
            case 0:
            default: break;
        }
        setParameterFromUI (ParamIDs::push (l), push);
        setParameterFromUI (ParamIDs::laneSwing (l), share);
    }
}

juce::StringArray FaseGrooveLeadProcessor::velocityShapeNames()
{
    return { "Offbeat push", "On the beat", "Shuffle 16ths", "Crescendo", "Humanize", "Flat" };
}

void FaseGrooveLeadProcessor::applyVelocityShape (int lane, int shape)
{
    if (! juce::isPositiveAndBelow (lane, fgl::numLanes))
        return;
    makeUserPattern();
    auto& r = juce::Random::getSystemRandom();

    for (int step = 0; step < steps(); ++step)
    {
        const char symbol = stepSymbol (lane, step);
        if (! fgl::isHit (symbol))
            continue;
        const int inBeat = step % 4;
        int v = 90;
        switch (shape)
        {
            case 0: v = inBeat == 2 ? 112 : inBeat == 0 ? 80 : inBeat == 3 ? 66 : 56; break;   // the offbeat sings
            case 1: v = inBeat == 0 ? 115 : inBeat == 2 ? 84 : 58; break;               // on the beat
            case 2: v = step % 2 == 0 ? 100 : 66; break;                                 // loud-soft 16ths
            case 3: v = 60 + (step % 16) * 55 / 15; break;                               // a crescendo over each bar
            case 4: v = juce::jlimit (1, 127, effectiveVelocity (lane, step) + r.nextInt ({ -14, 15 })); break;
            case 5:
            default: v = 96; break;
        }
        if (symbol == 'g')
            v = juce::jmin (v, 55);
        setStepVelocity (lane, step, juce::jlimit (1, 127, v));
    }
}

void FaseGrooveLeadProcessor::randomizeVelocities (int lane)
{
    auto& r = juce::Random::getSystemRandom();
    applyVelocityShape (lane, r.nextInt (4));
    for (int step = 0; step < steps(); ++step)
        if (const int v = stepVelocity (lane, step); v > 0)
            setStepVelocity (lane, step, juce::jlimit (1, 127, v + r.nextInt ({ -9, 10 })));
}

bool FaseGrooveLeadProcessor::isAudioFile (const juce::File& file)
{
    return file.hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
}

//==============================================================================
fgl::Chord FaseGrooveLeadProcessor::chordAt (int slot) const
{
    slot = juce::jlimit (0, fgl::maxChords - 1, slot);
    return { juce::jlimit (0, 11, juce::roundToInt (load (chordRootParams[(size_t) slot]))),
             juce::jlimit (0, fgl::numChordTypes - 1, juce::roundToInt (load (chordTypeParams[(size_t) slot]))) };
}

void FaseGrooveLeadProcessor::setChord (int slot, fgl::Chord chord)
{
    slot = juce::jlimit (0, fgl::maxChords - 1, slot);
    setParameterFromUI (ParamIDs::chordRoot (slot), (float) ((chord.root % 12 + 12) % 12));
    setParameterFromUI (ParamIDs::chordType (slot), (float) juce::jlimit (0, fgl::numChordTypes - 1, chord.type));
}

void FaseGrooveLeadProcessor::applyProgression (int index)
{
    index = juce::jlimit (0, (int) fgl::progressions().size() - 1, index);
    setParameterFromUI (ParamIDs::progression, (float) index);
    setParameterFromUI (ParamIDs::scale, (float) fgl::progressions()[(size_t) index].scale);
    setChords (fgl::progressionChords (index, juce::roundToInt (raw (ParamIDs::key)), juce::roundToInt (raw (ParamIDs::chordColour))));
    appliedKey = juce::roundToInt (raw (ParamIDs::key));   // the chords are in this key now
}

void FaseGrooveLeadProcessor::setChords (const std::vector<fgl::Chord>& chords)
{
    if (chords.empty())
        return;
    setChordCount ((int) chords.size());
    for (int i = 0; i < fgl::maxChords; ++i)
        setChord (i, chords[(size_t) i % chords.size()]);
}

int FaseGrooveLeadProcessor::chordCount() const
{
    return juce::jlimit (1, fgl::maxChords, juce::roundToInt (raw (ParamIDs::chordCount)) + 1);
}

void FaseGrooveLeadProcessor::setChordCount (int count)
{
    setParameterFromUI (ParamIDs::chordCount, (float) (juce::jlimit (1, fgl::maxChords, count) - 1));
}

void FaseGrooveLeadProcessor::transposeChords (int semitones)
{
    for (int i = 0; i < fgl::maxChords; ++i)
    {
        auto c = chordAt (i);
        c.root = ((c.root + semitones) % 12 + 12) % 12;
        setChord (i, c);
    }
}

void FaseGrooveLeadProcessor::setKey (int key)
{
    key = (key % 12 + 12) % 12;
    // From the key the chords are in (the parameter may have moved without them).
    const int old = appliedKey.load() >= 0 ? appliedKey.load() : juce::roundToInt (raw (ParamIDs::key));
    appliedKey = key;
    if (juce::roundToInt (raw (ParamIDs::key)) != key)
        setParameterFromUI (ParamIDs::key, (float) key);
    if (key == old)
        return;
    // The shortest way: up to six semitones up or down.
    int delta = ((key - old) % 12 + 12) % 12;
    if (delta > 6)
        delta -= 12;
    transposeChords (delta);
}

void FaseGrooveLeadProcessor::setKeyAndScale (int key, int scale)
{
    key = (key % 12 + 12) % 12;
    scale = juce::jlimit (0, fgl::numScales - 1, scale);
    const int old = appliedKey.load() >= 0 ? appliedKey.load() : juce::roundToInt (raw (ParamIDs::key));
    const int oldScale = juce::roundToInt (raw (ParamIDs::scale));
    // Measured on the relative major: C major to A minor moves nothing.
    const int from = old + (fgl::scaleIsMinor (oldScale) ? 3 : 0), to = key + (fgl::scaleIsMinor (scale) ? 3 : 0);
    int delta = ((to - from) % 12 + 12) % 12;
    if (delta > 6)
        delta -= 12;
    appliedKey = key;
    setParameterFromUI (ParamIDs::key, (float) key);
    setParameterFromUI (ParamIDs::scale, (float) scale);
    if (delta != 0)
        transposeChords (delta);
}

void FaseGrooveLeadProcessor::followKeyParameter()
{
    const int key = juce::roundToInt (raw (ParamIDs::key));
    if (appliedKey.load() < 0)
        appliedKey = key;
    else if (key != appliedKey.load())
        setKey (key);
}

void FaseGrooveLeadProcessor::randomizeChords()
{
    // A progression that suits the genre the dice write in, now and then any other one, and
    // sometimes a new walk round the circle of fifths.
    auto& r = juce::Random::getSystemRandom();
    const int genre = juce::roundToInt (raw (ParamIDs::genre));
    const auto& list = fgl::progressions();
    juce::Array<int> fits;
    for (int i = 0; i < (int) list.size(); ++i)
        if (juce::String (list[(size_t) i].category) == fgl::genreProgressionCategory (genre))
            fits.add (i);
    const int current = juce::roundToInt (raw (ParamIDs::progression));
    // Funky and dance stabs stay inside their list: a throw must not land on an Italo or pop anthem.
    const bool strict = genre == 8 || genre == 12;
    int pick = ! fits.isEmpty() && (strict || r.nextInt (3) != 0) ? fits[r.nextInt (fits.size())] : r.nextInt ((int) list.size());
    if (pick == current)
        pick = (pick + 1 + r.nextInt ((int) list.size() - 1)) % (int) list.size();
    applyProgression (pick);
}

fgl::ChordTones FaseGrooveLeadProcessor::chordForBar (long long bar) const
{
    if (followsHeldChord())
        return heldChord();
    const int n = chordCount();
    return fgl::tonesOf (chordAt ((int) (((bar % n) + n) % n)));
}

std::uint32_t FaseGrooveLeadProcessor::freshSeed()
{
    return (std::uint32_t) juce::Random::getSystemRandom().nextInt ({ 1, 100000 });
}

void FaseGrooveLeadProcessor::selectGroove (int pattern)
{
    setParameterFromUI (ParamIDs::pattern, (float) pattern);
    const auto& presets = fgl::leadPatterns();
    if (juce::isPositiveAndBelow (pattern, (int) presets.size()))
        if (const int genre = fgl::categoryIndex (presets[(size_t) pattern].category); genre >= 0)
        {
            setParameterFromUI (ParamIDs::genre, (float) genre);
            setParameterFromUI (ParamIDs::swing, fgl::genreSwing (genre));
        }
}

void FaseGrooveLeadProcessor::rollNewBeat()
{
    setParameterFromUI (ParamIDs::swing, fgl::genreSwing (juce::roundToInt (raw (ParamIDs::genre))));

    auto seed = freshSeed();
    if (seed == (std::uint32_t) juce::roundToInt (raw (ParamIDs::riffSeed)))
        seed = seed % 99999 + 1;

    // Locked parts stay: the new groove goes around them, in the user pattern.
    if (std::any_of (segmentLocked.begin(), segmentLocked.end(), [] (bool b) { return b; }))
    {
        for (int part = 0; part < fgl::numSegments; ++part)
            randomizeSegment (part);
        return;
    }

    setParameterFromUI (ParamIDs::riffSeed, (float) seed);
    setParameterFromUI (ParamIDs::pattern, (float) fgl::randomPatternIndex());
}

void FaseGrooveLeadProcessor::totalRandom()
{
    auto& r = juce::Random::getSystemRandom();
    const int style = r.nextInt (styleNames().size());
    applyStyle (style);
    const int genre = juce::jlimit (0, (int) fgl::grooveCategories().size() - 1,
                                    juce::roundToInt (raw (ParamIDs::genre)));

    // Another groove of that genre, or a brand new one in it; and now and then other chords.
    if (r.nextBool())
        rollNewBeat();
    else
    {
        juce::Array<int> grooves;
        for (size_t i = 0; i < fgl::leadPatterns().size(); ++i)
            if (fgl::leadPatterns()[i].category == fgl::grooveCategories()[(size_t) genre])
                grooves.add ((int) i);
        if (! grooves.isEmpty())
            setParameterFromUI (ParamIDs::pattern, (float) grooves[r.nextInt (grooves.size())]);
    }

    setParameterFromUI (ParamIDs::density, 0.25f + 0.6f * r.nextFloat());
    setParameterFromUI (ParamIDs::swing, juce::jlimit (0.0f, 1.0f, fgl::genreSwing (genre) + (r.nextFloat() - 0.5f) * 0.12f));
    setParameterFromUI (ParamIDs::humanize, 0.05f + 0.35f * r.nextFloat());
    setParameterFromUI (ParamIDs::fillStyle, (float) r.nextInt ((int) fgl::FillStyle::custom));   // not the custom fill: it may be empty
    if (r.nextInt (3) == 0)
        setParameterFromUI (ParamIDs::kit, (float) r.nextInt (GuideLead::numTones));
    if (r.nextBool())
        randomizeChords();
    newFeel();
}

void FaseGrooveLeadProcessor::newFeel()
{
    setParameterFromUI (ParamIDs::humanizeSeed, (float) freshSeed());
}

juce::StringArray FaseGrooveLeadProcessor::styleNames()
{
    juce::StringArray names;
    for (const auto& s : styles)
        names.add (s.name);
    return names;
}

void FaseGrooveLeadProcessor::applyStyle (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) std::size (styles)))
        return;

    const int familyStart=(int)std::size(styles)-fgfamily::numGenres;
    if(index>=familyStart){
        const int genre=index-familyStart;const auto& a=fgfamily::sound(genre);
        setParameterFromUI(ParamIDs::tempo,(float)fgfamily::tempos[(size_t)genre]);
        setParameterFromUI(ParamIDs::swing,fgfamily::swing[(size_t)genre]);
        setParameterFromUI(ParamIDs::section,0);
        setParameterFromUI(ParamIDs::velocity,100);
        setParameterFromUI(ParamIDs::variation,2);
        setParameterFromUI(ParamIDs::genre,(float)genre);
        setParameterFromUI(ParamIDs::humanize,.2f);
        setParameterFromUI(ParamIDs::density,genre==8?.68f:genre==4?.22f:.35f);
        setParameterFromUI(ParamIDs::accents,.85f);
        setParameterFromUI(ParamIDs::kit,(float)a.guitar);
        setParameterFromUI(ParamIDs::tone,(float)a.guitarTone);
        setParameterFromUI(ParamIDs::attackDelay,genre==8?9.6f:genre==4?4.8f:17.6f);
        setParameterFromUI(ParamIDs::letRing,genre==11?1.f:0.f);
        setParameterFromUI(ParamIDs::roomForBand,1);
        setParameterFromUI(ParamIDs::reverb,.07f);
        setParameterFromUI(ParamIDs::fills,0);
        if(genre==4){
            const auto& p=fgl::leadPatterns()[(size_t)indexOfGroove("Chicago Pulse")];
            for(int lane=0;lane<fgl::numLanes;++lane)for(int step=0;step<fgl::userPatternLength;++step){
                const auto& line=p.lanes[(size_t)lane];userGrid[(size_t)lane][(size_t)step]=step%32<16?'.':line[(size_t)step%line.size()];
            }
            resetStepVelocities();
            setParameterFromUI(ParamIDs::pattern,(float)fgl::userPatternIndex());
        }else setParameterFromUI(ParamIDs::pattern,(float)fgl::randomPatternIndex());
        setParameterFromUI(ParamIDs::chordColour,(float)std::min(2,fgfamily::extensions[(size_t)genre]));
        const auto& progressions=fgl::progressions();
        int progression=fgl::genreProgression(genre);
        for(size_t i=0;i<progressions.size();++i)if(juce::String(progressions[i].name)==fgfamily::progressionNames[(size_t)genre]){progression=(int)i;break;}
        applyProgression(progression);
        return;
    }

    const auto& s = styles[index];
    setParameterFromUI (ParamIDs::pattern, (float) indexOfGroove (s.groove));
    setParameterFromUI (ParamIDs::genre, (float) index);
    setParameterFromUI (ParamIDs::swing, fgl::genreSwing (index));
    setParameterFromUI (ParamIDs::humanize, s.humanize);
    setParameterFromUI (ParamIDs::fills, (float) s.fills);
    setParameterFromUI (ParamIDs::fillStyle, (float) (int) s.fillStyle);
    setParameterFromUI (ParamIDs::variation, (float) s.variation);
    setParameterFromUI (ParamIDs::kit, (float) s.kit);
    applyProgression (fgl::genreProgression (index));
}

void FaseGrooveLeadProcessor::resetToDefaults()
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (ranged->getDefaultValue());
            ranged->endChangeGesture();
        }
    for (auto& lane : userGrid)
        for (auto& c : lane)
            c = '.';
    resetStepVelocities();
    for (auto* grid : { &userChance, &userRoll })
        for (auto& lane : *grid)
            for (auto& v : lane)
                v = 0;
    clearCustomFill();
    clearGrooveTemplate();
    segmentLocked.fill (false);
    resetSong();
}

//==============================================================================
void FaseGrooveLeadProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;
    const double sampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    buffer.clear();

    const bool sequencerOn = raw (ParamIDs::sequencer) > 0.5f;
    const int channel = juce::jlimit (1, 16, juce::roundToInt (raw (ParamIDs::midiChannel)));
    const bool chordInput = raw (ParamIDs::chordInput) > 0.5f;

    // ---- MIDI in: the clock drives the standalone app; the notes held are the chord to play
    //      (with Chords from MIDI in), otherwise they pass through (and play the guide lead) ----
    outMidi.clear();
    guideEvents.clear();
    double clockPpqAtStart = clockPpq;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isMidiClock())
        {
            const auto at = samplesProcessed + meta.samplePosition;
            if (lastClockSample >= 0)
                if (const double gap = (double) (at - lastClockSample); gap > 0.0 && gap < sampleRate * 0.25)
                    clockInterval = clockInterval <= 0.0 ? gap : clockInterval * 0.92 + gap * 0.08;
            lastClockSample = at;
            if (clockRunning)
                clockPpq += 1.0 / 24.0;
            continue;
        }
        if (m.isMidiStart())           { clockRunning = true; clockPpq = clockPpqAtStart = 0.0; continue; }
        if (m.isMidiContinue())        { clockRunning = true; clockPpqAtStart = clockPpq; continue; }
        if (m.isMidiStop())            { clockRunning = false; continue; }
        if (m.isSongPositionPointer()) { clockPpq = clockPpqAtStart = m.getSongPositionPointerMidiBeat() / 4.0; continue; }

        if (m.isNoteOnOrOff())
        {
            // Counted per pitch: the same note struck twice stays held until both are released.
            auto& count = heldCount[(size_t) m.getNoteNumber()];
            count = m.isNoteOn() ? (std::uint8_t) juce::jmin (16, count + 1) : (std::uint8_t) juce::jmax (0, count - 1);
            heldNotes[(size_t) m.getNoteNumber()] = count > 0;
            if (chordInput)
                continue;
            guideEvents.push_back ({ meta.samplePosition, m.getNoteNumber(), m.isNoteOn() ? m.getFloatVelocity() : 0.0f, (int) fgl::Lane::hook });
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldNotes.fill (false);
            heldCount.fill (0);
            guideEvents.push_back ({ meta.samplePosition, -1, 0.0f, 0 });
        }

        outMidi.addEvent (m, meta.samplePosition);
    }
    {
        int count = 0;   // (no vector here: nothing allocated on the audio thread)
        const auto chord = fgl::tonesOfHeld (heldNotes, count);
        // A single note is not a chord: it plays as the root of a power chord (root and 5th).
        liveRoot = chord.root;
        liveMask = count == 1 ? 0x81 : (int) chord.mask;
    }
    auto settings = currentSettings();

    // ---- Timing ----
    double dawBpm = 120.0, hostPpq = 0.0;
    bool hostPlaying = false, haveHostPpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())          dawBpm = *b;
            if (auto p = pos->getPpqPosition()) { hostPpq = *p; haveHostPpq = true; }
            hostPlaying = pos->getIsPlaying();
        }

    const bool clockAlive = lastClockSample >= 0 && clockInterval > 0.0
                         && (double) (samplesProcessed - lastClockSample) < sampleRate * 0.5;
    if (! haveHostPpq && clockAlive)
    {
        dawBpm = juce::roundToInt (600.0 * sampleRate / (24.0 * clockInterval)) / 10.0;
        hostPlaying = clockRunning;
        hostPpq = clockPpqAtStart;
        haveHostPpq = clockRunning;
    }

    // Synced: the DAW's tempo and position. Unsynced: our own BPM and clock,
    // still started / stopped by the DAW's play button.
    const bool tempoSynced = raw (ParamIDs::tempoSync) > 0.5f;
    hostBpm = juce::jlimit (20.0, 400.0, dawBpm);
    const double bpm = tempoSynced ? hostBpm.load() : juce::jlimit (20.0, 400.0, (double) raw (ParamIDs::tempo));
    lastBpm = bpm;
    settings.tempo = bpm;
    const double samplesPerBeat = sampleRate * 60.0 / bpm;
    const double blockBeats = numSamples / samplesPerBeat;

    const bool followHost = tempoSynced && hostPlaying && haveHostPpq;
    const bool preview = previewOn.load();
    const bool running = (sequencerOn && hostPlaying) || preview;

    double startPpq = 0.0;
    double linkCorrection = 0.0;   // see the linked clock below
    if (followHost)
        startPpq = hostPpq;
    else if (running)
    {
        // Linked: the shared clock, so the linked plug-ins play the same beat at the same time.
        // Else a free-running clock: from the start, or where PAUSE left it; REWIND jumps back.
        if (const double anchor = linkAnchor.load(); linkOn.load() && preview && anchor >= 0.0)
        {
            // The shared clock is the system time, which doesn't move with the audio: read
            // block by block it jumps back and forth, and notes would play twice or not at all.
            // So the beat goes on from the last block, and drifts gently towards the shared clock
            // (it only jumps when far from it: a start, or another plug-in moved it).
            const double shared = juce::jmax (0.0, (juce::Time::getMillisecondCounterHiRes() - anchor) * bpm / 60000.0);
            if (wasRunning && std::abs (shared - expectedNextPpq) < 0.5)
            {
                startPpq = expectedNextPpq;
                linkCorrection = juce::jlimit (-0.03 * blockBeats, 0.03 * blockBeats, 0.05 * (shared - expectedNextPpq));
            }
            else
                startPpq = shared;
            previewRewind = false;
        }
        else
        {
            startPpq = wasRunning || ! previewFromStart.load() ? internalPpq : 0.0;
            if (previewRewind.exchange (false))
                startPpq = 0.0;
        }
        if (! wasRunning)
            previewFromStart = true;
    }
    else
        previewRewind = false;

    events.clear();
    if (running)
    {
        if (! wasRunning || std::abs (startPpq - expectedNextPpq) > 0.05)
            engine.allNotesOff (startPpq, events);   // start, loop or jump

        const auto ready = std::atomic_load (&preparedPattern);   // the message thread keeps it alive
        engine.setPrepared (ready.get());
        engine.render (startPpq, startPpq + blockBeats + linkCorrection, settings, events);
        engine.setPrepared (nullptr);
        expectedNextPpq = internalPpq = startPpq + blockBeats + linkCorrection;

        const auto step = (long long) std::floor (startPpq / fgl::stepLength);
        const int n = fgl::GrooveEngine::patternLength (settings);
        uiStep = (int) (((step % n) + n) % n);
        uiPpq = startPpq;
        uiSteps = n;
        playClock.publish (startPpq, blockBeats + linkCorrection, numSamples / juce::jmax (1.0, getSampleRate()));
    }
    else
    {
        if (engine.anyNoteSounding())
            engine.allNotesOff (startPpq, events);
        uiStep = -1;
        uiPpq = -1.0;
        playClock.stop();
    }
    wasRunning = running;

    for (const auto& e : events)
    {
        const int pos = juce::jlimit (0, numSamples - 1, (int) std::floor ((e.ppq - startPpq) * samplesPerBeat));
        if (e.isController())
        {
            outMidi.addEvent (juce::MidiMessage::controllerEvent (channel, e.note, juce::jlimit (0, 127, e.velocity)), pos);
            continue;
        }
        const bool keyswitch = e.lane == fgl::keyswitchLane;   // for the lead instrument, not the guide
        if (e.isNoteOn())
        {
            noteChannels[(size_t) e.note] = channel;
            outMidi.addEvent (juce::MidiMessage::noteOn (channel, e.note, (juce::uint8) e.velocity), pos);
            if (! keyswitch)
                laneFlash[(size_t) juce::jlimit (0, fgl::numLanes - 1, e.lane)] = (float) e.velocity / 127.0f;
        }
        else
        {
            auto& original = noteChannels[(size_t) e.note];
            outMidi.addEvent (juce::MidiMessage::noteOff (original > 0 ? original : channel, e.note), pos);
            original = 0;
        }
        if (! keyswitch)
            guideEvents.push_back ({ pos, e.note, e.isNoteOn() ? (float) e.velocity / 127.0f : 0.0f, e.lane });
    }

    // Audition timestamps are absolute samples: small host blocks must not collapse a strum.
    if (const int request = auditionRequest.exchange (-1); request >= 0 && auditionCount == 0)
    {
        auditionLane = juce::jlimit (0, fgl::numLanes - 1, request / 128);
        const double ppq = juce::jmax (0.0, uiPpq.load());
        // (no allocation here: the shapes and the notes in fixed arrays)
        const auto chord = chordForBar ((long long) std::floor (ppq / 4.0));
        const auto voicing = fgl::voiceChord (chord, nullptr, settings.roomForBand);
        fgl::NoteList notes;
        fgl::laneNotes ((fgl::Lane) auditionLane, chord, voicing, (int) std::floor (ppq / fgl::stepLength) % 16,
                        settings.octaves[(size_t) auditionLane] + settings.globalOctave, notes, settings.roomForBand, false, 0, settings.key, settings.scale, settings.riffSeed);
        if (auditionLane == (int) fgl::Lane::answer)
            std::reverse (notes.begin(), notes.end());   // high to low
        auditionCount = juce::jmin ((int) notes.size(), (int) auditionNotes.size());
        std::copy_n (notes.begin(), auditionCount, auditionNotes.begin());
        auditionChannel = channel;
        auditionVelocity = request % 128;
        auditionStart = samplesProcessed;
        auditionGap = juce::roundToInt (settings.attackMs * 0.001 * sampleRate);
        auditionDuration = (juce::int64) (sampleRate * 0.5);
        laneFlash[(size_t) auditionLane] = (float) auditionVelocity / 127.0f;
    }
    for (int i = 0; i < auditionCount; ++i)
    {
        const auto on = auditionStart + i * auditionGap;
        const auto off = on + auditionDuration;
        if (on >= samplesProcessed && on < samplesProcessed + numSamples)
        {
            const int at = (int) (on - samplesProcessed);
            outMidi.addEvent (juce::MidiMessage::noteOn (auditionChannel, auditionNotes[(size_t) i], (juce::uint8) auditionVelocity), at);
            guideEvents.push_back ({ at, auditionNotes[(size_t) i], (float) auditionVelocity / 127.0f, auditionLane });
        }
        if (off >= samplesProcessed && off < samplesProcessed + numSamples)
        {
            const int at = (int) (off - samplesProcessed);
            outMidi.addEvent (juce::MidiMessage::noteOff (auditionChannel, auditionNotes[(size_t) i]), at);
            guideEvents.push_back ({ at, auditionNotes[(size_t) i], 0.0f, auditionLane });
        }
    }
    if (auditionCount > 0 && auditionStart + (auditionCount - 1) * auditionGap + auditionDuration < samplesProcessed + numSamples)
        auditionCount = 0;

    // ---- The guide lead ----
    const bool soundOn = raw (ParamIDs::sound) > 0.5f;
    if (soundWasOn && ! soundOn)
        lead.allNotesOff();
    soundWasOn = soundOn;

    if (soundOn)
    {
        std::array<float, fgl::numLanes> gains {};
        for (int l = 0; l < fgl::numLanes; ++l)
        {
            const float db = load (laneParams[(size_t) l].volume);
            gains[(size_t) l] = db <= -47.9f ? 0.0f : juce::Decibels::decibelsToGain (db);
        }
        lead.setSound (juce::roundToInt (raw (ParamIDs::kit)));
        lead.setTone (raw (ParamIDs::tone));
        // In time order (nearly sorted already): an insertion sort, no allocation on the audio thread.
        for (size_t i = 1; i < guideEvents.size(); ++i)
            for (size_t j = i; j > 0 && guideEvents[j].sample < guideEvents[j - 1].sample; --j)
                std::swap (guideEvents[j], guideEvents[j - 1]);
        lead.render (buffer, guideEvents, gains);

        const float amount = raw (ParamIDs::reverb);
        if (amount > 0.001f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.45f;
            rp.damping = 0.5f;
            rp.width = 1.0f;
            rp.wetLevel = amount * 0.4f;
            rp.dryLevel = 1.0f - amount * 0.25f;
            reverbFx.setParameters (rp);
            if (buffer.getNumChannels() > 1)
                reverbFx.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
            else
                reverbFx.processMono (buffer.getWritePointer (0), numSamples);
        }

        outputGain.setTargetValue (outputLevel());
        outputGain.applyGain (buffer, numSamples);
        softLimit (buffer, numSamples);
    }

    samplesProcessed += numSamples;
    midi.swapWith (outMidi);
}

//==============================================================================
float FaseGrooveLeadProcessor::outputLevel() const
{
    // Short stabs read quiet next to piano and drums: +8 dB of make-up under the Volume knob.
    return juce::Decibels::decibelsToGain (raw (ParamIDs::volume) + 8.0f);
}

void FaseGrooveLeadProcessor::softLimit (juce::AudioBuffer<float>& buffer, int numSamples)
{
    // Transparent below 0.8; above it a soft knee that never passes 1.0.
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* x = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float a = std::abs (x[i]);
            if (a > 0.8f)
                x[i] = std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), x[i]);
        }
    }
}

//==============================================================================
double FaseGrooveLeadProcessor::exportBpm() const
{
    return raw (ParamIDs::tempoSync) > 0.5f ? hostBpm.load() : (double) raw (ParamIDs::tempo);
}

int FaseGrooveLeadProcessor::clipLengthInBars (int bars) const
{
    if (bars == songArrangement)
    {
        int total = 0;
        for (const auto& p : songArrangementParts())
            total += juce::jmax (0, p.bars);
        return juce::jmax (1, total);
    }
    if (bars > 0)
        return bars;
    return juce::jmax (1, fgl::GrooveEngine::loopLengthInSteps (currentSettings()) / 16);
}

juce::MidiFile FaseGrooveLeadProcessor::midiFromEvents (const std::vector<fgl::NoteEvent>& clipEvents, double beats,
                                                         const std::vector<fgl::GrooveEngine::Marker>* markers,
                                                         ClipPart part, const juce::String& name) const
{
    constexpr int ticksPerQuarter = 960;
    const int channel = juce::jlimit (1, 16, juce::roundToInt (raw (ParamIDs::midiChannel)));

    // Track 1 (or the conductor track of a multitrack file): name, tempo, metre and the markers.
    juce::MidiMessageSequence header;
    header.addEvent (juce::MidiMessage::textMetaEvent (3, name), 0.0);
    header.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / exportBpm())), 0.0);
    header.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    if (markers != nullptr)
        for (const auto& m : *markers)
            header.addEvent (juce::MidiMessage::textMetaEvent (6, m.name), m.ppq * ticksPerQuarter);

    auto notesOf = [&] (juce::MidiMessageSequence& track, int lane)
    {
        // Clean notes for any instrument: each note-on paired with its note-off, then per pitch
        // a note ends where the next one of the same pitch starts (a string struck again), two
        // at the same moment become one, every note lasts at least a 64th when it has room, and
        // at the same tick the note-offs are written before the note-ons.
        struct Note { int pitch; double start, end; int velocity; };
        std::vector<Note> notes;
        std::array<std::vector<size_t>, 128> open;   // per pitch, the notes still waiting for their off
        const double endTick = beats * ticksPerQuarter;
        auto toTick = [&] (double ppq) { return std::round (juce::jlimit (0.0, beats, ppq) * ticksPerQuarter); };
        // A track of its own (one lane) sends its own keyswitches, when they are asked for.
        const bool ownKeyswitches = lane >= 0 && raw (ParamIDs::keyswitches) > 0.5f;
        int lastArticulation = -1;
        for (const auto& e : clipEvents)
        {
            if (lane >= 0 && e.lane != lane)
                continue;
            if (e.isController())
            {
                track.addEvent (juce::MidiMessage::controllerEvent (channel, e.note, juce::jlimit (0, 127, e.velocity)), toTick (e.ppq));
                continue;
            }
            if (ownKeyswitches && e.isNoteOn() && e.articulation != lastArticulation)
            {
                const int ks = juce::jlimit (0, 39, juce::roundToInt (raw (ParamIDs::keyswitch (juce::jlimit (0, fgl::numArticulations - 1, e.articulation)))));
                notes.push_back ({ ks, toTick (e.ppq), juce::jmin (endTick, toTick (e.ppq + 0.03)), 100 });
                lastArticulation = e.articulation;
            }
            const int pitch = juce::jlimit (0, 127, e.note);
            if (e.isNoteOn())
            {
                open[(size_t) pitch].push_back (notes.size());
                notes.push_back ({ pitch, toTick (e.ppq), endTick, juce::jlimit (1, 127, (int) e.velocity) });
            }
            else if (! open[(size_t) pitch].empty())
            {
                // The off closes the oldest note of that pitch still open.
                auto& waiting = open[(size_t) pitch];
                size_t best = 0;
                for (size_t k = 1; k < waiting.size(); ++k)
                    if (notes[waiting[k]].start < notes[waiting[best]].start)
                        best = k;
                notes[waiting[best]].end = juce::jmax (notes[waiting[best]].start, toTick (e.ppq));
                waiting.erase (waiting.begin() + (std::ptrdiff_t) best);
            }
        }
        std::stable_sort (notes.begin(), notes.end(), [] (const Note& x, const Note& y)
                          { return x.pitch != y.pitch ? x.pitch < y.pitch : x.start < y.start; });
        const double minLength = ticksPerQuarter / 16.0;
        std::vector<Note> clean;
        for (size_t k = 0; k < notes.size(); ++k)
        {
            auto n = notes[k];
            if (n.start >= endTick)
                continue;
            if (! clean.empty() && clean.back().pitch == n.pitch && clean.back().start == n.start)
            {
                // The same note twice at once (two lanes): one, the louder and the longer.
                clean.back().velocity = juce::jmax (clean.back().velocity, n.velocity);
                clean.back().end = juce::jmax (clean.back().end, n.end);
                continue;
            }
            n.end = juce::jmin (endTick, juce::jmax (n.end, n.start + minLength));
            clean.push_back (n);
        }
        for (size_t k = 0; k + 1 < clean.size(); ++k)   // struck again: the earlier one stops there
            if (clean[k + 1].pitch == clean[k].pitch)
                clean[k].end = juce::jmin (clean[k].end, clean[k + 1].start);
        for (const auto& n : clean)
            if (n.end > n.start)
                track.addEvent (juce::MidiMessage::noteOff (channel, n.pitch), n.end);
        for (const auto& n : clean)   // (added after the offs: at the same tick they follow them)
            if (n.end > n.start)
                track.addEvent (juce::MidiMessage::noteOn (channel, n.pitch, (juce::uint8) n.velocity), n.start);
        // Every clip as long as asked: many DAWs cut a clip after its last event, so the end
        // gets a silent one (pitch bend in the middle: no change).
        const double end = beats * ticksPerQuarter;
        track.addEvent (juce::MidiMessage::pitchWheel (channel, 8192), end);
        track.addEvent (juce::MidiMessage::endOfTrack(), end);
        track.updateMatchedPairs();
    };

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ticksPerQuarter);

    if (part == ClipPart::tracks)
    {
        // One file, one track per lane: the DAW puts repeats, stabs, licks... on separate tracks.
        header.addEvent (juce::MidiMessage::endOfTrack(), beats * ticksPerQuarter);
        file.addTrack (header);
        for (int l = 0; l < fgl::numLanes; ++l)
        {
            if (std::none_of (clipEvents.begin(), clipEvents.end(), [l] (const fgl::NoteEvent& e) { return e.lane == l && e.isNoteOn(); }))
                continue;
            juce::MidiMessageSequence track;
            track.addEvent (juce::MidiMessage::textMetaEvent (3, fgl::laneName (l)), 0.0);
            notesOf (track, l);
            file.addTrack (track);
        }
    }
    else
    {
        notesOf (header, -1);
        file.addTrack (header);
    }
    return file;
}

bool FaseGrooveLeadProcessor::writeMidiFileSafely (const juce::MidiFile& midi, const juce::File& target)
{
    return FaseGroove::writeMidiFileSafely (midi, target);
}

juce::MidiFile FaseGrooveLeadProcessor::createMidiClip (int bars, ClipPart part) const
{
    const auto settings = currentSettings();
    const double beats = clipLengthInBars (bars) * 4.0;
    const auto name = clipFileName (bars, part).upToLastOccurrenceOf (".mid", false, false);

    if (bars == songArrangement)
    {
        std::vector<fgl::GrooveEngine::Marker> markers;
        const auto songEvents = fgl::GrooveEngine::renderArrangement (settings, songArrangementParts(), &markers);
        return midiFromEvents (songEvents, beats, &markers, part, name);
    }

    return midiFromEvents (fgl::GrooveEngine::renderClip (settings, beats), beats, nullptr, part, name);
}

juce::String FaseGrooveLeadProcessor::clipFileName (int bars, ClipPart part) const
{
    const auto settings = currentSettings();
    const auto groove = fgl::GrooveEngine::basePattern (settings).name;
    juce::String name = "Lead " + juce::String (groove);
    if (settings.section != fgl::Section::main && bars != songArrangement)
        name << " " << fgl::sectionName ((int) settings.section);
    name << " " << juce::roundToInt (exportBpm()) << "bpm";
    name << (bars == songArrangement ? juce::String (" Song") : " " + juce::String (clipLengthInBars (bars)) + " bars");
    if (part == ClipPart::tracks)
        name << " (tracks)";
    return juce::File::createLegalFileName (name.replace ("'", "")) + ".mid";
}

juce::File FaseGrooveLeadProcessor::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Fase Groove Lead");
}

juce::File FaseGrooveLeadProcessor::clipFolder()
{
    return userFolder().getChildFile ("MIDI clips");
}

juce::File FaseGrooveLeadProcessor::writeMidiClip (int bars, ClipPart part) const
{
    auto folder = clipFolder();
    if (! folder.createDirectory())
        folder = juce::File::getSpecialLocation (juce::File::tempDirectory);

    auto file = folder.getChildFile (clipFileName (bars, part));
    return writeMidiFileSafely (createMidiClip (bars, part), file) ? file : juce::File();
}

//==============================================================================
void FaseGrooveLeadProcessor::renderGuideAudio (const std::vector<fgl::NoteEvent>& clipEvents, double beats,
                                                 juce::AudioBuffer<float>& out, double sampleRate, int onlyLane) const
{
    const double samplesPerBeat = sampleRate * 60.0 / exportBpm();
    const int total = juce::jmax (1, (int) std::ceil (beats * samplesPerBeat));
    out.setSize (2, total);
    out.clear();

    GuideLead guide;
    guide.prepare (sampleRate);
    guide.setSound (juce::roundToInt (raw (ParamIDs::kit)));
    guide.setTone (raw (ParamIDs::tone));
    std::array<float, fgl::numLanes> gains {};
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        const float db = raw (ParamIDs::laneVolume (l));
        gains[(size_t) l] = (onlyLane >= 0 && l != onlyLane) || db <= -47.9f ? 0.0f : juce::Decibels::decibelsToGain (db);
    }

    juce::Reverb reverb;
    reverb.setSampleRate (sampleRate);
    const float amount = raw (ParamIDs::reverb);
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.45f;
    rp.damping = 0.5f;
    rp.width = 1.0f;
    rp.wetLevel = amount * 0.4f;
    rp.dryLevel = 1.0f - amount * 0.25f;
    reverb.setParameters (rp);
    const float gain = outputLevel();

    constexpr int renderBlock = 256;
    juce::AudioBuffer<float> block (2, renderBlock);
    size_t next = 0;
    for (int start = 0; start < total; start += renderBlock)
    {
        const int n = juce::jmin (renderBlock, total - start);
        std::vector<GuideLead::Event> blockEvents;
        while (next < clipEvents.size() && clipEvents[next].ppq * samplesPerBeat < start + n)
        {
            const auto& e = clipEvents[next++];
            const int at = juce::jlimit (0, n - 1, (int) std::floor (e.ppq * samplesPerBeat) - start);
            if ((onlyLane >= 0 && e.lane != onlyLane) || e.isController() || e.lane == fgl::keyswitchLane)
                continue;
            blockEvents.push_back ({ at, e.note, e.isNoteOn() ? (float) e.velocity / 127.0f : 0.0f, e.lane });
        }
        block.setSize (2, n, false, false, true);
        block.clear();
        guide.render (block, blockEvents, gains);
        if (amount > 0.001f)
            reverb.processStereo (block.getWritePointer (0), block.getWritePointer (1), n);
        block.applyGain (0, n, gain);
        softLimit (block, n);
        for (int ch = 0; ch < 2; ++ch)
            out.addFrom (ch, start, block, ch, 0, n);
    }
}

juce::String FaseGrooveLeadProcessor::audioFileName (AudioExport mode, int bars) const
{
    const auto midiName = clipFileName (mode == AudioExport::song ? songArrangement : bars, ClipPart::all);
    return midiName.upToLastOccurrenceOf (".mid", false, false) + (mode == AudioExport::stems ? " (stems)" : ".wav");
}

juce::Array<juce::File> FaseGrooveLeadProcessor::writeAudio (AudioExport mode, int bars, const juce::File& target) const
{
    const double sampleRate = getSampleRate() >= 44100.0 ? getSampleRate() : 44100.0;
    const auto settings = currentSettings();
    juce::Array<juce::File> written;

    auto write = [&] (const juce::File& file, const juce::AudioBuffer<float>& audio)
    {
        // Whole or not at all: a temporary file first, the one already there replaced only
        // once the new one is complete.
        juce::TemporaryFile temp (file);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream (temp.getFile().createOutputStream());
        if (stream == nullptr)
            return;
        const auto options = juce::AudioFormatWriterOptions().withSampleRate (sampleRate).withNumChannels (2).withBitsPerSample (24);
        bool ok = false;
        if (auto writer = wav.createWriterFor (stream, options))
            ok = writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()) && writer->flush();
        // (the writer is gone here: the file is closed)
        if (ok && temp.overwriteTargetFileWithTemporary())
            written.add (file);
    };

    if (mode == AudioExport::song)
    {
        // The whole song, with two seconds for the last sounds to ring out.
        const double beats = clipLengthInBars (songArrangement) * 4.0;
        const auto songEvents = fgl::GrooveEngine::renderArrangement (settings, songArrangementParts());
        juce::AudioBuffer<float> audio;
        renderGuideAudio (songEvents, beats + exportBpm() / 30.0, audio, sampleRate, -1);
        write (target, audio);
        return written;
    }

    // A clip that loops seamlessly: rendered twice, the second time (with the tails of the first
    // ringing into it) is the one kept.
    const double beats = clipLengthInBars (bars) * 4.0;
    auto twice = fgl::GrooveEngine::renderClip (settings, beats * 2.0);
    auto loopOf = [&] (int lane)
    {
        juce::AudioBuffer<float> both, audio;
        renderGuideAudio (twice, beats * 2.0, both, sampleRate, lane);
        const int half = both.getNumSamples() / 2;
        audio.setSize (2, half);
        for (int ch = 0; ch < 2; ++ch)
            audio.copyFrom (ch, 0, both, ch, both.getNumSamples() - half, half);
        return audio;
    };

    if (mode == AudioExport::clip)
    {
        write (target, loopOf (-1));
        return written;
    }

    // Stems: one file per lane that plays.
    if (! target.createDirectory())
        return written;
    for (int l = 0; l < fgl::numLanes; ++l)
        if (std::any_of (twice.begin(), twice.end(), [l] (const fgl::NoteEvent& e) { return e.lane == l && e.isNoteOn(); }))
            write (target.getChildFile (juce::String (l + 1).paddedLeft ('0', 2) + " " + fgl::laneName (l) + ".wav"), loopOf (l));
    return written;
}

//==============================================================================
juce::File FaseGrooveLeadProcessor::linkFile()
{
    return FaseGroove::LinkClient::file();
}

void FaseGrooveLeadProcessor::setKeyLinked (bool on)
{
    keyLinked = on;
    link.shareAgain();   // switched on: this key goes out to the others
    updateLink();
}

void FaseGrooveLeadProcessor::setLinked (bool on)
{
    if (on == linkOn.load())
        return;
    linkOn = on;
    link.reset();
    linkAnchor = -1.0;
}

int FaseGrooveLeadProcessor::currentGenre() const
{
    // The groove's own genre (a preset), else the genre the dice write in.
    const int pattern = juce::roundToInt (raw (ParamIDs::pattern));
    const auto& presets = fgl::leadPatterns();
    if (juce::isPositiveAndBelow (pattern, (int) presets.size()))
        if (const int g = fgl::categoryIndex (presets[(size_t) pattern].category); g >= 0)
            return g;
    return juce::roundToInt (raw (ParamIDs::genre));
}

FaseGrooveLeadProcessor::LinkState FaseGrooveLeadProcessor::localLinkState() const
{
    LinkState s;
    s.genre = currentGenre();
    s.swing = raw (ParamIDs::swing);
    s.bpm = raw (ParamIDs::tempo);
    s.sync = raw (ParamIDs::tempoSync) > 0.5f;
    s.playing = previewOn.load();
    s.paused = ! s.playing && ! previewFromStart.load();
    s.anchor = s.playing ? linkAnchor.load() : -1.0;
    if (keyLinked.load())
    {
        s.key = juce::roundToInt (raw (ParamIDs::key));
        s.scale = juce::roundToInt (raw (ParamIDs::scale));
    }
    return s;
}

void FaseGrooveLeadProcessor::previewPlay()
{
    // Linked: the clock starts now (or where PAUSE left it), and everyone plays from it.
    if (linkOn.load())
        linkAnchor = juce::Time::getMillisecondCounterHiRes() - (previewFromStart.load() ? 0.0 : internalPpq * 60000.0 / lastBpm.load());
    previewOn = true;
    updateLink();
}

void FaseGrooveLeadProcessor::previewPause()
{
    previewOn = false;
    previewFromStart = false;
    updateLink();
}

void FaseGrooveLeadProcessor::previewStop()
{
    previewOn = false;
    previewFromStart = true;
    linkAnchor = -1.0;
    updateLink();
}

void FaseGrooveLeadProcessor::previewToStart()
{
    previewFromStart = true;
    previewRewind = true;
    if (linkOn.load() && previewOn.load())
        linkAnchor = juce::Time::getMillisecondCounterHiRes();
    updateLink();
}

void FaseGrooveLeadProcessor::updateLink()
{
    if (! linkOn.load())
        return;
    link.update();
}

juce::String FaseGrooveLeadProcessor::linkGenreName (int genre) const
{
    return fgl::grooveCategories()[(size_t) juce::jlimit (0, (int) fgl::grooveCategories().size() - 1, genre)];
}

void FaseGrooveLeadProcessor::followLinkedGroove (const juce::XmlElement& xml)
{
    // Their genre: the first groove of it (unless this one already is), and the dice's genre.
    const int theirs = fgl::categoryIndex (xml.getStringAttribute ("genre").toStdString());
    if (theirs >= 0 && theirs != currentGenre())
    {
        const int pattern = juce::roundToInt (raw (ParamIDs::pattern));
        const auto& presets = fgl::leadPatterns();
        if (juce::isPositiveAndBelow (pattern, (int) presets.size()))
            for (size_t i = 0; i < presets.size(); ++i)
                if (presets[i].category == fgl::grooveCategories()[(size_t) theirs])
                {
                    setParameterFromUI (ParamIDs::pattern, (float) i);
                    break;
                }
        setParameterFromUI (ParamIDs::genre, (float) theirs);
    }
    if (keyLinked.load() && xml.hasAttribute ("key"))   // the same key (the chords move the shortest way)
    {
        if (xml.hasAttribute ("scale"))   // (a relative key keeps the chords where they are)
            setKeyAndScale (xml.getIntAttribute ("key"), xml.getIntAttribute ("scale"));
        else
            setKey (xml.getIntAttribute ("key"));
    }
}

//==============================================================================
juce::ValueTree FaseGrooveLeadProcessor::stateTree() const
{
    auto state = const_cast<juce::AudioProcessorValueTreeState&> (apvts).copyState();   // copyState() locks, it changes nothing

    juce::ValueTree grid ("Grid");
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        juce::String line, levels, chances, rolls;
        bool extras = false;
        for (int i = 0; i < gridSteps; ++i)
        {
            line << juce::String::charToString ((juce::juce_wchar) (unsigned char) userGrid[(size_t) l][(size_t) i].load());
            levels << juce::String::toHexString (juce::jlimit (0, 127, userVelocity[(size_t) l][(size_t) i].load())).paddedLeft ('0', 2);
            chances << juce::String::toHexString (juce::jlimit (0, 100, userChance[(size_t) l][(size_t) i].load())).paddedLeft ('0', 2);
            rolls << juce::String (juce::jlimit (0, 4, userRoll[(size_t) l][(size_t) i].load()));
            extras = extras || userChance[(size_t) l][(size_t) i].load() > 0 || userRoll[(size_t) l][(size_t) i].load() > 1;
        }
        grid.setProperty ("lane" + juce::String (l + 1), line, nullptr);
        grid.setProperty ("velocity" + juce::String (l + 1), levels, nullptr);
        if (extras)
        {
            grid.setProperty ("chance" + juce::String (l + 1), chances, nullptr);
            grid.setProperty ("roll" + juce::String (l + 1), rolls, nullptr);
        }
    }

    juce::ValueTree fill ("Fill");
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        juce::String line;
        for (int i = 0; i < 16; ++i)
            line << juce::String::charToString ((juce::juce_wchar) (unsigned char) customFill[(size_t) l][(size_t) i].load());
        fill.setProperty ("lane" + juce::String (l + 1), line, nullptr);
    }

    juce::ValueTree groove ("Template");
    if (templateName.isNotEmpty())
    {
        juce::String timing, level;
        for (int i = 0; i < 16; ++i)
        {
            timing << juce::String (templateTiming[(size_t) i].load(), 4) << " ";
            level << juce::String (templateLevel[(size_t) i].load(), 4) << " ";
        }
        groove.setProperty ("name", templateName, nullptr);
        groove.setProperty ("active", templateActive.load(), nullptr);
        groove.setProperty ("timing", timing.trim(), nullptr);
        groove.setProperty ("level", level.trim(), nullptr);
    }
    juce::ValueTree link ("Link");
    link.setProperty ("on", linkOn.load(), nullptr);
    link.setProperty ("key", keyLinked.load(), nullptr);
    juce::String locks;
    for (bool b : segmentLocked)
        locks << (b ? "1" : "0");
    grid.setProperty ("locks", locks, nullptr);

    juce::ValueTree song ("Song");
    for (int i = 0; i < numSongParts; ++i)
    {
        juce::ValueTree part ("Part");
        part.setProperty ("bars", songBars[(size_t) i].load(), nullptr);
        part.setProperty ("variation", (juce::int64) songVariation[(size_t) i].load(), nullptr);
        part.setProperty ("fill", songFill[(size_t) i].load(), nullptr);
        song.appendChild (part, nullptr);
    }

    for (const auto* name : { "Grid", "Song", "Fill", "Template", "Link" })
        state.removeChild (state.getChildWithName (name), nullptr);
    for (auto& child : { grid, song, fill, groove, link })
        state.appendChild (child, nullptr);
    return state;
}

void FaseGrooveLeadProcessor::applyStateTree (const juce::ValueTree& source)
{
    if (! source.hasType (apvts.state.getType()))
        return;

    auto state = source.createCopy();
    const auto grid = state.getChildWithName ("Grid");
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        const auto line = grid.getProperty ("lane" + juce::String (l + 1)).toString();
        const auto levels = grid.getProperty ("velocity" + juce::String (l + 1)).toString();
        const auto chances = grid.getProperty ("chance" + juce::String (l + 1)).toString();
        const auto rolls = grid.getProperty ("roll" + juce::String (l + 1)).toString();
        for (int i = 0; i < gridSteps; ++i)
        {
            const char c = i < line.length() ? (char) line[i] : '.';
            userGrid[(size_t) l][(size_t) i] = fgl::isHit (c) ? c : '.';
            userVelocity[(size_t) l][(size_t) i] = juce::jlimit (0, 127, levels.substring (i * 2, i * 2 + 2).getHexValue32());
            userChance[(size_t) l][(size_t) i] = juce::jlimit (0, 100, chances.substring (i * 2, i * 2 + 2).getHexValue32());
            userRoll[(size_t) l][(size_t) i] = juce::jlimit (0, 4, rolls.substring (i, i + 1).getIntValue());
        }
    }

    const auto fill = state.getChildWithName ("Fill");
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        const auto line = fill.getProperty ("lane" + juce::String (l + 1)).toString();
        for (int i = 0; i < 16; ++i)
        {
            const char c = i < line.length() ? (char) line[i] : '.';
            customFill[(size_t) l][(size_t) i] = fgl::isHit (c) ? c : '.';
        }
    }

    const auto groove = state.getChildWithName ("Template");
    {
        std::array<float, 16> timing {}, level {};
        level.fill (1.0f);
        const auto t = juce::StringArray::fromTokens (groove.getProperty ("timing").toString(), " ", {});
        const auto v = juce::StringArray::fromTokens (groove.getProperty ("level").toString(), " ", {});
        for (int i = 0; i < 16; ++i)
        {
            if (i < t.size()) timing[(size_t) i] = juce::jlimit (-0.5f, 0.5f, t[i].getFloatValue());
            if (i < v.size()) level[(size_t) i] = juce::jlimit (0.2f, 1.6f, v[i].getFloatValue());
        }
        setTemplate (groove.getProperty ("name").toString(), timing, level);
        templateActive = templateName.isNotEmpty() && (bool) groove.getProperty ("active", true);
    }
    keyLinked = false;
    if (const auto link = state.getChildWithName ("Link"); link.isValid())
    {
        keyLinked = (bool) link.getProperty ("key", false);
        setLinked ((bool) link.getProperty ("on", false));
    }
    else
        setLinked (false);
    const auto locks = grid.getProperty ("locks").toString();
    for (int i = 0; i < fgl::numSegments; ++i)
        segmentLocked[(size_t) i] = locks.substring (i, i + 1) == "1";

    resetSong();
    const auto song = state.getChildWithName ("Song");
    for (int i = 0; i < numSongParts && i < song.getNumChildren(); ++i)
    {
        const auto part = song.getChild (i);
        songBars[(size_t) i] = juce::jlimit (0, 64, (int) part.getProperty ("bars", songBars[(size_t) i].load()));
        songVariation[(size_t) i] = (std::uint32_t) (juce::int64) part.getProperty ("variation", 0);
        songFill[(size_t) i] = juce::jlimit (-2, fgl::numFillStyles - 1, (int) part.getProperty ("fill", -1));
    }

    for (const auto* name : { "Grid", "Song", "Fill", "Template", "Link" })
        state.removeChild (state.getChildWithName (name), nullptr);
    state.removeProperty ("editorScale", nullptr);
    // APVTS retains the current value when a parameter child is absent. Old presets
    // must instead restore defaults for parameters added after that preset was saved.
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            if (! state.getChildWithProperty ("id", ranged->paramID).isValid())
            {
                juce::ValueTree child ("PARAM");
                child.setProperty ("id", ranged->paramID, nullptr);
                child.setProperty ("value", ranged->convertFrom0to1 (ranged->getDefaultValue()), nullptr);
                state.appendChild (child, nullptr);
            }
    apvts.replaceState (state);
    appliedKey = juce::roundToInt (raw (ParamIDs::key));   // the chords were saved in this key
}

void FaseGrooveLeadProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = stateTree();
    state.setProperty ("editorScale", editorScale, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FaseGrooveLeadProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    editorScale = (float) state.getProperty ("editorScale", 0.0f);
    if (editorScale > 0.0f)
        editorScale = juce::jlimit (0.6f, 2.0f, editorScale);
    applyStateTree (state);
}

juce::String FaseGrooveLeadProcessor::snapshot() const
{
    return stateTree().toXmlString();
}

void FaseGrooveLeadProcessor::restoreSnapshot (const juce::String& text)
{
    if (auto xml = juce::parseXML (text))
        applyStateTree (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
std::vector<fgl::ArrangementPart> FaseGrooveLeadProcessor::songArrangementParts() const
{
    std::vector<fgl::ArrangementPart> parts;
    for (int i = 0; i < numSongParts; ++i)
        parts.push_back ({ fgl::defaultArrangement()[(size_t) i].section, songBars[(size_t) i].load(),
                           songVariation[(size_t) i].load(), songFill[(size_t) i].load() });
    return parts;
}

void FaseGrooveLeadProcessor::setSongBars (int part, int bars)
{
    if (juce::isPositiveAndBelow (part, numSongParts))
        songBars[(size_t) part] = juce::jlimit (0, 64, bars);
}

void FaseGrooveLeadProcessor::setSongFill (int part, int fill)
{
    if (juce::isPositiveAndBelow (part, numSongParts))
        songFill[(size_t) part] = juce::jlimit (-2, fgl::numFillStyles - 1, fill);
}

void FaseGrooveLeadProcessor::regenerateSongPart (int part)
{
    if (juce::isPositiveAndBelow (part, numSongParts))
        songVariation[(size_t) part] = freshSeed();
}

void FaseGrooveLeadProcessor::resetSongPart (int part)
{
    if (juce::isPositiveAndBelow (part, numSongParts))
        songVariation[(size_t) part] = 0;
}

void FaseGrooveLeadProcessor::resetSong()
{
    for (int i = 0; i < numSongParts; ++i)
    {
        songBars[(size_t) i] = fgl::defaultArrangement()[(size_t) i].bars;
        songVariation[(size_t) i] = 0;
        songFill[(size_t) i] = -1;
    }
}

juce::String FaseGrooveLeadProcessor::sectionClipName (int partIndex)
{
    const auto section = (int) fgl::defaultArrangement()[(size_t) juce::jlimit (0, numSongParts - 1, partIndex)].section;
    return section == (int) fgl::Section::main ? juce::String ("Main groove") : juce::String (fgl::sectionName (section));
}

juce::File FaseGrooveLeadProcessor::writeSongSectionClip (int partIndex, ClipPart part) const
{
    const auto parts = songArrangementParts();
    if (! juce::isPositiveAndBelow (partIndex, (int) parts.size()) || parts[(size_t) partIndex].bars <= 0)
        return {};

    auto folder = clipFolder();
    if (! folder.createDirectory())
        folder = juce::File::getSpecialLocation (juce::File::tempDirectory);

    const auto& section = parts[(size_t) partIndex];
    std::vector<fgl::GrooveEngine::Marker> markers;
    const auto sectionEvents = fgl::GrooveEngine::renderArrangement (currentSettings(), { section }, &markers);
    const auto name = "Lead " + sectionClipName (partIndex) + " - "
                    + juce::String (fgl::GrooveEngine::basePattern (currentSettings()).name)
                    + " " + juce::String (juce::roundToInt (exportBpm())) + "bpm" + (part == ClipPart::tracks ? " (tracks)" : "");
    auto file = folder.getChildFile (juce::File::createLegalFileName (name.replace ("'", "")) + ".mid");
    return writeMidiFileSafely (midiFromEvents (sectionEvents, section.bars * 4.0, &markers, part, name), file) ? file : juce::File();
}

int FaseGrooveLeadProcessor::saveSongSections (const juce::File& folder, ClipPart part) const
{
    if (! folder.createDirectory())
        return 0;
    int written = 0;
    for (int i = 0; i < numSongParts; ++i)
        if (const auto file = writeSongSectionClip (i, part); file.existsAsFile())
        {
            const auto target = folder.getChildFile (juce::String (i + 1).paddedLeft ('0', 2) + " " + file.getFileName());
            juce::TemporaryFile temp (target);   // the one already there stays until the copy is whole
            if (file.copyFileTo (temp.getFile()) && temp.overwriteTargetFileWithTemporary())
                ++written;
        }
    return written;
}

//==============================================================================
juce::File FaseGrooveLeadProcessor::presetFolder()  { return userFolder().getChildFile ("Presets"); }
juce::File FaseGrooveLeadProcessor::grooveFolder()  { return userFolder().getChildFile ("Grooves"); }

juce::Array<juce::File> FaseGrooveLeadProcessor::presetFiles()
{
    auto files = presetFolder().findChildFiles (juce::File::findFiles, false, "*.fglpreset");
    files.sort();
    return files;
}

juce::Array<juce::File> FaseGrooveLeadProcessor::grooveFiles()
{
    auto files = grooveFolder().findChildFiles (juce::File::findFiles, false, "*.fglgroove");
    files.sort();
    return files;
}

bool FaseGrooveLeadProcessor::savePreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty() || ! presetFolder().createDirectory())
        return false;
    return presetFolder().getChildFile (clean + ".fglpreset").replaceWithText (snapshot());
}

bool FaseGrooveLeadProcessor::loadPreset (const juce::File& file)
{
    auto xml = juce::parseXML (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;
    applyStateTree (juce::ValueTree::fromXml (*xml));
    return true;
}

bool FaseGrooveLeadProcessor::saveGroove (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty() || ! grooveFolder().createDirectory())
        return false;

    // The grid as you hear it (in the A-D order), with its velocities.
    const auto p = displayedPattern();
    juce::XmlElement xml ("FaseGrooveLeadGroove");
    xml.setAttribute ("name", clean);
    for (int l = 0; l < fgl::numLanes; ++l)
    {
        juce::String levels, chances, rolls;
        for (int i = 0; i < steps(); ++i)
        {
            levels << juce::String::toHexString (stepVelocity (l, i)).paddedLeft ('0', 2);
            chances << juce::String::toHexString (stepChance (l, i) >= 100 ? 0 : stepChance (l, i)).paddedLeft ('0', 2);
            rolls << juce::String (stepRoll (l, i) > 1 ? stepRoll (l, i) : 0);
        }
        auto* lane = xml.createNewChildElement ("Lane");
        lane->setAttribute ("lane", fgl::laneName (l));
        lane->setAttribute ("steps", juce::String (p.lanes[(size_t) l]));
        lane->setAttribute ("velocity", levels);
        lane->setAttribute ("chance", chances);
        lane->setAttribute ("roll", rolls);
    }
    return xml.writeTo (grooveFolder().getChildFile (clean + ".fglgroove"));
}

bool FaseGrooveLeadProcessor::loadGroove (const juce::File& file)
{
    auto xml = juce::parseXML (file);
    if (xml == nullptr || ! xml->hasTagName ("FaseGrooveLeadGroove"))
        return false;

    int l = 0;
    for (auto* lane : xml->getChildWithTagNameIterator ("Lane"))
    {
        if (l >= fgl::numLanes)
            break;
        const auto steps = lane->getStringAttribute ("steps");
        const auto levels = lane->getStringAttribute ("velocity");
        const auto chances = lane->getStringAttribute ("chance");
        const auto rolls = lane->getStringAttribute ("roll");
        const int length = juce::jmax (1, steps.length());
        for (int i = 0; i < gridSteps; ++i)   // a two-bar groove fills the four bars twice
        {
            const int k = i % length;
            const char c = k < steps.length() ? (char) steps[k] : '.';
            userGrid[(size_t) l][(size_t) i] = fgl::isHit (c) ? c : '.';
            userVelocity[(size_t) l][(size_t) i] = juce::jlimit (0, 127, levels.substring (k * 2, k * 2 + 2).getHexValue32());
            userChance[(size_t) l][(size_t) i] = juce::jlimit (0, 100, chances.substring (k * 2, k * 2 + 2).getHexValue32());
            userRoll[(size_t) l][(size_t) i] = juce::jlimit (0, 4, rolls.substring (k, k + 1).getIntValue());
        }
        ++l;
    }
    for (int i = 0; i < fgl::numSegments; ++i)
        setSegmentAt (i, i);
    setParameterFromUI (ParamIDs::pattern, (float) fgl::userPatternIndex());
    return true;
}

namespace
{
    /** Zero-phase filtering (forwards, then backwards), twice: a steep band without delay. */
    void filterBand (std::vector<float>& y, double sr, double low, double high)
    {
        auto run = [&y] (juce::IIRCoefficients c)
        {
            for (int pass = 0; pass < 2; ++pass)
            {
                juce::IIRFilter f;
                f.setCoefficients (c);
                f.processSamples (y.data(), (int) y.size());
                std::reverse (y.begin(), y.end());
            }
        };
        for (int order = 0; order < 2; ++order)
        {
            if (low > 0.0)  run (juce::IIRCoefficients::makeHighPass (sr, low));
            if (high > 0.0) run (juce::IIRCoefficients::makeLowPass (sr, high));
        }
    }

    struct AudioHit { double time; float level, ring, body; };

    /** Where the sound of a band starts again (a fast rise in level), with how loud it is, how
        long it rings (open hats) and how much body it has 35-60 ms later (claps and snares do,
        a kick's click doesn't). */
    std::vector<AudioHit> findHits (const std::vector<float>& y, double sr, double minGap)
    {
        const int hop = juce::jmax (1, (int) (sr * 0.002));
        const int n = (int) y.size() / hop;
        std::vector<float> e ((size_t) n), db ((size_t) n), flux ((size_t) n, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            double sum = 0.0;
            for (int k = 0; k < hop; ++k)
                sum += (double) y[(size_t) (i * hop + k)] * y[(size_t) (i * hop + k)];
            e[(size_t) i] = (float) std::sqrt (sum / hop + 1.0e-12);
            db[(size_t) i] = 20.0f * std::log10 (e[(size_t) i] + 1.0e-7f);
        }
        for (int i = 4; i < n; ++i)
            flux[(size_t) i] = juce::jmax (0.0f, db[(size_t) i] - db[(size_t) (i - 4)]);

        const int gap = juce::jmax (1, (int) (minGap * sr / hop));
        const int w = (int) (0.025 * sr / hop), k100 = (int) (0.1 * sr / hop), a = (int) (0.035 * sr / hop), b = (int) (0.06 * sr / hop);
        std::vector<AudioHit> hits;
        int last = -gap;
        for (int i = 1; i + 1 < n; ++i)
        {
            if (flux[(size_t) i] < 6.0f || flux[(size_t) i] < flux[(size_t) (i - 1)] || flux[(size_t) i] < flux[(size_t) (i + 1)])
                continue;
            float peak = 0.0f;
            for (int j = i; j < juce::jmin (n, i + w); ++j)
                peak = juce::jmax (peak, e[(size_t) j]);
            float body = 0.0f;
            for (int j = i + a; j < juce::jmin (n, i + b); ++j)
                body += e[(size_t) j];
            body /= (float) juce::jmax (1, juce::jmin (n, i + b) - (i + a)) * (peak + 1.0e-9f);
            const AudioHit hit { (double) i * hop / sr, peak, e[(size_t) juce::jmin (n - 1, i + k100)] / (peak + 1.0e-9f), body };
            if (i - last < gap)
            {
                if (! hits.empty() && hit.level > hits.back().level)
                    hits.back() = hit, last = i;   // the stronger of two hits too close together
                continue;
            }
            hits.push_back (hit);
            last = i;
        }
        return hits;
    }
}

struct FaseGrooveLeadProcessor::LoopAnalysis
{
    double bpm = 120.0;
    int bars = 1;
    enum Band { low = 0, mid, high };
    struct Hit { double position; int band; float level; };   // position in 16ths from the loop's grid
    std::vector<Hit> hits;
};

std::unique_ptr<FaseGrooveLeadProcessor::LoopAnalysis> FaseGrooveLeadProcessor::analyseLoop (const juce::File& file) const
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return {};

    const double sr = reader->sampleRate;
    const int length = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (sr * 40.0));
    juce::AudioBuffer<float> audio ((int) juce::jlimit (1u, 2u, reader->numChannels), length);
    reader->read (&audio, 0, length, 0, true, audio.getNumChannels() > 1);
    std::vector<float> mono ((size_t) length);
    for (int i = 0; i < length; ++i)
        mono[(size_t) i] = audio.getNumChannels() > 1 ? 0.5f * (audio.getSample (0, i) + audio.getSample (1, i)) : audio.getSample (0, i);

    // The tempo: a loop is a whole number of bars. The tempo in use first, else the one nearest
    // 124 BPM that makes it 1, 2, 4 or 8 bars.
    const double seconds = length / sr;
    double bpm = exportBpm();
    double bars = seconds / (4.0 * 60.0 / bpm);
    if (std::abs (bars - std::round (bars)) > 0.04 || std::round (bars) < 1.0)
    {
        double best = 1.0e9;
        for (int b : { 1, 2, 4, 8 })
            if (const double t = b * 4.0 * 60.0 / seconds; t >= 70.0 && t <= 180.0 && std::abs (t - 124.0) < best)
            {
                best = std::abs (t - 124.0);
                bpm = t;
            }
        bars = seconds / (4.0 * 60.0 / bpm);
    }
    const int wholeBars = juce::jmax (1, (int) std::round (bars));
    const double T = seconds / (wholeBars * 16.0);   // seconds per 16th

    auto bandOf = [&] (double low, double high) { auto y = mono; filterBand (y, sr, low, high); return y; };
    auto kicks = findHits (bandOf (0.0, 110.0), sr, 0.1);
    auto mids = findHits (bandOf (1000.0, 4000.0), sr, 0.045);
    auto highs = findHits (bandOf (7500.0, 0.0), sr, 0.045);
    if (kicks.empty() && mids.empty() && highs.empty())
        return {};

    // Where the loop sits against the grid: the strong kicks (or else the hats).
    double offset = 0.0;
    {
        const auto& ref = kicks.empty() ? highs : kicks;
        float top = 0.0f;
        for (const auto& h : ref) top = juce::jmax (top, h.level);
        std::complex<double> sum;
        for (const auto& h : ref)
            if (h.level > 0.5f * top)
                sum += std::polar (1.0, juce::MathConstants<double>::twoPi * std::fmod (h.time / T, 1.0));
        if (std::abs (sum) > 0.0)
            offset = std::arg (sum) / juce::MathConstants<double>::twoPi * T;
    }

    auto result = std::make_unique<LoopAnalysis>();
    result->bpm = bpm;
    result->bars = wholeBars;

    auto place = [&] (const std::vector<AudioHit>& hits, int band, float minBody)
    {
        float top = 0.0f;
        for (const auto& h : hits)
            if (h.body >= minBody) top = juce::jmax (top, h.level);
        for (const auto& h : hits)
        {
            if (h.body < minBody || top <= 0.0f)
                continue;
            const double pos = (h.time - offset) / T;
            const float r = h.level / top;
            if (r >= 0.12f && pos > -0.5)
                result->hits.push_back ({ pos, band, r });
        }
    };
    place (kicks, LoopAnalysis::low, 0.0f);
    place (mids, LoopAnalysis::mid, 0.28f);
    place (highs, LoopAnalysis::high, 0.0f);
    return result;
}

namespace
{
    /** A groove template from hits on a 16th grid (position in 16ths, level 0..1): the median
        timing and the average level of each 16th of the bar. */
    bool templateFrom (const std::vector<std::pair<double, float>>& hits, std::array<float, 16>& timing, std::array<float, 16>& level)
    {
        std::array<std::vector<double>, 16> offsets;
        std::array<std::vector<double>, 16> levels;
        for (const auto& [position, l] : hits)
        {
            const auto step = (long long) std::llround (position);
            const int inBar = (int) (((step % 16) + 16) % 16);
            offsets[(size_t) inBar].push_back (position - (double) step);
            levels[(size_t) inBar].push_back (l);
        }
        auto median = [] (std::vector<double> v) { std::sort (v.begin(), v.end()); return v[v.size() / 2]; };
        auto mean = [] (const std::vector<double>& v) { double sum = 0.0; for (double x : v) sum += x; return sum / (double) v.size(); };

        // The 16ths the loop doesn't play take the timing of their kind (even / odd).
        std::vector<double> even, odd;
        int filled = 0;
        double top = 0.0;
        for (int i = 0; i < 16; ++i)
            if (! offsets[(size_t) i].empty())
            {
                ++filled;
                (i % 2 == 0 ? even : odd).push_back (median (offsets[(size_t) i]));
                top = std::max (top, mean (levels[(size_t) i]));
            }
        if (filled < 3 || top <= 0.0)
            return false;
        const double evenTiming = even.empty() ? 0.0 : median (even);
        const double oddTiming = odd.empty() ? 0.0 : median (odd);
        for (int i = 0; i < 16; ++i)
        {
            // Relative to the downbeats: a loop that is late everywhere is just late.
            const double t = offsets[(size_t) i].empty() ? (i % 2 == 0 ? evenTiming : oddTiming) : median (offsets[(size_t) i]);
            timing[(size_t) i] = (float) juce::jlimit (-0.5, 0.5, t - evenTiming);
            level[(size_t) i] = levels[(size_t) i].empty() ? 1.0f : (float) juce::jlimit (0.4, 1.2, 0.45 + 0.75 * mean (levels[(size_t) i]) / top);
        }
        return true;
    }
}

void FaseGrooveLeadProcessor::setTemplate (const juce::String& name, const std::array<float, 16>& timing, const std::array<float, 16>& level)
{
    templateName = name;
    for (int i = 0; i < 16; ++i)
    {
        templateTiming[(size_t) i] = timing[(size_t) i];
        templateLevel[(size_t) i] = level[(size_t) i];
    }
}

void FaseGrooveLeadProcessor::clearGrooveTemplate()
{
    templateActive = false;
    std::array<float, 16> timing {}, level {};
    level.fill (1.0f);
    setTemplate ({}, timing, level);
}

juce::String FaseGrooveLeadProcessor::grooveTemplateFromAudio (const juce::File& file)
{
    const auto loop = analyseLoop (file);
    if (loop == nullptr)
        return {};

    // The hands make the groove: hats, claps, snares, a strumming hand (the kick is the grid).
    std::vector<std::pair<double, float>> hits;
    for (const auto& h : loop->hits)
        if (h.band != LoopAnalysis::low && h.level > 0.2f)
            hits.push_back ({ h.position, h.level });
    std::array<float, 16> timing {}, level {};
    if (! templateFrom (hits, timing, level))
        return {};
    setTemplate (file.getFileNameWithoutExtension(), timing, level);
    templateActive = true;
    return file.getFileNameWithoutExtension() + " (" + juce::String (loop->bpm, 1) + " BPM)";
}

juce::String FaseGrooveLeadProcessor::grooveTemplateFromMidi (const juce::File& file)
{
    juce::FileInputStream in (file);
    juce::MidiFile midi;
    if (! in.openedOk() || ! midi.readFrom (in) || midi.getTimeFormat() <= 0)
        return {};
    const double ticks = midi.getTimeFormat();

    std::vector<std::pair<double, float>> hits;
    for (int t = 0; t < midi.getNumTracks(); ++t)
        for (const auto* e : *midi.getTrack (t))
            if (e->message.isNoteOn() && ! (e->message.getChannel() == 10 && e->message.getNoteNumber() <= 36))   // (not a kick)
                hits.push_back ({ e->message.getTimeStamp() / ticks / fgl::stepLength, e->message.getFloatVelocity() });
    std::array<float, 16> timing {}, level {};
    if (! templateFrom (hits, timing, level))
        return {};
    setTemplate (file.getFileNameWithoutExtension(), timing, level);
    templateActive = true;
    return file.getFileNameWithoutExtension();
}

bool FaseGrooveLeadProcessor::importMidiGroove (const juce::File& file)
{
    juce::FileInputStream in (file);
    juce::MidiFile midi;
    if (! in.openedOk() || ! midi.readFrom (in))
        return false;
    const int ticks = midi.getTimeFormat();
    if (ticks <= 0)
        return false;

    // Every note, with its length (drums on channel 10 left out).
    struct Note { double beat, length; int note, velocity; };
    std::vector<Note> notes;
    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence track (*midi.getTrack (t));
        track.updateMatchedPairs();
        for (int i = 0; i < track.getNumEvents(); ++i)
        {
            const auto* e = track.getEventPointer (i);
            if (! e->message.isNoteOn() || e->message.getChannel() == 10)
                continue;
            const double on = e->message.getTimeStamp() / ticks;
            const double off = e->noteOffObject != nullptr ? e->noteOffObject->message.getTimeStamp() / ticks : on + 0.25;
            notes.push_back ({ on, juce::jmax (0.01, off - on), e->message.getNoteNumber(), e->message.getVelocity() });
        }
    }
    if (notes.empty())
        return false;

    double first = notes.front().beat, last = first;
    for (const auto& n : notes)
    {
        first = juce::jmin (first, n.beat);
        last = juce::jmax (last, n.beat);
    }
    const double start = std::floor (first / 4.0 + 1.0e-6) * 4.0;

    // The notes struck together (on the same 16th) are one hit: a chord, an octave, one note.
    std::map<int, std::vector<Note>> byStep;
    for (const auto& n : notes)
        byStep[(int) std::lround ((n.beat - start) / fgl::stepLength)].push_back (n);

    fgl::LeadPattern p;
    for (auto& line : p.lanes)
        line.assign ((size_t) gridSteps, '.');
    std::array<std::array<int, gridSteps>, fgl::numLanes> levels {};
    for (const auto& [step, hit] : byStep)
    {
        if (! juce::isPositiveAndBelow (step, steps()))
            continue;
        int velocity = 0, low = 127, high = 0;
        double length = 0.0;
        for (const auto& n : hit)
        {
            velocity = juce::jmax (velocity, n.velocity);
            low = juce::jmin (low, n.note);
            high = juce::jmax (high, n.note);
            length = juce::jmax (length, n.length / fgl::stepLength);   // in 16ths
        }
        const int count = (int) hit.size();
        fgl::Lane lane;
        if (count >= 4)
            lane = length < 0.6 && velocity < 70 ? fgl::Lane::scratch
                 : length >= 6.0 ? fgl::Lane::swell
                 : step % 2 == 1 ? fgl::Lane::answer
                 : length < 1.0 ? fgl::Lane::stab : fgl::Lane::hook;
        else if (count == 3)
            lane = length < 1.0 ? fgl::Lane::stab : fgl::Lane::hook;
        else if (count == 2)
            lane = high - low == 12 ? fgl::Lane::octave : high - low == 7 || high - low == 19 ? fgl::Lane::power : fgl::Lane::stab;
        else
            lane = low < 52 ? fgl::Lane::root : length < 0.6 ? fgl::Lane::chank : fgl::Lane::lick;
        const auto l = (size_t) lane;
        if (velocity > levels[l][(size_t) step])
        {
            levels[l][(size_t) step] = velocity;
            p.lanes[l][(size_t) step] = velocity >= 110 ? 'X' : velocity <= 50 ? 'g' : 'x';
        }
    }

    // A part shorter than the groove plays again (whole bars).
    const int loopSteps = juce::jlimit (16, steps(), (int) std::ceil ((last - start) / 4.0 + 1.0e-6) * 16);
    for (size_t l = 0; l < (size_t) fgl::numLanes; ++l)
        for (int i = loopSteps; i < gridSteps; ++i)
        {
            p.lanes[l][(size_t) i] = p.lanes[l][(size_t) (i % loopSteps)];
            levels[l][(size_t) i] = levels[l][(size_t) (i % loopSteps)];
        }

    setUserPattern (p);
    for (int l = 0; l < fgl::numLanes; ++l)
        for (int i = 0; i < gridSteps; ++i)
            userVelocity[(size_t) l][(size_t) i] = levels[(size_t) l][(size_t) i];
    for (int i = 0; i < fgl::numSegments; ++i)
        setSegmentAt (i, i);

    // The chords of the first bars (up to eight): the chord whose tones weigh the most (long, loud
    // notes count more, the lowest note is likely the root). A bar without notes keeps its chord.
    const int bars = juce::jlimit (1, fgl::maxChords, (int) std::ceil ((last - start) / 4.0 + 1.0e-6));
    setChordCount (bars);
    for (int bar = 0; bar < bars; ++bar)
    {
        std::array<double, 12> weight {};
        int lowest = 128;
        for (const auto& n : notes)
            if (n.beat >= start + bar * 4.0 - 0.05 && n.beat < start + bar * 4.0 + 3.95)
            {
                weight[(size_t) (n.note % 12)] += juce::jmin (n.length, 4.0) * n.velocity;
                lowest = juce::jmin (lowest, n.note);
            }
        if (lowest > 127)
            continue;
        double best = -1.0e9;
        fgl::Chord chord;
        for (int root = 0; root < 12; ++root)
            for (int type = 0; type < fgl::numChordTypes; ++type)
            {
                const auto mask = fgl::chordTypeMask (type);
                double score = root == lowest % 12 ? 40.0 : 0.0;
                int size = 0;
                for (int pc = 0; pc < 12; ++pc)
                {
                    const bool isTone = ((mask >> (unsigned) ((pc - root + 12) % 12)) & 1u) != 0;
                    size += isTone ? 1 : 0;
                    score += isTone ? weight[(size_t) pc] : -weight[(size_t) pc];
                    if (isTone && weight[(size_t) pc] <= 0.0)
                        score -= 25.0;   // a chord tone the part never plays
                }
                score -= size;   // the simpler chord when two fit as well
                if (score > best)
                {
                    best = score;
                    chord = { root, type };
                }
            }
        setChord (bar, chord);
    }
    return true;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FaseGrooveLeadProcessor();
}
