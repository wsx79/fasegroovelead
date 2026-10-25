// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Where the music is now, for the editor's playhead and step lights. The audio thread publishes
// each block's start (beats), tempo and the time it did so; the editor moves on from there at the
// tempo, up to the length of that block, so it is not one block or one timer tick behind. Nothing
// is moved on while stopped or in the first block after a jump (start, loop, seek).

#include <juce_core/juce_core.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace FaseGroove
{
    class PlayClock
    {
    public:
        /** Audio thread: a block that plays `beats` from `ppq` in `seconds`. */
        void publish (double ppq, double beats, double seconds) noexcept
        {
            const bool continues = expected >= 0.0 && std::abs (ppq - expected) < 0.05;
            expected = ppq + beats;
            const double ms = seconds * 1000.0;
            write (ppq, continues && ms > 0.0 ? beats / ms : 0.0, juce::Time::getMillisecondCounterHiRes(), ms);
        }

        /** Audio thread: not playing. */
        void stop() noexcept
        {
            expected = -1.0;
            write (-1.0, 0.0, 0.0, 0.0);
        }

        /** Any thread: the position now in beats, -1 when stopped. */
        double position() const noexcept { return positionAt (juce::Time::getMillisecondCounterHiRes()); }

        double positionAt (double nowMs) const noexcept
        {
            double ppq, beatsPerMs, stampMs, blockMs;
            for (;;)
            {
                const auto v = version.load (std::memory_order_acquire);
                if ((v & 1u) != 0)
                    continue;   // being written: the audio thread is done in a moment
                ppq = ppqValue.load (std::memory_order_relaxed);
                beatsPerMs = rateValue.load (std::memory_order_relaxed);
                stampMs = stampValue.load (std::memory_order_relaxed);
                blockMs = blockValue.load (std::memory_order_relaxed);
                std::atomic_thread_fence (std::memory_order_acquire);
                if (version.load (std::memory_order_relaxed) == v)
                    break;
            }
            if (ppq < 0.0)
                return -1.0;
            // Never past the block the audio thread rendered (the next one says where it went).
            const double dt = std::clamp (nowMs - stampMs, 0.0, std::min (blockMs, 200.0));
            return ppq + dt * beatsPerMs;
        }

    private:
        void write (double ppq, double beatsPerMs, double stampMs, double blockMs) noexcept
        {
            const auto v = version.load (std::memory_order_relaxed);
            version.store (v + 1, std::memory_order_relaxed);
            std::atomic_thread_fence (std::memory_order_release);
            ppqValue.store (ppq, std::memory_order_relaxed);
            rateValue.store (beatsPerMs, std::memory_order_relaxed);
            stampValue.store (stampMs, std::memory_order_relaxed);
            blockValue.store (blockMs, std::memory_order_relaxed);
            version.store (v + 2, std::memory_order_release);
        }

        std::atomic<std::uint32_t> version { 0 };
        std::atomic<double> ppqValue { -1.0 }, rateValue { 0.0 }, stampValue { 0.0 }, blockValue { 0.0 };
        double expected = -1.0;   // audio thread only: where the next block starts if nothing jumps
    };
}
