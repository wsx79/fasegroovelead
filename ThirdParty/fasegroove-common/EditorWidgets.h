// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// The editor widgets of Fase Groove Drums and Guitar: icon buttons, knobs, parameter menus and the
// drag-to-DAW tile, with the menu helpers their editors use too. Colours and fonts: FamilyTheme.h.

#include "FamilyTheme.h"
#include "Localisation.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace FaseGroove
{
    /** Shows a value in a menu that the timer keeps up to date. Only a new value is shown: a pick
        still on its way is left alone (a ComboBox tells onChange a moment later, and showing the
        old value in between would undo the pick). onChange calls pickedFromMenu(). */
    void showInMenu (juce::ComboBox&, int id);
    void pickedFromMenu (juce::ComboBox&);
    /** A caption: upper case, small and dim. */
    void styleCaption (juce::Label&, const juce::String& text);

    /** The shared panel chrome; layout and instrument-specific contents stay in each editor. */
    void drawPanel (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title);

    /** The editor a DragTile shrinks while its files are dragged into the DAW. */
    struct MiniModeHost
    {
        virtual ~MiniModeHost() = default;
        virtual void enterMiniMode() = 0;
        virtual void exitMiniMode() = 0;
    };

    /** A flat button with a drawn icon (and optional text): transport, dice, undo, locks... */
    class IconButton : public juce::Button
    {
    public:
        enum class Icon { none, play, pause, stop, rewind, dice, undo, redo, gear, close, fullScreen, exitFullScreen,
                          lock, unlock, left, right, song, folder };

        explicit IconButton (Icon i = Icon::none) : juce::Button ({}), icon (i) {}
        void setIcon (Icon i) { if (i != icon) { icon = i; repaint(); } }
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        static void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float>, juce::Colour);

        juce::Colour onColour { Palette::accent };
        juce::Colour iconColour;   // when set: the icon's own colour while off (the dice are amber)
        bool primary = false;      // filled with onColour even when off (the main action of a panel)

    private:
        Icon icon;
    };

    //==============================================================================
    /** Rotary knob with a caption under it. */
    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption);
        void resized() override;
        void setTip (const juce::String& tip) { Localisation::bindTip (slider, tip); }
        /** The caption in an instrument's colour (what the control changes). */
        void setAccent (juce::Colour c) { label.setColour (juce::Label::textColourId, c); }

    private:
        juce::Slider slider;
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    /** A drop-down menu for a parameter, with a caption above it (or none). The items can be grouped
        under headings in any order: an item's id is always "parameter index + 1". */
    class ParamCombo : public juce::Component
    {
    public:
        ParamCombo (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption);
        void resized() override;

        /** Re-reads the parameter (call it a few times a second). */
        void refresh();
        juce::ComboBox& comboBox() noexcept { return box; }
        void setTip (const juce::String& tip) { Localisation::bindTip (box, tip); }

    private:
        juce::ComboBox box;
        juce::Label label;
        juce::RangedAudioParameter* parameter = nullptr;
        bool hasCaption = true;
    };

    //==============================================================================
    /** Drag me into the DAW: writes the MIDI clip(s) and starts an external file drag. */
    class DragTile : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        DragTile (juce::String titleText = "DRAG MIDI TO DAW", juce::String subtitleText = "drag this tile onto a track")
            : title (std::move (titleText)), subtitle (std::move (subtitleText)) {}
        std::function<juce::StringArray()> createFiles;
        bool filled = false;   // the big orange tile of the export panel
        void setSubtitle (const juce::String& s) { if (s != subtitle) { subtitle = s; repaint(); } }
        void paint (juce::Graphics&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override { dragging = false; repaint(); }
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }

    private:
        juce::String title, subtitle;
        bool dragging = false;
    };
}
