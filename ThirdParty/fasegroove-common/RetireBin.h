// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_events/juce_events.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace FaseGroove
{
    /** Objects replaced while the audio thread may still use them: kept here until nobody else
        holds them, then freed on the message thread (never on the audio thread, mid-block). */
    template <typename T>
    struct RetireBin
    {
        RetireBin() { sweeper.startTimer (1000); }

        void retire (std::shared_ptr<const T> old)
        {
            if (old == nullptr)
                return;
            const juce::ScopedLock lock (mutex);
            held.push_back (std::move (old));
        }

    private:
        void freeUnused()
        {
            const juce::ScopedLock lock (mutex);
            held.erase (std::remove_if (held.begin(), held.end(), [] (const auto& p) { return p.use_count() <= 1; }), held.end());
        }

        juce::CriticalSection mutex;
        std::vector<std::shared_ptr<const T>> held;
        juce::TimedCallback sweeper { [this] { freeUnused(); } };   // last: stopped before the rest goes
    };
}
