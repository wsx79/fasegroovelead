// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// LINK with three instances in one process: what one changes reaches the other two through
// Link.xml (LinkClient.h). Include after TestSupport.h (CHECK). The user's Link.xml is put back.

#include "LinkClient.h"

#include <memory>

namespace FaseGrooveTests
{
    template <typename Processor>
    void checkLinkWithThreeInstances()
    {
        const auto file = FaseGroove::LinkClient::file();
        const bool existed = file.existsAsFile();
        const auto saved = existed ? file.loadFileAsString() : juce::String();
        file.deleteFile();

        std::unique_ptr<Processor> a (new Processor()), b (new Processor()), c (new Processor());
        for (auto* p : { a.get(), b.get(), c.get() })
        {
            p->setLinked (true);
            p->updateLink();
        }

        // A new swing and tempo in one: the others take them.
        a->setParameterFromUI ("swing", 0.42f);
        a->setParameterFromUI ("tempo", 131.0f);
        a->updateLink();
        b->updateLink();
        c->updateLink();
        for (auto* p : { b.get(), c.get() })
        {
            CHECK (std::abs (p->raw ("swing") - 0.42f) < 0.01f);
            CHECK (std::abs (p->raw ("tempo") - 131.0f) < 0.11f);
        }

        // The play button in another: all three play from the same clock.
        b->previewPlay();
        a->updateLink();
        c->updateLink();
        for (auto* p : { a.get(), b.get(), c.get() })
            CHECK (p->previewOn.load());

        // ...and stop together.
        c->previewStop();
        a->updateLink();
        b->updateLink();
        for (auto* p : { a.get(), b.get(), c.get() })
            CHECK (! p->previewOn.load());

        a.reset(); b.reset(); c.reset();
        if (existed)
            file.replaceWithText (saved);
        else
            file.deleteFile();
    }
}
