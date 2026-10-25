// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "Updater.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace FaseGroove
{
    /** "A new version is ready": what's new (the changelog) and the user confirms the install. */
    class UpdateOverlay : public juce::Component
    {
    public:
        UpdateOverlay();
        void show (const Updater::Result&);
        void refreshText();
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        juce::Rectangle<float> card() const { return getLocalBounds().toFloat().withSizeKeepingCentre (640.0f, 560.0f); }
        Updater::Result result;
        juce::TextEditor notes;
        juce::TextButton installButton, laterButton, skipButton;
    };
}
