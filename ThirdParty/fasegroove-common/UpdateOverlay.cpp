// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "UpdateOverlay.h"

#include "FamilyTheme.h"
#include "Localisation.h"
#include "ProductInfo.h"

namespace FaseGroove
{
namespace
{
    juce::Font font (float height, bool bold = false) { return FamilyTheme::font (height, bold); }
}

UpdateOverlay::UpdateOverlay()
{
    notes.setMultiLine (true, true);
    notes.setReadOnly (true);
    notes.setScrollbarsShown (true);
    notes.setIndents (10, 8);
    notes.setCaretVisible (false);
    notes.setFont (font (14.0f));
    notes.setColour (juce::TextEditor::backgroundColourId, Palette::background);
    notes.setColour (juce::TextEditor::textColourId, Palette::text);
    notes.setColour (juce::TextEditor::outlineColourId, Palette::accent.withAlpha (0.3f));
    addAndMakeVisible (notes);

    installButton.setColour (juce::TextButton::buttonColourId, Palette::accent);
    installButton.setColour (juce::TextButton::textColourOffId, Palette::background);
    for (auto* b : { &installButton, &laterButton, &skipButton })
        addAndMakeVisible (b);

    installButton.onClick = [this]
    {
        const auto report = Updater::install (result);
        setVisible (false);

        if (report.relaunching)
        {
            // Standalone app: close now; the new version opens by itself.
            juce::MessageManager::callAsync ([] { if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); });
            return;
        }

        if (! report.updated.isEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, product().productName,
                                                    tr ("Updated to version") + " " + result.version + ":\n\n"
                                                        + report.updated.joinIntoString ("\n") + "\n\n"
                                                        + tr ("Restart your DAW (or the app) to use the new version."));
        else if (! report.done && ! report.installerStarted)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, product().productName,
                                                    tr ("The files could not be replaced automatically: the download is shown, copy it as the first time."));
    };
    laterButton.onClick = [this] { setVisible (false); };
    skipButton.onClick = [this]
    {
        Updater::skipVersion (result.version);
        setVisible (false);
    };
    refreshText();
}

void UpdateOverlay::show (const Updater::Result& r)
{
    result = r;
    notes.setText (r.notes, false);
    notes.moveCaretToTop (false);
    refreshText();
    setVisible (true);
    toFront (false);
}

void UpdateOverlay::refreshText()
{
    installButton.setButtonText (tr ("INSTALL NOW"));
    laterButton.setButtonText (tr ("LATER"));
    skipButton.setButtonText (tr ("SKIP THIS VERSION"));
    repaint();
}

void UpdateOverlay::resized()
{
    auto inner = card().toNearestInt().reduced (28, 22);
    inner.removeFromTop (92);
    auto buttons = inner.removeFromBottom (34);
    inner.removeFromBottom (44);
    notes.setBounds (inner);

    const int w = 170, gap = 12;
    buttons = buttons.withSizeKeepingCentre (3 * w + 2 * gap, buttons.getHeight());
    for (auto* b : { &installButton, &laterButton, &skipButton })
    {
        b->setBounds (buttons.removeFromLeft (w));
        buttons.removeFromLeft (gap);
    }
}

void UpdateOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.78f));

    auto c = card();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (c, 10.0f);
    g.setColour (Palette::panelEdge.brighter (0.3f));
    g.drawRoundedRectangle (c, 10.0f, 1.0f);

    auto r = c.reduced (28.0f, 22.0f);
    g.setColour (Palette::accent);
    g.setFont (font (22.0f, true));
    g.drawText (tr ("New version") + " " + result.version + " " + tr ("downloaded"), r.removeFromTop (30.0f), juce::Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (font (13.0f));
    g.drawText (tr ("You have") + " " + juce::String (JucePlugin_VersionString) + ". " + tr ("What's new:"),
                r.removeFromTop (26.0f), juce::Justification::centredLeft);

    const char* how = result.kind == Updater::InstallKind::installer
                        ? "INSTALL NOW updates it with the setup, without the wizard (same folders and parts). Close your DAW if it asks."
                    : result.kind == Updater::InstallKind::portable
                        ? "INSTALL NOW replaces the files of this copy where they are. Then restart your DAW."
                       #if JUCE_MAC
                        : "INSTALL NOW opens the zip: copy the plug-ins to the same folders as the first time, then restart your DAW.";
                       #else
                        : "INSTALL NOW shows the downloaded zip: copy the plug-ins to the same folders as the first time.";
                       #endif
    auto bottom = c.reduced (28.0f, 22.0f).removeFromBottom (34.0f + 44.0f).removeFromTop (44.0f);
    g.drawFittedText (tr (how), bottom.toNearestInt(), juce::Justification::centredLeft, 2);
    g.setFont (font (11.0f));
    g.drawText (result.file.getFullPathName(), r.removeFromTop (20.0f), juce::Justification::centredLeft);
}

} // namespace FaseGroove
