// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include "Branding.h"
#include "EditorWidgets.h"
#include "Localisation.h"
#include "UpdateOverlay.h"
#include "Updater.h"
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

// The "studio" theme of Fase Groove Piano: graphite surfaces, one warm and one cool accent.
#include "FamilyTheme.h"

/** Each lane's colour, everywhere: repeats amber, stabs and chanks cyan, picking green, slides and
    swells violet, the low strings red. */
juce::Colour laneColour (int lane);

//==============================================================================
using GrooveLookAndFeel = FamilyLookAndFeel;

// The widgets shared by the Fase Groove editors (EditorWidgets.h, UpdateOverlay.h).
using FaseGroove::IconButton;
using FaseGroove::Knob;
using FaseGroove::ParamCombo;
using FaseGroove::DragTile;
using FaseGroove::UpdateOverlay;
using FaseGroove::showInMenu;
using FaseGroove::pickedFromMenu;
using FaseGroove::styleCaption;

//==============================================================================
/** The rhythm grid: one row per lane (with its die), 32 steps (two bars). */
class LeadGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    /** What a click on an empty step draws: 'X' accent, 'x' hit, 'g' ghost, 'f' rake (the editor's brush). */
    char brush = 'x';
    explicit LeadGrid (FaseGrooveLeadProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** Re-reads the groove and the playhead (editor timer). */
    void refresh();
    void setPlayStep (int step);   // repaints the two columns that change
    /** The lanes' lights (they flash on the hits) and the lane picked in the part panel. */
    void setLights (const std::array<float, fgl::numLanes>& l, int selected) { lights = l; selectedLane = selected; }
    std::function<void (int lane)> onSelectLane;

    static constexpr int headerWidth = 112;
    /** Called when a lane's name is double-clicked: show it in the velocity view. */
    std::function<void (int lane)> onShowVelocity;

private:
    juce::Rectangle<float> cellBounds (int lane, int step) const;
    juce::Rectangle<float> headerBounds (int lane) const;
    juce::Rectangle<float> diceBounds (int lane) const;
    bool cellAt (juce::Point<float>, int& lane, int& step) const;
    void showCellMenu (int lane, int step);
    void showLaneMenu (int lane);
    void paintCell (int lane, int step, char symbol);

    FaseGrooveLeadProcessor& processor;
    fgl::LeadPattern pattern;
    std::array<bool, fgl::numLanes> muted {};
    std::array<float, fgl::numLanes> lights {};
    int playStep = -1, selectedLane = -1;
    int hoverLane = -1, hoverStep = -1;
    bool hoverDice = false;
    char paintSymbol = 0;
    int lastPaintLane = -1, lastPaintStep = -1;
};

//==============================================================================
/** The velocity piano roll: one lane at a time (chosen from a menu), its hits on top and a
    velocity bar under each one, to drag up and down. */
class VelocityView : public juce::Component
{
public:
    explicit VelocityView (FaseGrooveLeadProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    void refresh();
    void setPlayStep (int step);   // repaints the two columns that change
    void setLane (int lane);
    int getLane() const noexcept { return lane; }

private:
    juce::Rectangle<float> stripArea() const;
    juce::Rectangle<float> laneArea() const;
    juce::Rectangle<float> column (int step, juce::Rectangle<float> area) const;
    int stepAt (float x) const;
    void setFromMouse (juce::Point<float>);

    FaseGrooveLeadProcessor& processor;
    int lane = (int) fgl::Lane::scratch;
    juce::ComboBox laneBox, shapeBox;
    IconButton diceButton { IconButton::Icon::dice };
    juce::TextButton autoButton { "AUTO" }, autoAllButton { "ALL AUTO" };
    std::array<char, FaseGrooveLeadProcessor::gridSteps> symbols {};
    std::array<int, FaseGrooveLeadProcessor::gridSteps> levels {}, setByHand {};
    int playStep = -1, hoverStep = -1;
    bool painting = false;
};

//==============================================================================
/** The fill drawn by hand (Fill style: Custom): one bar, every lane. It takes over from its first
    written step; the lanes left empty keep playing the groove. */
class FillView : public juce::Component, public juce::SettableTooltipClient
{
public:
    char brush = 'x';   // what a click on an empty step draws (the editor's brush)
    explicit FillView (FaseGrooveLeadProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { paintSymbol = 0; }

private:
    juce::Rectangle<float> gridArea() const;
    bool cellAt (juce::Point<float>, int& lane, int& step) const;
    void paintCell (int lane, int step, char symbol);

    FaseGrooveLeadProcessor& processor;
    juce::TextButton startButton { "START FROM..." }, clearButton { "CLEAR FILL" };
    char paintSymbol = 0;
    int lastLane = -1, lastStep = -1;
};

//==============================================================================
/** The song window: bars, own variation and fill of each section; drag or save the whole song,
    one section, or every section as its own clip. */
class SongOverlay : public juce::Component
{
public:
    explicit SongOverlay (FaseGrooveLeadProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void show();
    void refresh();

    FaseGrooveLeadProcessor::ClipPart clipPart() const;

private:
    juce::Rectangle<float> card() const;
    juce::Rectangle<float> timelineArea() const;
    juce::Rectangle<int> rowArea (int part) const;
    void saveSong();
    void saveSections();

    FaseGrooveLeadProcessor& processor;
    juce::OwnedArray<juce::ComboBox> barBoxes, fillBoxes;
    juce::OwnedArray<IconButton> newButtons;
    juce::OwnedArray<juce::TextButton> originalButtons;
    juce::TextButton allButton { "ONE TRACK" }, tracksButton { "ONE TRACK PER LANE" };
    juce::TextButton saveSongButton { "SAVE SONG .MID..." }, saveSectionsButton { "ONE .MID PER SECTION..." }, closeButton { "CLOSE" };
    juce::TextButton resetButton { "RESET SONG" };
    DragTile sectionTile { "DRAG THIS SECTION" }, sectionsTile { "DRAG ALL SECTIONS", "one clip per section" },
             songTile { "DRAG THE WHOLE SONG", "every section, with markers" };
    std::unique_ptr<juce::FileChooser> chooser;
    int selected = 1;   // the section the tile drags (part index)
};

//==============================================================================
/** OPTIONS, as in Fase Groove Piano: a SETTINGS tab (language, interface size, tips, updates,
    shortcuts) and an ABOUT tab (credits, licence, links). OPTIONS, or a click on the logo. */
class OptionsOverlay : public juce::Component
{
public:
    OptionsOverlay();

    std::function<void (float)> onScaleChosen;
    std::function<void (bool)> onTipsChanged;
    std::function<void()> onCheckUpdates;

    juce::TextButton checkUpdatesButton;

    void show (bool aboutTab, float currentScale);
    void refreshText();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> card() const { return getLocalBounds().toFloat().withSizeKeepingCentre (620.0f, 620.0f); }
    void selectTab (bool about);

    bool showingAbout = false;
    juce::Image artist;
    juce::TextButton settingsTab, aboutTab, closeButton;
    juce::TextButton sourceButton, kofiButton, paypalButton, spotifyButton;
    juce::Label languageLabel, sizeLabel;
    juce::ComboBox languageBox, sizeBox;
    juce::ToggleButton tipsToggle, updatesToggle;
};

//==============================================================================
/** One of the four parts of the groove (a half bar), shown like the piano's chord slots: its
    position, the part playing there (A-D), a picture of its hits, its die and its lock. */
class PartCard : public juce::Component
{
public:
    PartCard (FaseGrooveLeadProcessor&, int position);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh (const fgl::LeadPattern& shown, int playStep);

private:
    FaseGrooveLeadProcessor& processor;
    const int position;
    ParamCombo combo;
    IconButton dice { IconButton::Icon::dice }, lock { IconButton::Icon::unlock };
    fgl::LeadPattern pattern;
    bool playing = false, locked = false;
};

//==============================================================================
/** The groove browser: the genres on the left, their grooves on the right, each with a picture
    of its rhythm. A click loads it (you hear it at once if it plays). */
class GrooveBrowser : public juce::Component
{
public:
    explicit GrooveBrowser (FaseGrooveLeadProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    bool keyPressed (const juce::KeyPress&) override;
    void show();

private:
    juce::Rectangle<float> card() const;
    juce::Rectangle<float> genreBounds (int genre) const;
    juce::Rectangle<float> tileBounds (int index) const;
    const std::vector<int>& groovesOf (int genre) const;
    void pick (int pattern);

    FaseGrooveLeadProcessor& processor;
    int genre = 0, hover = -1;
    IconButton randomButton { IconButton::Icon::dice };
    juce::TextButton closeButton { "CLOSE" };
};

//==============================================================================
/** The mixer: a volume fader, a mute, push / pull and swing for each lane of the guide lead. */
class MixerView : public juce::Component
{
public:
    explicit MixerView (FaseGrooveLeadProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setLights (const std::array<float, fgl::numLanes>& l) { lights = l; repaint(); }

private:
    juce::Rectangle<int> strip (int lane) const;
    FaseGrooveLeadProcessor& processor;
    juce::OwnedArray<juce::Slider> faders, pushKnobs, swingKnobs;
    juce::OwnedArray<juce::TextButton> mutes;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> faderAttachments, knobAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachments;
    std::array<float, fgl::numLanes> lights {};
};

//==============================================================================
/** The chords, as the piano's chord slots: the key, a progression by genre, four chords (one bar
    each; click one to change it), the dice for new ones, and MIDI IN: a chord held on the MIDI
    input (your keyboard, or Fase Groove Piano's MIDI out) takes over. */
class ChordPanel : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ChordPanel (FaseGrooveLeadProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();

    /** "Am7", "Fmaj9"... the name of the chord some notes make ("A (5)" for a root and a 5th). */
    static juce::String nameOf (const fgl::ChordTones&);

private:
    juce::Rectangle<float> slotBounds (int slot) const;
    void showSlotMenu (int slot);

    FaseGrooveLeadProcessor& processor;
public:
    /** Opens the progression browser (set by the editor). */
    std::function<void()> onBrowse;
private:
    juce::ComboBox keyBox;
    juce::TextButton progressionButton { "PROGRESSION..." }, inputButton { "MIDI IN" };
    IconButton diceButton { IconButton::Icon::dice };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inputAttachment;
    std::array<fgl::Chord, fgl::maxChords> chords {};
    int count = 4;
    int playingSlot = -1;
    fgl::ChordTones held { 0, 0 };
    bool following = false;
};

//==============================================================================
/** Family harmony browser with a lead pitch preview and the tonal context controls. */
class ProgressionBrowser : public juce::Component
{
public:
    explicit ProgressionBrowser (FaseGrooveLeadProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void show();
    /** Re-reads the chords (editor timer, while it is open). */
    void refresh();

private:
    juce::Rectangle<float> card() const;
    juce::Rectangle<float> categoryBounds (int category) const;
    juce::Rectangle<float> listArea() const;
    juce::Rectangle<float> tileBounds (int index) const;   // before scrolling
    std::vector<int> progressionsOf (int category) const;
    void pick (int progression);

    FaseGrooveLeadProcessor& processor;
    int category = 0, hover = -1;
    float scroll = 0.0f;
    juce::ComboBox keyBox, scaleBox, colourBox, countBox;
    IconButton harmonyDice { IconButton::Icon::dice };
    juce::TextButton closeButton { "CLOSE" };
    juce::String lastState;
};

//==============================================================================
/** The groove at a glance, on the main window: every lane's hits in the bars playing (their
    variation or fill too) and the playhead, only to look at. A click opens the editor. */
class GrooveOverview : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit GrooveOverview (FaseGrooveLeadProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onOpen != nullptr) onOpen(); }
    void refresh();
    /** The playing step only (every UI tick, so the light keeps up with the sound): repaints
        just the two columns that changed. */
    void setPlayStep (int step);
    std::function<void()> onOpen;

private:
    juce::Rectangle<int> stepColumn (int step) const;
    FaseGrooveLeadProcessor& processor;
    fgl::LeadPattern pattern;
    std::array<bool, fgl::numLanes> muted {};
    int playStep = -1;
};

/** The volume of each lane in the guide sound, small: the only thing changed from the main
    window (everything else in the editor). */
class MiniMixer : public juce::Component
{
public:
    explicit MiniMixer (FaseGrooveLeadProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();   // redraws when a lane's mute changes

private:
    juce::Rectangle<int> strip (int lane) const;
    FaseGrooveLeadProcessor& processor;
    std::uint32_t mutedShown = 0;
    juce::OwnedArray<juce::Slider> faders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
};

/** The editor, big, over the whole window (as Fase Groove Piano's piano roll): the transport, the
    brush (what a click draws), the views (step grid, velocity, mixer, fill) and the groove's tools. */
class EditorOverlay : public juce::Component
{
public:
    EditorOverlay();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override {}   // nothing under it is clicked
    void refreshText();
    void setBrush (char symbol);
    std::function<void (char)> onBrush;
    std::function<int()> viewMode;           // the view shown (its caption above the tools)

    IconButton rewindButton { IconButton::Icon::rewind }, playButton { IconButton::Icon::play }, stopButton { IconButton::Icon::stop },
               closeButton { IconButton::Icon::close };
    juce::TextButton accentBrush, hitBrush, ghostBrush, flamBrush;
    char brush = 'x';
};

class FaseGrooveLeadEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, public juce::Timer,
                               public FaseGroove::MiniModeHost
{
public:
    explicit FaseGrooveLeadEditor (FaseGrooveLeadProcessor&);
    ~FaseGrooveLeadEditor() override;

    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    static constexpr int designWidth = 1180, designHeight = 716;

    void timerCallback() override;
    /** The step lights and the playhead, 60 times a second from the processor's PlayClock; only
        what changes is repainted. */
    void showPlayPosition();
    juce::TimedCallback playTimer { [this] { showPlayPosition(); } };
    int shownSegment = -2;
    long long shownWindow = -2;

private:

    /** Everything is laid out at the design size, then scaled (and centred) in the window. */
    struct Content : public juce::Component
    {
        explicit Content (FaseGrooveLeadEditor& e) : editor (e) {}
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;
        FaseGrooveLeadEditor& editor;
    };

    float currentScale() const;
    void applyTips();
    void translateHostWindow();
    void parentHierarchyChanged() override;
    void layout();
    void paintContent (juce::Graphics&);
    int selectedClipBars() const;
    FaseGrooveLeadProcessor::ClipPart selectedClipPart() const;
    void saveMidiFile();
    void showPresetsMenu();
    void showGroovesMenu();
    void askName (const juce::String& title, const juce::String& suggestion, std::function<void (const juce::String&)> onName);
    void importMidi();
    void importFile (const juce::File&);
    void setView (int mode);   // 0 step grid, 1 velocity, 2 mixer, 3 fill
    void showTemplateMenu();
    void showAudioExportMenu();
    void stepGroove (int delta);
    void selectLane (int lane);
    void refreshKitPanel();
    void undo();
    void redo();
    bool isFullScreen() const;
    void toggleFullScreen();
    void showMessage (const juce::String& text);

    FaseGrooveLeadProcessor& processor;
    GrooveLookAndFeel lookAndFeel;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    Content content { *this };

    // Header
    juce::TextButton presetsButton { "PRESETS" }, styleButton { "STYLE" }, resetButton { "RESET" };
    IconButton totalRandomButton { IconButton::Icon::dice }, undoButton { IconButton::Icon::undo }, redoButton { IconButton::Icon::redo },
               fullScreenButton { IconButton::Icon::fullScreen }, optionsButton { IconButton::Icon::gear };
    IconButton rewindButton { IconButton::Icon::rewind }, playButton { IconButton::Icon::play }, stopButton { IconButton::Icon::stop };
    juce::ToggleButton sequencerToggle { "Sequencer" };
    juce::TextButton syncButton { "SYNC DAW" };
    juce::Slider tempoSlider;
    juce::Label dawTempo;   // the DAW's tempo, in place of the slider while SYNC DAW is on
    juce::TextButton playingButton { "PLAYING..." };   // let ring, room for the band, push, keyswitches
    void showPlayingMenu();
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment, soundAttachment, resolveNoteAttachment, sequencerAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tempoAttachment;

    // Groove row
    ParamCombo grooveCombo, genreCombo, partLengthCombo;
    juce::TextButton grooveButton;
    IconButton prevGroove { IconButton::Icon::left }, nextGroove { IconButton::Icon::right };
    IconButton newBeatButton { IconButton::Icon::dice };
    juce::Slider densitySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> densityAttachment;
    std::unique_ptr<PartCard> partCards[fgl::numSegments];

    // Lead part (the lane picked)
    int selectedLane = 0;
    juce::ComboBox laneBox, octaveBox;
    juce::TextButton hearButton { "HEAR IT" }, muteButton { "MUTE" };
    IconButton laneDiceButton { IconButton::Icon::dice };
    juce::Slider levelKnob, volumeKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment, volumeAttachment;

    // Chords
    ChordPanel chordPanel;

    // Groove panel
    ParamCombo sectionCombo, fillsCombo, variationCombo, fillStyleCombo;
    juce::TextButton resolveNoteButton { "RESOLVE" }, newFeelButton { "NEW FEEL" };
    Knob swing, humanize, velocity, accents;

    // Guide sound & MIDI out
    juce::ToggleButton soundToggle { "Guide sound" };
    ParamCombo kitCombo, channelCombo;
    juce::Slider attackSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    Knob tone, reverb, volume;

    // Edit
    juce::TextButton gridTab { "STEP GRID" }, velocityTab { "VELOCITY" }, mixerTab { "MIXER" }, fillTab { "FILL" }, clearButton { "CLEAR" },
                     myGroovesButton { "MY GROOVES" }, importButton { "IMPORT MIDI" }, templateButton { "TEMPLATE..." }, timingButton { "TIMING..." };
    IconButton newRhythmButton { IconButton::Icon::dice };
    LeadGrid grid;
    VelocityView velocityView;
    MixerView mixerView;
    FillView fillView;
    int viewMode = 0;
    GrooveOverview overview { processor };
    MiniMixer miniMixer { processor };
    EditorOverlay editOverlay;
    juce::TextButton openEditorButton;
    // The same tools on the main window (the editor has its own): tabs groove / volumes, the dice,
    // MY GROOVES, IMPORT, TEMPLATE, CLEAR.
    juce::TextButton mainGrooveTab, mainVolumeTab, mainMyGrooves, mainImport, mainTemplate, mainClear;
    IconButton mainNewRhythm { IconButton::Icon::dice };
    bool showVolumes = false;
    void setMainTab (bool volumes);
    /** The button a menu opens from: the editor's while it is open, else the main window's. */
    juce::Component* menuTarget (juce::Component& inEditor, juce::Component& onMain) { return editOverlay.isVisible() ? &inEditor : &onMain; }
    void openEditor (int view = -1);

    // Export
    juce::ComboBox notesBox, lengthBox;
    DragTile dragTile;
    juce::TextButton saveButton { "SAVE .MID..." }, songButton { "FULL SONG..." }, wavButton { "SAVE .WAV..." };
    juce::TextButton linkButton { "LINK" };
    juce::TextButton keyLinkButton { "KEY" };   // the same key in the linked plug-ins (only while linked)   // the lead leads, a linked piano fills in (only while linked)
    std::unique_ptr<juce::FileChooser> chooser;

    SongOverlay songOverlay { processor };
    GrooveBrowser browser { processor };
    ProgressionBrowser progressionBrowser { processor };
    OptionsOverlay options;
    UpdateOverlay updateOverlay;
    Updater::Checker updateChecker;
    std::vector<ParamCombo*> combos;
    juce::Rectangle<int> tempoArea, logoArea, displayArea;
    std::array<float, fgl::numLanes> lights {};

    // Undo history: a snapshot twice a second; a change pushes the previous one.
    juce::StringArray undoStack, redoStack;
    juce::String lastSnapshot;
    int snapshotTicks = 0;
    int uiTicks = 0;
    // What the timer last showed (rebuilt only when it changes).
    int shownKey = -1, shownScale = -1, shownLanguage = -1, browserPattern = -1;
    juce::String shownKeyText, shownNotes, shownLength;

public:
    /** As in Fase Groove Piano: while a clip is dragged the window shrinks to a small strip, so
        the DAW's tracks are in view; it comes back when the clip is dropped. */
    void enterMiniMode() override;
    void exitMiniMode() override;
private:
    struct MiniStrip : public juce::Component
    {
        void paint (juce::Graphics&) override;
    };
    MiniStrip miniStrip;
    bool miniMode = false;
    juce::uint32 miniStartedAt = 0;
    juce::Point<int> fullSize;
    // Undo history: a snapshot only after the mouse has changed something (not polled).
    int pendingSnapshot = 0;
    void mouseUp (const juce::MouseEvent&) override { pendingSnapshot = 8; }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override { pendingSnapshot = 8; }

    juce::Rectangle<int> sizeBeforeFullScreen;
    juce::String message;
    int messageTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaseGrooveLeadEditor)
};
