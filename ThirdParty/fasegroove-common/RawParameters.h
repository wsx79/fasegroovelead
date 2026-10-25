// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Every parameter's value by ID, looked up once: reading one (audio thread included) then never
// builds a String nor searches the parameter tree.

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace FaseGroove
{
    class RawParameters
    {
    public:
        /** Takes every parameter of the processor (call once, after the value tree state). */
        void cache (juce::AudioProcessorValueTreeState& state)
        {
            const auto& params = state.processor.getParameters();
            values.assign ((size_t) params.size(), nullptr);
            byId.clear();
            byId.reserve ((size_t) params.size());
            for (int i = 0; i < params.size(); ++i)
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (params[i]))
                {
                    values[(size_t) i] = state.getRawParameterValue (withId->paramID);
                    byId[std::string_view (withId->paramID.toRawUTF8())] = i;   // the ID's text lives as long as the parameter
                }
        }

        /** The value of a parameter, nullptr when the processor has none with that ID. */
        std::atomic<float>* find (juce::StringRef id) const noexcept
        {
            const auto it = byId.find (std::string_view (id.text.getAddress(), (size_t) id.length()));
            return it != byId.end() ? values[(size_t) it->second] : nullptr;
        }

        float operator() (juce::StringRef id) const noexcept
        {
            if (const auto* v = find (id))
                return v->load();
            jassertfalse;   // not a parameter of this plug-in
            return 0.0f;
        }

    private:
        std::vector<std::atomic<float>*> values;           // in parameter order, filled once
        std::unordered_map<std::string_view, int> byId;    // ID -> index in values
    };
}
