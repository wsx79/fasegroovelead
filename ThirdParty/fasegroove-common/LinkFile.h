// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// LINK: the products share their genre, tempo and play state through a small Link.xml file
// (the protocol is each product's own code). This is the shared, cheap way to read it.

#include <juce_core/juce_core.h>

namespace FaseGroove
{
    /** A file polled for changes (message thread): read again only when its date or size
        changed, when it was written in the last few seconds, or at least every few seconds
        anyway. Some file systems (HFS+, FAT) keep the date to 1-2 s, so two writes in the same
        second with the same size (bpm 120 -> 121) would otherwise go unseen. */
    struct PolledFile
    {
        const juce::String& read (const juce::File& file)
        {
            const auto now = juce::Time::getCurrentTime();
            const auto t = file.getLastModificationTime();
            const auto n = file.getSize();
            if (n != size || t != modified || (now - t).inMilliseconds() < 3000 || (now - lastRead).inMilliseconds() >= 5000)
            {
                text = file.loadFileAsString();
                modified = t;
                size = n;
                lastRead = now;
            }
            return text;
        }

    private:
        juce::Time modified, lastRead;
        juce::int64 size = -1;
        juce::String text;
    };
}
