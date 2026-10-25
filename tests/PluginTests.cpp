// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Plug-in level tests (they need JUCE, unlike EngineTests): project state, grid editing,
// chords, styles, exported MIDI and audio files, DAW transport, the MIDI chord input and the
// guide lead.

#include <array>
#include "../Source/PluginProcessor.h"
#include "Localisation.h"
#include "../Source/PluginEditor.h"
#include "Updater.h"
#include "../Source/GuideLead.h"

#include <cstdio>
#include "FamilyAudioEvidence.h"
#include "TestSupport.h"
#include "TestUpdater.h"
#include "TestLink.h"
#include "TestPlayHead.h"
#include "FamilySoundProfiles.h"
#include <set>

namespace
{
    using FaseGrooveTests::PlayHead;

    float value (FaseGrooveLeadProcessor& p, const juce::String& id)
    {
        return p.apvts.getRawParameterValue (id)->load();
    }

    int countNoteOns (const juce::MidiFile& file, int channel = -1)
    {
        int count = 0;
        for (int t = 0; t < file.getNumTracks(); ++t)
            for (const auto* e : *file.getTrack (t))
                if (e->message.isNoteOn() && (channel < 0 || e->message.getChannel() == channel))
                    ++count;
        return count;
    }

    bool isChordTone (const fgl::Chord& chord, int note)
    {
        const auto t = fgl::tonesOf (chord);
        return (t.mask >> (unsigned) (((note - t.root) % 12 + 12) % 12)) & 1u;
    }

    /** Every note of a file clean, for any instrument: each note-on closed by its note-off on
        the same track, later than it, no pitch struck again while still sounding, nothing left open. */
    bool notesAreClean (const juce::MidiFile& file)
    {
        for (int t = 0; t < file.getNumTracks(); ++t)
        {
            std::array<double, 128> openAt;
            openAt.fill (-1.0);
            for (const auto* e : *file.getTrack (t))
            {
                const auto& m = e->message;
                if (m.isNoteOn())
                {
                    if (openAt[(size_t) m.getNoteNumber()] >= 0.0)
                    {
                        std::printf ("  note %d struck again at %.0f (open since %.0f)\n", m.getNoteNumber(), m.getTimeStamp(), openAt[(size_t) m.getNoteNumber()]);
                        return false;   // struck again while sounding
                    }
                    openAt[(size_t) m.getNoteNumber()] = m.getTimeStamp();
                }
                else if (m.isNoteOff())
                {
                    const double start = openAt[(size_t) m.getNoteNumber()];
                    if (start < 0.0 || m.getTimeStamp() <= start)
                    {
                        std::printf ("  note %d off at %.0f (open since %.0f)\n", m.getNoteNumber(), m.getTimeStamp(), start);
                        for (const auto* x : *file.getTrack (t))
                            if (x->message.isNoteOnOrOff() && x->message.getNoteNumber() == m.getNoteNumber() && std::abs (x->message.getTimeStamp() - m.getTimeStamp()) < 400)
                                std::printf ("    %s %.0f\n", x->message.isNoteOn() ? "on " : "off", x->message.getTimeStamp());
                        return false;
                    }   // an off with nothing open, or a zero-length note
                    openAt[(size_t) m.getNoteNumber()] = -1.0;
                }
            }
            for (double o : openAt)
                if (o >= 0.0)
                    return false;   // left hanging at the end
        }
        return true;
    }

    //==============================================================================
    /** The family's compatibility checks on every preset groove: clean MIDI, one track and the
        tracks per lane with the same notes, the same seed the same clip, the dice another one. */
    void testGrooveCompatibility()
    {
        FaseGrooveLeadProcessor p;
        p.setParameterFromUI (ParamIDs::tempoSync, 0.0f);
        for (size_t i = 0; i < fgl::leadPatterns().size(); ++i)
        {
            p.selectGroove ((int) i);
            const auto one = p.createMidiClip (4, FaseGrooveLeadProcessor::ClipPart::all);
            const auto tracks = p.createMidiClip (4, FaseGrooveLeadProcessor::ClipPart::tracks);
            CHECK (notesAreClean (one));
            CHECK (notesAreClean (tracks));
            CHECK (countNoteOns (one) > 0);
            CHECK (countNoteOns (tracks) >= countNoteOns (one));   // (two lanes on one note at once: one note on a single track)
            // The same settings: the same clip.
            juce::MemoryOutputStream a, b;
            one.writeTo (a);
            p.createMidiClip (4, FaseGrooveLeadProcessor::ClipPart::all).writeTo (b);
            CHECK (a.getMemoryBlock() == b.getMemoryBlock());
        }
        // The whole song too, with the clip's end on a section boundary.
        const auto song = p.createMidiClip (FaseGrooveLeadProcessor::songArrangement, FaseGrooveLeadProcessor::ClipPart::all);
        CHECK (notesAreClean (song));
        CHECK (notesAreClean (p.createMidiClip (FaseGrooveLeadProcessor::songArrangement, FaseGrooveLeadProcessor::ClipPart::tracks)));
    }

    //==============================================================================
    void testOldPresetDefaults()
    {
        FaseGrooveLeadProcessor p, fresh;
        p.setParameterFromUI (ParamIDs::swing, 1.4f);
        p.setParameterFromUI (ParamIDs::pattern, 2.0f);
        juce::XmlElement old ("FaseGrooveLead");
        auto* param = old.createNewChildElement ("PARAM");
        param->setAttribute ("id", "tempo");
        param->setAttribute ("value", 132.0);
        juce::MemoryBlock data;
        juce::AudioProcessor::copyXmlToBinary (old, data);
        p.setStateInformation (data.getData(), (int) data.getSize());
        CHECK (value (p, ParamIDs::swing) == value (fresh, ParamIDs::swing));
        CHECK (value (p, ParamIDs::pattern) == value (fresh, ParamIDs::pattern));
        CHECK (std::abs (value (p, ParamIDs::tempo) - 132.0f) < 0.01f);
    }

    void testStateRoundTrip()
    {
        FaseGrooveLeadProcessor a;
        a.setParameterFromUI (ParamIDs::swing, 0.4f);
        a.setParameterFromUI (ParamIDs::kit, 2.0f);
        a.setParameterFromUI (ParamIDs::octave (3), 1.0f);
        a.setParameterFromUI (ParamIDs::globalOctave, -1.0f);
        a.setChord (2, { 4, (int) fgl::ChordType::dom9 });
        a.editStep (1, 3, 'f');   // turns the groove into the user pattern
        a.editorScale = 1.3f;

        juce::MemoryBlock data;
        a.getStateInformation (data);

        FaseGrooveLeadProcessor b;
        b.setStateInformation (data.getData(), (int) data.getSize());
        CHECK (std::abs (value (b, ParamIDs::swing) - 0.4f) < 1.0e-4f);
        CHECK (juce::roundToInt (value (b, ParamIDs::kit)) == 2);
        CHECK (b.laneOctave (3) == 1);
        CHECK (b.currentSettings().globalOctave == -1);
        CHECK (b.chordAt (2) == (fgl::Chord { 4, (int) fgl::ChordType::dom9 }));
        CHECK (juce::roundToInt (value (b, ParamIDs::pattern)) == fgl::userPatternIndex());
        CHECK (b.stepSymbol (1, 3) == 'f');
        CHECK (std::abs (b.editorScale - 1.3f) < 1.0e-4f);
        for (int l = 0; l < fgl::numLanes; ++l)
            for (int s = 0; s < 32; ++s)
                CHECK (a.stepSymbol (l, s) == b.stepSymbol (l, s));

        // Garbage doesn't crash or change anything.
        const char junk[] = "not a state";
        b.setStateInformation (junk, (int) sizeof (junk));
        CHECK (b.stepSymbol (1, 3) == 'f');

        // Undo snapshots restore the grid and the chords.
        const auto snap = b.snapshot();
        b.clearGrid();
        b.setChord (2, { 0, 0 });
        b.restoreSnapshot (snap);
        CHECK (b.stepSymbol (1, 3) == 'f');
        CHECK (b.chordAt (2).root == 4);
    }

    void testGridEditing()
    {
        FaseGrooveLeadProcessor p;
        p.selectGroove (0);
        const char before = p.stepSymbol ((int) fgl::Lane::stab, 2);
        CHECK (fgl::isHit (before));   // Offbeat Chords: a stab on the offbeat
        p.editStep ((int) fgl::Lane::lick, 5, 'x');
        CHECK (juce::roundToInt (value (p, ParamIDs::pattern)) == fgl::userPatternIndex());
        CHECK (p.stepSymbol ((int) fgl::Lane::stab, 2) == before);   // the groove is kept
        CHECK (p.stepSymbol ((int) fgl::Lane::lick, 5) == 'x');
        p.clearLane ((int) fgl::Lane::stab);
        for (int s = 0; s < p.steps(); ++s)
            CHECK (p.stepSymbol ((int) fgl::Lane::stab, s) == '.');
        p.randomizeLane ((int) fgl::Lane::arp);
        int arps = 0;
        for (int s = 0; s < p.steps(); ++s)
            arps += fgl::isHit (p.stepSymbol ((int) fgl::Lane::arp, s)) ? 1 : 0;
        CHECK (arps > 0);

        // A locked part stays when the dice roll.
        p.setSegmentLocked (0, true);
        std::array<char, 8> locked {};
        for (int s = 0; s < 8; ++s)
            locked[(size_t) s] = p.stepSymbol ((int) fgl::Lane::lick, s);
        p.rollNewBeat();
        for (int s = 0; s < 8; ++s)
            CHECK (p.stepSymbol ((int) fgl::Lane::lick, s) == locked[(size_t) s]);

        // Velocities, chance and roll.
        p.setStepVelocity ((int) fgl::Lane::lick, 5, 90);
        CHECK (p.stepVelocity ((int) fgl::Lane::lick, 5) == 90);
        p.setStepRoll ((int) fgl::Lane::lick, 5, 3);
        CHECK (p.stepRoll ((int) fgl::Lane::lick, 5) == 3);
        p.setStepChance ((int) fgl::Lane::lick, 5, 50);
        CHECK (p.stepChance ((int) fgl::Lane::lick, 5) == 50);
        // A 2-bar edit is repeated in the stored second half, so 4 bars play the same groove.
        p.setParameterFromUI (ParamIDs::partLength, 1.0f);
        CHECK (p.steps() == 64);
        CHECK (p.stepSymbol ((int) fgl::Lane::lick, 37) == p.stepSymbol ((int) fgl::Lane::lick, 5));
        CHECK (p.stepVelocity ((int) fgl::Lane::lick, 37) == 90);
        CHECK (p.stepRoll ((int) fgl::Lane::lick, 37) == 3);
        CHECK (p.stepChance ((int) fgl::Lane::lick, 37) == 50);
        p.setParameterFromUI (ParamIDs::partLength, 0.0f);

        // Chance "Always" is menu id 200; that used to be read as a roll and clear the roll.
        const auto always = stepMenuChoice (200);
        CHECK (always.kind == StepMenuChoice::Kind::chance && always.value == 100);
        const auto roll = stepMenuChoice (203);
        CHECK (roll.kind == StepMenuChoice::Kind::roll && roll.value == 3);
        CHECK (chordMenuCopySlot (303) == 3);
        CHECK (chordMenuCopySlot (307) == 7);
        CHECK (chordMenuCopySlot (308) == -1);
        p.clearGrid();
        CHECK (p.stepSymbol ((int) fgl::Lane::lick, 5) == '.');
    }

    void testChordsAndStyles()
    {
        FaseGrooveLeadProcessor p;
        // A progression in the key.
        p.setParameterFromUI (ParamIDs::key, 0.0f);
        p.applyProgression (0);   // i7 - iv7
        CHECK (p.chordAt (0) == (fgl::Chord { 0, (int) fgl::ChordType::m7 }));
        CHECK (p.chordAt (1) == (fgl::Chord { 5, (int) fgl::ChordType::m7 }));
        // A new key moves the chords (the shortest way).
        p.setKey (2);
        CHECK (p.chordAt (0).root == 2 && p.chordAt (1).root == 7);
        CHECK (juce::roundToInt (value (p, ParamIDs::key)) == 2);
        // The relative minor keeps the chords (D major to B minor: the same notes)...
        p.setParameterFromUI (ParamIDs::scale, 0.0f);
        p.setKeyAndScale (11, 2);
        CHECK (p.chordAt (0).root == 2 && p.chordAt (1).root == 7);
        CHECK (juce::roundToInt (value (p, ParamIDs::key)) == 11 && juce::roundToInt (value (p, ParamIDs::scale)) == 2);
        // ...another key moves them.
        p.setKeyAndScale (9, 2);
        CHECK (p.chordAt (0).root == 0 && p.chordAt (1).root == 5);
        p.setKey (2);
        p.setParameterFromUI (ParamIDs::scale, 0.0f);
        // One chord by hand.
        p.setChord (3, { 11, (int) fgl::ChordType::sus4 });
        CHECK (fgl::chordName (p.chordAt (3)) == "B7sus4");
        // The dice: another progression.
        const int before = juce::roundToInt (value (p, ParamIDs::progression));
        bool changed = false;   // (the dice may land on the same one: a few throws)
        for (int t = 0; t < 8 && ! changed; ++t)
        {
            p.randomizeChords();
            changed = juce::roundToInt (value (p, ParamIDs::progression)) != before;
        }
        CHECK (changed);

        // A style: a groove of the genre, its swing and its chords.
        const auto names = FaseGrooveLeadProcessor::styleNames();
        CHECK (names.size() == 13+fgfamily::numGenres);
        for (int i = 0; i < 13; ++i)
        {
            p.applyStyle (i);
            const int pattern = juce::roundToInt (value (p, ParamIDs::pattern));
            CHECK (juce::isPositiveAndBelow (pattern, (int) fgl::leadPatterns().size()));
            CHECK (fgl::leadPatterns()[(size_t) pattern].category == fgl::grooveCategories()[(size_t) i]);
            CHECK (std::abs (value (p, ParamIDs::swing) - fgl::genreSwing (i)) < 1.0e-3f);
            CHECK (juce::roundToInt (value (p, ParamIDs::progression)) == fgl::genreProgression (i));
        }

        // Total random always ends on something that plays.
        for (int i = 0; i < 64; ++i)
        {
            p.totalRandom();
            CHECK (juce::isPositiveAndBelow (p.currentSettings().randomGenre, fgfamily::numGenres));
            CHECK (countNoteOns (p.createMidiClip (4, FaseGrooveLeadProcessor::ClipPart::all)) > 0);
            CHECK (value (p, ParamIDs::volume) > -12.0f);   // random never leaves the lead inaudible
        }
        p.resetToDefaults();
        CHECK (juce::roundToInt (value (p, ParamIDs::pattern)) == 0);
    }

    void testMidiExport()
    {
        FaseGrooveLeadProcessor p;
        p.setParameterFromUI (ParamIDs::tempoSync, 0.0f);
        p.setParameterFromUI (ParamIDs::tempo, 122.0f);
        p.setParameterFromUI (ParamIDs::fills, 0.0f);
        p.setParameterFromUI (ParamIDs::variation, 0.0f);
        p.setParameterFromUI (ParamIDs::humanize, 0.0f);
        p.setParameterFromUI (ParamIDs::pushChanges, 0.0f);   // (the last offbeat would play the next bar's chord)
        p.selectGroove (0);   // Offbeat Chords: stabs on every offbeat

        const auto clip = p.createMidiClip (4, FaseGrooveLeadProcessor::ClipPart::all);
        CHECK (clip.getNumTracks() == 1);
        CHECK (countNoteOns (clip) >= 16);   // a melodic voice, at least one attack per beat
        CHECK (countNoteOns (clip, 1) == countNoteOns (clip));   // channel 1 by default

        // The notes are the chords of the slots, bar by bar.
        for (const auto* e : *clip.getTrack (0))
            if (e->message.isNoteOn())
            {
                const int bar = (int) (e->message.getTimeStamp() / (960.0 * 4.0));
                CHECK (isChordTone (p.chordAt (bar % 4), e->message.getNoteNumber()));
            }

        // The tempo of the file.
        juce::MidiMessageSequence tempos;
        clip.findAllTempoEvents (tempos);
        CHECK (tempos.getNumEvents() == 1);
        if (tempos.getNumEvents() == 1)
            CHECK (std::abs (60.0 / tempos.getEventPointer (0)->message.getTempoSecondsPerQuarterNote() - 122.0) < 0.01);

        // One track per lane.
        const auto tracks = p.createMidiClip (4, FaseGrooveLeadProcessor::ClipPart::tracks);
        CHECK (tracks.getNumTracks() >= 3);
        CHECK (countNoteOns (tracks) >= countNoteOns (clip));

        // Another channel; a muted lane is left out.
        p.setParameterFromUI (ParamIDs::midiChannel, 5.0f);
        CHECK (countNoteOns (p.createMidiClip (4, FaseGrooveLeadProcessor::ClipPart::all), 5) > 0);
        p.setLaneMuted ((int) fgl::Lane::stab, true);
        p.setLaneMuted ((int) fgl::Lane::scratch, true);
        CHECK (countNoteOns (p.createMidiClip (4, FaseGrooveLeadProcessor::ClipPart::all)) == 0);
        p.setLaneMuted ((int) fgl::Lane::stab, false);
        p.setLaneMuted ((int) fgl::Lane::scratch, false);

        // The whole song: 60 bars with markers.
        const auto song = p.createMidiClip (FaseGrooveLeadProcessor::songArrangement, FaseGrooveLeadProcessor::ClipPart::all);
        std::set<juce::String> markers;
        for (const auto* e : *song.getTrack (0))
            if (e->message.isTextMetaEvent() && e->message.getMetaEventType() == 6)
                markers.insert (e->message.getTextFromTextMetaEvent());
        CHECK (markers.count ("Intro") == 1 && markers.count ("Drop") == 1 && markers.count ("Outro") == 1);
        CHECK (std::abs (song.getLastTimestamp() - 60.0 * 4.0 * 960.0) < 1.0);

        // The file on disk, with a readable name.
        const auto file = p.writeMidiClip (0, FaseGrooveLeadProcessor::ClipPart::all);
        CHECK (file.existsAsFile());
        CHECK (file.getFileName().startsWith ("Lead "));
        CHECK (file.getFileName().contains ("122bpm"));
        file.deleteFile();
    }

    void testAudioExport()
    {
        FaseGrooveLeadProcessor p;
        p.setParameterFromUI (ParamIDs::tempoSync, 0.0f);
        p.selectGroove (1);
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("fgl-audio-test");
        folder.deleteRecursively();
        folder.createDirectory();

        const auto files = p.writeAudio (FaseGrooveLeadProcessor::AudioExport::clip, 2, folder.getChildFile ("clip.wav"));
        CHECK (files.size() == 1);
        if (files.size() == 1)
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (files[0]));
            CHECK (reader != nullptr);
            if (reader != nullptr)
            {
                juce::AudioBuffer<float> audio (2, (int) reader->lengthInSamples);
                reader->read (&audio, 0, audio.getNumSamples(), 0, true, true);
                const float peak = audio.getMagnitude (0, audio.getNumSamples());
                CHECK (peak > 0.02f && peak < 4.0f);   // it sounds, and nothing explodes
                bool finite = true;
                for (int i = 0; i < audio.getNumSamples(); ++i)
                    finite = finite && std::isfinite (audio.getSample (0, i));
                CHECK (finite);
            }
        }
        const auto stems = p.writeAudio (FaseGrooveLeadProcessor::AudioExport::stems, 1, folder.getChildFile ("stems"));
        CHECK (stems.size() >= 2);
        folder.deleteRecursively();
    }

    void testChannelChangeWhilePlaying()
    {
        FaseGrooveLeadProcessor p;
        p.setParameterFromUI (ParamIDs::sound, 0.0f);
        p.setParameterFromUI (ParamIDs::humanize, 0.0f);
        p.setParameterFromUI (ParamIDs::fills, 0.0f);
        p.setParameterFromUI (ParamIDs::variation, 0.0f);
        p.prepareToPlay (44100.0, 256);
        p.previewToggle();
        juce::AudioBuffer<float> audio (2, 256);
        juce::MidiBuffer midi;
        int open[16][128] {};
        bool changed = false;
        int ons = 0;
        auto track = [&]
        {
            for (const auto meta : midi)
            {
                const auto m = meta.getMessage();
                if (m.isNoteOn()) { ++open[m.getChannel()-1][m.getNoteNumber()]; ++ons; }
                if (m.isNoteOff()) { --open[m.getChannel()-1][m.getNoteNumber()]; }
            }
        };
        for (int block = 0; block < 600; ++block)
        {
            midi.clear();
            p.processBlock (audio, midi);
            track();
            if (! changed && ons > 0)
            {
                p.setParameterFromUI (ParamIDs::midiChannel, 5.0f);
                changed = true;
            }
        }
        p.previewStop();
        midi.clear();
        p.processBlock (audio, midi);
        track();
        CHECK (changed && ons > 0);
        for (auto& channel : open)
            for (int count : channel) CHECK (count == 0);
    }

    void testAuditionAcrossBlocks()
    {
        auto run = [] (int blockSize)
        {
            FaseGrooveLeadProcessor p;
            p.setParameterFromUI (ParamIDs::sound, 0.0f);
            p.setParameterFromUI (ParamIDs::attackDelay, 40.0f);
            p.prepareToPlay (44100.0, blockSize);
            p.audition ((int) fgl::Lane::hook, 100);
            juce::AudioBuffer<float> audio (2, blockSize);
            juce::MidiBuffer midi;
            std::vector<std::pair<int, int>> times;
            for (int start = 0; start < 44100; start += blockSize)
            {
                midi.clear();
                p.processBlock (audio, midi);
                for (const auto meta : midi)
                {
                    const auto m = meta.getMessage();
                    if (m.isNoteOnOrOff())
                        times.push_back ({ start + meta.samplePosition, m.isNoteOn() ? m.getNoteNumber() : -m.getNoteNumber() });
                }
            }
            return times;
        };
        const auto small = run (64), large = run (2048);
        CHECK (small == large);
        CHECK (small.size() == 2);
        if (small.size() > 1) CHECK (small[1].first - small[0].first == 22050);
    }

    void testOverviewShowsWhatPlays()
    {
        // Every note processBlock plays over 16 bars (variations, fills, crashes, humanize) is in what
        // the overview shows for those bars (playedNotes), for the presets and random grooves.
        for (int trial = 0; trial < 6; ++trial)
        {
            FaseGrooveLeadProcessor p;
            PlayHead head;
            p.setPlayHead (&head);
            p.setRateAndBufferSizeDetails (44100.0, 256);
            p.prepareToPlay (44100.0, 256);
            if (trial > 0)
                p.rollNewBeat();
            if (trial == 5)
                p.setParameterFromUI (ParamIDs::section, 3.0f);
            const double sr = 44100.0, beatsPerBlock = 256.0 / (sr * 60.0 / head.bpm), length = 64.0;
            std::set<std::pair<long long, int>> played, shown;
            const int len = p.steps();
            for (long long first = 0; first * fgl::stepLength < length; first += len)
                for (const auto& e : p.playedNotes (first, len))
                    shown.insert ({ FaseGrooveLeadProcessor::stepOf (e.ppq), e.note });
            juce::AudioBuffer<float> buffer (2, 256);
            for (int b = 0; b * beatsPerBlock < length - 1.0; ++b)
            {
                head.ppq = b * beatsPerBlock;
                juce::MidiBuffer midi;
                p.processBlock (buffer, midi);
                for (const auto m : midi)
                    if (m.getMessage().isNoteOn())
                        played.insert ({ FaseGrooveLeadProcessor::stepOf (head.ppq + m.samplePosition / (sr * 60.0 / head.bpm)), m.getMessage().getNoteNumber() });
            }
            int missing = 0;
            for (const auto& v : played)
                missing += shown.count (v) == 0 ? 1 : 0;
            CHECK (missing == 0);
            CHECK (! played.empty());
        }
    }

    void testTransport()
    {
        FaseGrooveLeadProcessor p;
        PlayHead head;
        p.setPlayHead (&head);
        p.setParameterFromUI (ParamIDs::fills, 0.0f);
        p.setParameterFromUI (ParamIDs::variation, 0.0f);
        p.setParameterFromUI (ParamIDs::humanize, 0.0f);
        p.selectGroove (0);
        p.setRateAndBufferSizeDetails (48000.0, 512);
        p.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (2, 512);
        int notes = 0;
        bool sound = false; float dbgPeak = 0;
        const double beatsPerBlock = 512.0 / (48000.0 * 60.0 / head.bpm);
        const int blocks = (int) std::ceil (8.0 / beatsPerBlock);   // two bars
        for (int b = 0; b < blocks; ++b)
        {
            head.ppq = b * beatsPerBlock;
            juce::MidiBuffer midi;
            p.processBlock (buffer, midi);
            for (const auto m : midi)
                notes += m.getMessage().isNoteOn() ? 1 : 0;
            sound = sound || buffer.getMagnitude (0, 512) > 0.01f;
            dbgPeak = std::max (dbgPeak, buffer.getMagnitude (0, 512));
        }
        CHECK (dbgPeak > 0.4f && dbgPeak <= 1.0f);   // loud enough on its own, never past full scale
        CHECK (notes >= 8);   // eight monophonic offbeat attacks
        CHECK (sound);           // the guide lead plays

        // Stop: everything is released, nothing new starts.
        head.playing = false;
        juce::MidiBuffer midi;
        p.processBlock (buffer, midi);
        for (const auto m : midi)
            CHECK (! m.getMessage().isNoteOn());

        // The sequencer off: the DAW plays, the groove doesn't; PLAY still previews it.
        head.playing = true;
        p.setParameterFromUI (ParamIDs::sequencer, 0.0f);
        for (int b = 0; b < 40; ++b)
        {
            juce::MidiBuffer m2;
            head.ppq = 100.0 + b * beatsPerBlock;
            p.processBlock (buffer, m2);
            for (const auto m : m2)
                CHECK (! m.getMessage().isNoteOn());
        }
        p.previewPlay();
        int previewed = 0;
        for (int b = 0; b < 40; ++b)
        {
            juce::MidiBuffer m2;
            head.ppq = 200.0 + b * beatsPerBlock;
            p.processBlock (buffer, m2);
            for (const auto m : m2)
                previewed += m.getMessage().isNoteOn() ? 1 : 0;
        }
        CHECK (previewed > 0);
        p.setPlayHead (nullptr);
    }

    void testChordInput()
    {
        FaseGrooveLeadProcessor p;
        PlayHead head;
        p.setPlayHead (&head);
        p.setParameterFromUI (ParamIDs::fills, 0.0f);
        p.setParameterFromUI (ParamIDs::variation, 0.0f);
        p.selectGroove (0);
        p.setRateAndBufferSizeDetails (48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        const double beatsPerBlock = 512.0 / (48000.0 * 60.0 / head.bpm);

        // E9 held on the MIDI input (E G# B D F#): the lead plays it, the notes held don't pass through.
        const fgl::Chord e9 { 4, (int) fgl::ChordType::dom9 };
        int played = 0;
        for (int b = 0; b < 60; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                for (int n : { 40, 56, 59, 62, 66 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
            head.ppq = b * beatsPerBlock;
            p.processBlock (buffer, midi);
            for (const auto m : midi)
                if (m.getMessage().isNoteOn())
                {
                    ++played;
                    const auto context = p.currentSettings();
                    const auto& scale = fgl::scaleIntervals (context.scale);
                    const int pc = (m.getMessage().getNoteNumber() - context.key + 120) % 12;
                    CHECK (isChordTone (e9, m.getMessage().getNoteNumber()) || std::find (scale.begin(), scale.end(), pc) != scale.end());
                    CHECK (m.getMessage().getVelocity() != 100 || m.samplePosition > 0 || b > 0);
                }
        }
        CHECK (played > 0);
        CHECK (p.followsHeldChord());
        CHECK (ChordPanel::nameOf (p.heldChord()) == "E9");

        // Released: back to the chord slots.
        {
            juce::MidiBuffer midi;
            for (int n : { 40, 56, 59, 62, 66 })
                midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            p.processBlock (buffer, midi);
            CHECK (! p.followsHeldChord());
        }

        // The same note struck twice: held until both are released; All Notes Off lets go of all.
        {
            juce::MidiBuffer midi;
            for (int n : { 40, 56, 59, 62, 66 })
                midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 90), 1);
            midi.addEvent (juce::MidiMessage::noteOff (1, 40), 2);
            p.processBlock (buffer, midi);
            CHECK (p.followsHeldChord() && ChordPanel::nameOf (p.heldChord()) == "E9");   // E still held once
            juce::MidiBuffer off;
            off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            p.processBlock (buffer, off);
            CHECK (! p.followsHeldChord());
        }

        // MIDI IN off: the notes pass through.
        p.setParameterFromUI (ParamIDs::chordInput, 0.0f);
        head.playing = false;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 99), 10);
        p.processBlock (buffer, midi);
        bool passed = false;
        for (const auto m : midi)
            passed = passed || (m.getMessage().isNoteOn() && m.getMessage().getNoteNumber() == 64 && m.getMessage().getVelocity() == 99);
        CHECK (passed);
        CHECK (! p.followsHeldChord());

        // A lane played by hand: its notes now, released half a second later.
        p.audition ((int) fgl::Lane::stab, 110);
        juce::MidiBuffer first;
        p.processBlock (buffer, first);
        int ons = 0;
        for (const auto m : first)
            ons += m.getMessage().isNoteOn() ? 1 : 0;
        CHECK (ons > 0 && ons <= 3);   // the remaining strings can belong to later blocks
        int offs = 0;
        for (int b = 0; b < 60; ++b)
        {
            juce::MidiBuffer m2;
            p.processBlock (buffer, m2);
            for (const auto m : m2)
            {
                ons += m.getMessage().isNoteOn() ? 1 : 0;
                offs += m.getMessage().isNoteOff() ? 1 : 0;
            }
        }
        CHECK (ons == 1);
        CHECK (offs == 1);
        p.setPlayHead (nullptr);
    }

    void testMidiImport()
    {
        // Four bars: Dm7, G7, Cmaj7, Am7 strummed on the beats, a chank on the offbeats.
        const fgl::Chord chords[] { { 2, (int) fgl::ChordType::m7 }, { 7, (int) fgl::ChordType::dom7 },
                                    { 0, (int) fgl::ChordType::maj7 }, { 9, (int) fgl::ChordType::m7 } };
        juce::MidiMessageSequence track;
        for (int bar = 0; bar < 4; ++bar)
        {
            std::vector<int> notes;
            const auto harmony = fgl::tonesOf (chords[bar]);
            for (int interval = 0; interval < 12; ++interval)
                if ((harmony.mask & (1u << interval)) != 0) notes.push_back (60 + harmony.root + interval);
            for (int beat = 0; beat < 4; ++beat)
            {
                const double t = (bar * 4 + beat) * 480.0;
                for (int n : notes)
                {
                    track.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), t);
                    track.addEvent (juce::MidiMessage::noteOff (1, n), t + 400.0);
                }
                const int high = 72 + chords[bar].root + 7;
                track.addEvent (juce::MidiMessage::noteOn (1, high, (juce::uint8) 80), t + 240.0);
                track.addEvent (juce::MidiMessage::noteOff (1, high), t + 300.0);
            }
        }
        track.updateMatchedPairs();
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        midi.addTrack (track);
        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("fgl-import-test.mid");
        file.deleteFile();
        {
            juce::FileOutputStream out (file);
            midi.writeTo (out);
        }

        FaseGrooveLeadProcessor p;
        CHECK (p.importMidiGroove (file));
        CHECK (juce::roundToInt (value (p, ParamIDs::pattern)) == fgl::userPatternIndex());
        for (int bar = 0; bar < 4; ++bar)
            CHECK (p.chordAt (bar) == chords[bar]);
        CHECK (fgl::isHit (p.stepSymbol ((int) fgl::Lane::hook, 0)));
        CHECK (fgl::isHit (p.stepSymbol ((int) fgl::Lane::chank, 2)));
        file.deleteFile();

        // An empty file: nothing changes.
        juce::MidiFile empty;
        empty.setTicksPerQuarterNote (480);
        empty.addTrack (juce::MidiMessageSequence());
        {
            juce::FileOutputStream out (file);
            empty.writeTo (out);
        }
        CHECK (! p.importMidiGroove (file));
        file.deleteFile();
    }

    void testGuideLead()
    {
        static constexpr double rate = 44100.0;
        std::array<float, fgl::numLanes> volume {};
        volume.fill (1.0f);

        // The level of a note (in a lane) `at` seconds after it starts, with a note-off at `off`.
        auto levelAt = [&] (int lane, double at, double off)
        {
            GuideLead g;
            g.prepare (rate);
            juce::AudioBuffer<float> buffer (2, (int) (at * rate) + 2048);
            buffer.clear();
            std::vector<GuideLead::Event> events { { 0, 57, 1.0f, lane } };
            if (off >= 0.0)
                events.push_back ({ (int) (off * rate), 57, 0.0f, lane });
            g.render (buffer, events, volume);
            return buffer.getRMSLevel (0, (int) (at * rate), 2048);
        };
        CHECK (levelAt ((int) fgl::Lane::hook, 0.05, -1.0) > 0.005f);    // it sounds
        CHECK (levelAt ((int) fgl::Lane::hook, 0.5, -1.0) > 0.001f);     // and rings
        CHECK (levelAt ((int) fgl::Lane::hook, 0.5, 0.1) < 1.0e-4f);     // the finger lifts: it stops

        // In tune: the period of a plucked A2, A3 and E4 (autocorrelation over 50 ms).
        for (int note : { 45, 57, 64 })
        {
            GuideLead g;
            g.prepare (rate);
            juce::AudioBuffer<float> buffer (2, (int) (rate * 0.4));
            buffer.clear();
            g.render (buffer, { { 0, note, 1.0f, (int) fgl::Lane::hook } }, volume);
            const float* x = buffer.getReadPointer (0);
            std::vector<double> crossings;
            for (int i = (int) (rate * 0.1); i < buffer.getNumSamples(); ++i)
                if (x[i-1] <= 0 && x[i] > 0)
                    crossings.push_back (i - 1 - x[i-1] / (double) (x[i] - x[i-1]));
            CHECK (crossings.size() > 2);
            const double measured = rate * (double) (crossings.size() - 1) / (crossings.back() - crossings.front());
            const double expected = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            CHECK (std::abs (measured / expected - 1.0) < 0.005);

        }

        // Many notes: no NaN, no blow-up, on every sound.
        for (int sound = 0; sound < GuideLead::numTones; ++sound)
        {
            GuideLead g;
            g.prepare (rate);
            g.setSound (sound);
            juce::AudioBuffer<float> buffer (2, (int) rate);
            buffer.clear();
            std::vector<GuideLead::Event> events;
            for (int i = 0; i < 200; ++i)
                events.push_back ({ i * 200, 40 + (i * 7) % 40, 0.9f, i % fgl::numLanes });
            g.render (buffer, events, volume);
            const float peak = buffer.getMagnitude (0, buffer.getNumSamples());
            CHECK (std::isfinite (peak) && peak > 0.01f && peak < 8.0f);
        }
    }

    void testUpdater()
    {
        CHECK (Updater::isNewer ("v0.2.0", "0.1.0"));
        CHECK (Updater::isNewer ("v1.0.0", "0.10.3"));
        CHECK (! Updater::isNewer ("v0.1.0", "0.1.0"));
        const auto notes = Updater::changelogSection ("# Changelog\n\n## [0.2.0] - x\n\n- New A\n\n## [0.1.0] - y\n\n- Old\n", "0.2.0");
        CHECK (notes.contains ("New A") && ! notes.contains ("Old"));
    }

    void testTranslations()
    {
        const auto before = Localisation::current();
        Localisation::setCurrent (Localisation::Language::italian);
        CHECK (tr ("CLOSE") == "CHIUDI");
        CHECK (tr ("Chords") == "Accordi");
        CHECK (tr ("Hook") == "Motivo");
        CHECK (tr ("Swing") == "Swing");                         // technical terms stay in English
        CHECK (tr ("Some groove name") == "Some groove name");   // no translation: as it is

        // Every language has a text for the lead's own words.
        for (int lang = 1; lang < Localisation::numLanguages; ++lang)
        {
            Localisation::setCurrent ((Localisation::Language) lang);
            for (const auto* key : { "Lead part", "Chords", "Plays", "HEAR IT", "Random groove", "Hook", "Ghost",
                                     "Hold a chord on the MIDI input", "Groove feel & song", "CLOSE" })
                CHECK (tr (key).isNotEmpty() && tr (key) != key);
        }
        Localisation::setCurrent (before);
    }

    void testEditor()
    {
        // The editor opens, follows the processor, and closes, in every language.
        FaseGrooveLeadProcessor p;
        for (int lang = 0; lang < Localisation::numLanguages; ++lang)
        {
            Localisation::setCurrent ((Localisation::Language) lang);
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            editor->setSize (1180, 860);
            if (auto* t = dynamic_cast<juce::Timer*> (editor.get()))
                t->timerCallback();
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            CHECK (image.isValid());
        }
        Localisation::setCurrent (Localisation::Language::english);
    }

    //==============================================================================
    void saveScreenshots (const juce::File& folder)
    {
        folder.createDirectory();
        FaseGrooveLeadProcessor p;
        p.applyStyle (8);   // Funky & Disco
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        editor->setSize (1180, 860);
        if (auto* t = dynamic_cast<juce::Timer*> (editor.get()))
            t->timerCallback();

        auto save = [&] (const juce::String& name)
        {
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            juce::FileOutputStream out (folder.getChildFile (name + ".png"));
            out.setPosition (0);
            out.truncate();
            png.writeImageToStream (image, out);
        };
        std::function<juce::Button* (juce::Component&, const juce::String&)> findButton = [&] (juce::Component& c, const juce::String& text) -> juce::Button*
        {
            for (auto* child : c.getChildren())
            {
                if (auto* b = dynamic_cast<juce::Button*> (child); b != nullptr && b->getButtonText() == text)
                    return b;
                if (auto* found = findButton (*child, text))
                    return found;
            }
            return nullptr;
        };
        auto click = [&] (const juce::String& text) { if (auto* b = findButton (*editor, tr (text)); b != nullptr && b->onClick) b->onClick(); };

        save ("1-main");
        click ("PROGRESSION...");
        save ("1c-progressions");
        std::function<void (juce::Component&)> hideBrowsers = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (dynamic_cast<ProgressionBrowser*> (child) != nullptr)
                    child->setVisible (false);
                hideBrowsers (*child);
            }
        };
        hideBrowsers (*editor);
        click ("EDITOR...");
        save ("1b-editor");
        click ("VELOCITY");
        save ("2-velocity");
        click ("MIXER");
        save ("2b-mixer");
        click ("FILL");
        p.customFillFromStyle ((int) fgl::FillStyle::highRun);
        save ("2d-fill");
        click ("STEP GRID");
        click ("FULL SONG...");
        save ("3-song");
        click ("CLOSE");
        click ("OPTIONS");
        save ("4-options");
    }
    void testLinkRoles()
    {
        using P = FaseGrooveLeadProcessor;
        // KEY: another plug-in's key arrives only with KEY on (the chords move with it).
        {
            const auto link = P::linkFile();
            const bool existed = link.existsAsFile();
            const auto backup = link.loadFileAsString();
            auto writeTheirs = [&link] (int key)
            {
                juce::XmlElement xml ("FaseGrooveLink");
                xml.setAttribute ("instance", "someone else");
                xml.setAttribute ("key", key);
                xml.setAttribute ("scale", 2);
                xml.writeTo (link);
            };
            P p;
            p.setLinked (true);
            const int before = juce::roundToInt (p.raw (ParamIDs::key));
            writeTheirs ((before + 3) % 12);
            p.updateLink();
            CHECK (juce::roundToInt (p.raw (ParamIDs::key)) == before);
            p.setKeyLinked (true);
            writeTheirs ((before + 5) % 12);
            p.updateLink();
            CHECK (juce::roundToInt (p.raw (ParamIDs::key)) == (before + 5) % 12);
            p.setLinked (false);
            if (existed) link.replaceWithText (backup); else link.deleteFile();
        }


    }

}

namespace {

    /** The numbered parameters (per lane, chord slot, keyswitch, part) reach the engine settings,
        through the pointers looked up once for the audio thread. */
    void testNumberedParameters()
    {
        FaseGrooveLeadProcessor p;
        for (int l = 0; l < fgl::numLanes; ++l)
        {
            p.setParameterFromUI (ParamIDs::octave (l), l % 2 == 0 ? 1.0f : -1.0f);
            p.setParameterFromUI (ParamIDs::level (l), 0.5f + 0.05f * (float) l);
            p.setParameterFromUI (ParamIDs::mute (l), l == 3 ? 1.0f : 0.0f);
            p.setParameterFromUI (ParamIDs::push (l), (float) l - 5.0f);
            p.setParameterFromUI (ParamIDs::laneSwing (l), 0.1f * (float) l);
        }
        for (int i = 0; i < fgl::maxChords; ++i)
            p.setChord (i, { (i * 5) % 12, i % fgl::numChordTypes });
        p.setSegmentAt (0, 2);
        p.setSegmentAt (3, 1);
        p.setParameterFromUI (ParamIDs::keyswitch (2), 31.0f);

        const auto s = p.currentSettings();
        for (int l = 0; l < fgl::numLanes; ++l)
        {
            CHECK (s.octaves[(size_t) l] == (l % 2 == 0 ? 1 : -1));
            CHECK (std::abs (s.levels[(size_t) l] - (0.5f + 0.05f * (float) l)) < 0.02f);
            CHECK (s.muted[(size_t) l] == (l == 3));
            CHECK (std::abs (s.laneShift[(size_t) l] - ((float) l - 5.0f)) < 0.6f);
            CHECK (std::abs (s.laneSwing[(size_t) l] - 0.1f * (float) l) < 0.02f);
        }
        for (int i = 0; i < fgl::maxChords; ++i)
            CHECK ((s.chords[(size_t) i] == fgl::Chord { (i * 5) % 12, i % fgl::numChordTypes }));
        CHECK (s.segmentOrder[0] == 2 && s.segmentOrder[3] == 1);
        CHECK (s.keyswitch[2] == 31);
    }

    void testFamilyPresets(){
        FaseGrooveLeadProcessor p;const int first=p.styleNames().size()-fgfamily::numGenres;
        for(int genre=0;genre<fgfamily::numGenres;++genre){
            std::printf("Family genre %d: apply\n",genre);std::fflush(stdout);
            p.applyStyle(first+genre);const auto& a=fgfamily::sound(genre);
            std::printf("Family genre %d: render\n",genre);std::fflush(stdout);
            const auto settings=p.currentSettings();
            CHECK(settings.randomGenre==genre);
            CHECK(juce::roundToInt(value(p,ParamIDs::kit))==a.guitar);
            CHECK(std::abs(value(p,ParamIDs::tone)-a.guitarTone)<.001);
            CHECK(!fgl::GrooveEngine::renderClip(settings,32).empty());
            const int prog=juce::roundToInt(value(p,ParamIDs::progression));
            CHECK(juce::String(fgl::progressions()[(size_t)prog].name)==fgfamily::progressionNames[(size_t)genre]);
            if(genre==4){
                CHECK(settings.pattern==fgl::userPatternIndex());
                for(int lane=0;lane<fgl::numLanes;++lane)for(int step=0;step<16;++step)CHECK(settings.user[(size_t)lane][(size_t)step]=='.');
            }
        }
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    if(argc>2&&juce::String(argv[1])=="--family-audio")return writeFamilyAudioEvidence<FaseGrooveLeadProcessor>(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]),"Lead");

    if (argc > 3 && juce::String (argv[1]) == "--screenshots")
        Localisation::setCurrent ((Localisation::Language) juce::String (argv[3]).getIntValue());
    if (argc > 2 && juce::String (argv[1]) == "--screenshots")
    {
        saveScreenshots (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        return 0;
    }

    testNumberedParameters();
    testFamilyPresets();
    testOldPresetDefaults();
    testStateRoundTrip();
    testGridEditing();
    testChordsAndStyles();
    testMidiExport();
    testGrooveCompatibility();
    testAudioExport();
    testChannelChangeWhilePlaying();
    testAuditionAcrossBlocks();
    testOverviewShowsWhatPlays();
    testTransport();
    testChordInput();
    testMidiImport();
    testGuideLead();
    testUpdater();
    FaseGrooveTests::checkUpdaterSafeguards();
    FaseGrooveTests::checkLinkWithThreeInstances<FaseGrooveLeadProcessor>();
    testTranslations();
    testLinkRoles();
    testEditor();

    if (failures == 0)
        std::printf ("All plug-in tests passed.\n");
    return failures == 0 ? 0 : 1;
}
