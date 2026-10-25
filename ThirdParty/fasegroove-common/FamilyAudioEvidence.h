// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <cmath>
#include "FamilySoundProfiles.h"
#include <cstdio>

// Native processor-level evidence, with the real host clock, internal guide
// engines and MIDI output. Only Jackin files are retained; all 13 are measured.
class FamilyEvidenceClock : public juce::AudioPlayHead {
public:
    double bpm=126,ppq=0;juce::int64 sample=0;bool playing=true;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo p;p.setBpm(bpm);p.setPpqPosition(ppq);p.setIsPlaying(playing);
        p.setTimeInSamples(sample);p.setTimeSignature(TimeSignature{4,4});return p;
    }
};
template<class Processor> int writeFamilyAudioEvidence(const juce::File& folder,const char* instrument){
    if(folder.createDirectory().failed())return 1;
    constexpr double rate=44100;constexpr int blockSize=512;
    juce::String report="genre\tbpm\tpeak\trms\n";
    for(int genre=0;genre<fgfamily::numGenres;++genre){
        Processor p;p.setLinked(false);
        p.applyStyle(p.styleNames().size()-fgfamily::numGenres+genre);
        p.setParameterFromUI(ParamIDs::sequencer,1);p.setParameterFromUI(ParamIDs::tempoSync,1);
        p.setParameterFromUI(ParamIDs::section,4);
        FamilyEvidenceClock clock;clock.bpm=fgfamily::tempos[(size_t)genre];
        p.setPlayHead(&clock);p.prepareToPlay(rate,blockSize);
        const auto music=(juce::int64)std::ceil(8*240/clock.bpm*rate);
        const auto total=music+(juce::int64)(2*rate);
        std::unique_ptr<juce::AudioFormatWriter> writer;
        if(genre==4){
            const auto file=folder.getChildFile(juce::String("Jackin-")+instrument+".wav");
            auto output=file.createOutputStream();if(!output)return 1;
            juce::WavAudioFormat wav;writer.reset(wav.createWriterFor(output.release(),rate,2,24,{},0));if(!writer)return 1;
        }
        juce::MidiMessageSequence notes;
        juce::AudioBuffer<float> audio(2,blockSize);juce::MidiBuffer midi;
        double peak=0,energy=0;juce::int64 count=0;
        for(juce::int64 at=0;at<total;at+=blockSize){
            const int n=(int)std::min<juce::int64>(blockSize,total-at);
            audio.setSize(2,n,false,false,true);audio.clear();midi.clear();
            clock.sample=at;clock.ppq=at/rate*clock.bpm/60;clock.playing=at<music;
            p.processBlock(audio,midi);
            for(int ch=0;ch<2;++ch)for(int i=0;i<n;++i){const auto v=audio.getSample(ch,i);if(!std::isfinite(v))return 1;peak=juce::jmax(peak,(double)std::abs(v));energy+=v*v;++count;}
            if(writer&&!writer->writeFromAudioSampleBuffer(audio,0,n))return 1;
            if(genre==4)for(const auto meta:midi){auto m=meta.getMessage();m.setTimeStamp((at+meta.samplePosition)/rate*clock.bpm/60*960);notes.addEvent(m);}
        }
        writer.reset();p.releaseResources();p.setPlayHead(nullptr);
        const double rms=std::sqrt(energy/(double)count);
        if(peak>=.99||rms<.00001)return 1;
        report+=juce::String(genre)+"\t"+juce::String(clock.bpm)+"\t"+juce::String(peak,6)+"\t"+juce::String(rms,6)+"\n";
        if(genre==4){
            juce::MidiMessageSequence conductor;conductor.addEvent(juce::MidiMessage::tempoMetaEvent((int)(60000000/clock.bpm)));conductor.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4,4));
            juce::MidiFile file;file.setTicksPerQuarterNote(960);file.addTrack(conductor);file.addTrack(notes);
            auto output=folder.getChildFile(juce::String("Jackin-")+instrument+".mid").createOutputStream();if(!output||!file.writeTo(*output))return 1;
        }
        std::printf("PASS native family profile %d: peak %.6f, rms %.6f\n",genre,peak,rms);
    }
    return folder.getChildFile("profiles.tsv").replaceWithText(report)?0:1;
}
