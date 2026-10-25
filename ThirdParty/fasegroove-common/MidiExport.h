// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// MIDI files written by the products (clips, songs, one track per part).

#include <juce_audio_basics/juce_audio_basics.h>

namespace FaseGroove
{
    /** Writes through a temporary file, so a failed write never leaves half a file behind. */
    inline bool writeMidiFileSafely (const juce::MidiFile& midi, const juce::File& target)
    {
        juce::TemporaryFile temp (target);
        {
            juce::FileOutputStream out (temp.getFile());
            if (! out.openedOk() || ! midi.writeTo (out))
                return false;
            out.flush();
            if (out.getStatus().failed())
                return false;
        }
        return temp.overwriteTargetFileWithTemporary();
    }
}
