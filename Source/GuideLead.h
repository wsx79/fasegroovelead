// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "engine/Lead.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>

// A monophonic, band-limited guide synth. Event timing is shared with exported MIDI.
class GuideLead
{
public:
    static constexpr int numTones = 4;
    static const char* toneName (int t) noexcept
    {
        static const char* names[] { "Soft sine", "Warm triangle", "Bright saw", "Round pulse" };
        return t >= 0 && t < numTones ? names[t] : "";
    }
    struct Event { int sample = 0; int note = 60; float velocity = 0; int lane = 0; };
    void prepare (double rate) { sampleRate = rate > 0 ? rate : 44100; allNotesOff(); }
    void setSound (int s) noexcept { sound = juce::jlimit (0, numTones - 1, s); }
    void setTone (float t) noexcept { tone = juce::jlimit (0.0f, 1.0f, t); }
    void allNotesOff() noexcept { note = -1; envelope = filter = 0; gate = false; phase = 0; }
    void render (juce::AudioBuffer<float>& buffer, const std::vector<Event>& events,
                 const std::array<float, fgl::numLanes>& levels) noexcept
    {
        int at = 0;
        for (const auto& e : events)
        {
            const int end = juce::jlimit (at, buffer.getNumSamples(), e.sample);
            renderRange (buffer, at, end); at = end;
            if (e.note < 0) allNotesOff();
            else if (e.velocity > 0)
            {
                note = e.note; gate = true;
                gain = e.velocity * levels[(size_t) juce::jlimit (0, fgl::numLanes - 1, e.lane)];
                frequency = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            }
            else if (note == e.note) gate = false;
        }
        renderRange (buffer, at, buffer.getNumSamples());
    }
private:
    void renderRange (juce::AudioBuffer<float>& buffer, int begin, int end) noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        const float attack = (float) (1.0 - std::exp (-1.0 / (sampleRate * 0.004)));
        const float release = (float) (1.0 - std::exp (-1.0 / (sampleRate * 0.035)));
        const double cutoff = std::min (sampleRate * 0.4, 600.0 + tone * tone * 14500.0);
        const float coefficient = (float) (1.0 - std::exp (-2.0 * pi * cutoff / sampleRate));
        for (int i = begin; i < end; ++i)
        {
            envelope += ((gate ? 1.0f : 0.0f) - envelope) * (gate ? attack : release);
            double wave = 0;
            // Add only harmonics below Nyquist; no aliased saw/pulse at high pitches.
            const int harmonics = std::min (24, (int) (sampleRate * 0.45 / std::max (1.0, frequency)));
            for (int h = 1; h <= harmonics; ++h)
            {
                double level = 0;
                if (sound == 0) level = h == 1 ? 1.0 : 0.0;
                else if (sound == 1 && h % 2 == 1) level = (h % 4 == 1 ? 1.0 : -1.0) / (h * h);
                else if (sound == 2) level = 0.65 / h;
                else if (sound == 3 && h % 2 == 1) level = 0.75 / h;
                wave += level * std::sin (phase * h * 2.0 * pi);
            }
            phase += frequency / sampleRate;
            phase -= std::floor (phase);
            const float input = (float) wave * envelope * gain * 0.28f;
            filter += coefficient * (input - filter);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.addSample (ch, i, filter);
        }
    }
    double sampleRate = 44100, phase = 0, frequency = 440;
    float envelope = 0, gain = 0, tone = 0.7f, filter = 0;
    int note = -1, sound = 0;
    bool gate = false;
};
