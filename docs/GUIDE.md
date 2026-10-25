# Fase Groove Lead – User guide

*🇮🇹 [Guida in italiano](GUIDA.md)*

This guide covers [v0.1.0](https://github.com/wsx79/fasegroovelead/releases/tag/v0.1.0). Buttons, menus, colours and layout follow the other family instruments; the lanes are dedicated to solo melody.

**Arranger presets:** the thirteen `Arranger: …` styles use the shared family profiles. Choose the same genre in the other instruments and use LINK for tempo and transport; enable KEY to share key and scale.

## 1. Installation

**Windows:** run `FaseGrooveLead-Windows-Setup.exe` from the
[Releases](https://github.com/wsx79/fasegroovelead/releases/latest) page (or unzip the zip and copy
the files as below). **macOS / Linux:** download the zip for your system and unzip it.

| System | Copy | To |
|---|---|---|
| Windows | `Fase Groove Lead.vst3` | `C:\Program Files\Common Files\VST3\` |
| macOS | `Fase Groove Lead.vst3` / `Fase Groove Lead.component` | `~/Library/Audio/Plug-Ins/VST3/` / `~/Library/Audio/Plug-Ins/Components/` |
| Linux | `Fase Groove Lead.vst3` | `~/.vst3/` |

Then rescan the plug-ins in your DAW. On macOS, if the plug-in is blocked, run once in Terminal:
`xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Fase Groove Lead.vst3"` (and the same for the `.component`).
The **Standalone** app works without a DAW.

Load it as an **instrument** on a MIDI track. Press play in the DAW (or **PLAY** in the plug-in)
and the groove plays with the guide lead synth.

## 2. The window

The same layout as Fase Groove Piano, Drums and Guitar:

- **Header:** PRESETS, TOTAL (everything random), undo / redo, full screen, STYLE, RESET, OPTIONS;
  TRANSPORT & TEMPO (⏮ ▶ ■, SYNC DAW, the tempo, LINK, Sequencer); the display (groove, genre,
  the chord now, the section, where the playhead is).
- **GROOVE:** the groove, the genre of the dice, density, part length and the parts A-D.
- **LEAD PART:** the row picked on the grid. **CHORDS:** key, progression, the four chords.
- **GROOVE FEEL & SONG**, **GUIDE SOUND & MIDI OUT**.
- The big **grid** (or VELOCITY, MIXER, FILL) and **EXPORT MIDI & AUDIO**.

## 3. The groove

- **GROOVE:** 43 grooves in 13 genres (the same as Fase Groove Piano and Drums); click its name for
  the browser (genres on the left, each groove with a picture of its rhythm). **Random groove** is
  the one of NEW GROOVE, **User pattern** the one edited on the grid.
- **RANDOM / GROOVE (dice):** a new random groove in the genre chosen in **GROOVE GENRE**.
  **DENSITY** goes from sparse hooks to busy sixteenth-note phrases.
- **Parts A-D:** the groove in four parts (half a bar each, or one bar with **Part length: 1 bar**);
  play them in any order (A A A B...), roll one with its die, lock it so the dice leave it alone.
- **STYLE** (top): a genre's groove, chords, swing, feel, fills and sound in one click. **RESET**
  puts everything back to the default.

## 4. Chords

- **Key** (the menu on the left): the chord slots move with it. The harmony guides the melody;
  it does not output chord accompaniment. Strong beats resolve on chord tones; passing notes use
  the selected key and scale. The seed chooses a repeatable two-bar motif and its answer.
- **PROGRESSION...:** the family progression browser. Click a progression to load its chords in
  the key. On the right, a **melody preview**, followed by key, **scale** (13), **colour** (triads,
  7ths, 9ths) and **chords** (1–8, one per bar). **NEW HARMONY** makes another progression for the
  chosen groove genre. The die beside PROGRESSION... does the same from the main window.
- **Chord slots:** one chord per bar, round and round. Click a slot to change root or type, copy
  another bar's chord or apply it to every bar. The current slot lights up. The browser also lets
  you choose how many slots play; chord types include triads, sevenths, ninths, suspended,
  diminished and augmented chords.
- **MIDI IN** (on by default): held MIDI input notes supply the harmony instead of the slots.
  Play a chord on your keyboard or route **Fase Groove Piano's MIDI out** into Lead's track:
  the melody follows that harmony with its own rhythm. A single held note supplies root/fifth
  context. With MIDI IN off, incoming MIDI passes through and plays the guide synth.

**Global octave** transposes all lead lanes together (−2 to +2), relative to each lane’s octave. It is saved in presets and can be automated in the DAW.

## 5. The grid

12 phrase lanes × 32 steps (two bars of 16ths, or 64 steps with one-bar parts):

| Row | What it plays |
|---|---|
| **HOOK** | the main motif, chosen by the seed |
| **ANSWER** | an answering contour |
| **SHORT** (Staccato) | short notes on a compact contour |
| **PULSE** | alternating notes for a rhythmic pulse |
| **GHOST** | very short, soft pitched notes between the main attacks |
| **APPROACH** | a semitone approach into the target note |
| **LONG** | a sustained melodic note |
| **ARP** | one chord tone at a time |
| **RUN** | a moving melodic line |
| **HIGH** | a motif in the upper register |
| **LOW** | a lower motif |
| **TONIC** | the current harmony's root |

All lanes belong to **one monophonic lead voice**. A new generated note closes the previous
pitch; simultaneous fill attacks resolve to the strongest one. Ghosts have a pitch, arpeggios
play single notes, and HIGH/LOW never add a second accompaniment voice.

| Action | Result |
|---|---|
| Click | hit (click again: rest) |
| Shift + click | accent |
| Alt + click | ghost (very soft) |
| Ctrl / Cmd + click | **REPEAT**: two quick attacks |
| Drag | paints |
| Right-click a step | chance, roll (2–4 repeats), symbol and move tools |
| Click a row's name | hear it and select it in LEAD PART |
| Double-click a row's name | its velocities |
| Right-click a row's name | mute, a new line, clear, octave, level |
| A row's die | a new line for that row only |

The first edit of a preset turns it into the **User pattern**; the preset stays unchanged.

## 6. Lead part

The selected lane: **MUTE**, its die, **OCTAVE**, **HEAR IT**, **LEVEL** (MIDI velocity) and
**VOLUME** (guide audio only). **PLAYS** shows the melodic pitch for the current context.
Changing a lane's octave changes its pitches; Level changes MIDI and guide dynamics, while
Volume changes only its contribution to the guide sound. The main **VOLUMES** view offers the
same audio faders; open **EDITOR...** for the full grid, velocity, mixer and fill views.

## 7. Feel

- **SWING:** delays alternate 16ths: 50% straight, 67% triplet shuffle, 75% the heaviest.
  Each genre brings the same swing convention as the other family instruments.
- **HUMANIZE:** small timing and velocity changes; softer notes come and go over the phrase.
  **NEW FEEL** makes another repeatable humanization, saved with the project.
- **VELOCITY:** overall attack level. **ACCENTS:** emphasises offbeat answers, pulses, ghosts
  and arpeggios and softens the other 16ths (0%: flat).
- **MIXER:** each lane's push/pull in milliseconds and its share of swing. **TIMING...** offers
  ready-made feels, including laid-back, pushed, jackin' and neo soul timing.
- **TEMPLATE...:** applies the timing and accents of an audio loop or MIDI file to the current
  groove. Drop an audio loop on the window to use it as a template.

## 8. Song

- **Fills:** off, or every 1, 2, 4 or 8 bars. Styles include **Motif build**, **High run**,
  **Ghost build**, **Pulse lift**, **Drop out**, **Approach up**, **Staccato stutter**,
  **Melodic run**, **Arpeggio run**, **Low run**, **Long note**, **Mix** and **Custom**.
  Draw your own one-bar fill in the FILL view. Fills remain monophonic.
- **RESOLVE:** a hook attack on the bar after a fill, and every eight bars of the drop.
- **Variation:** every four or eight bars the final bar changes, with pickups, runs or pauses.
- **Song section:** sparse Intro / Outro, sustained Break, growing Build-up, full Main / Drop.
- **FULL SONG...:** set bars, a variation and a closing fill for each section. Drag or save the
  complete song with markers, a single section, or every section as its own clip.

## 9. Sound & MIDI

- **Guide sound:** the internal lead synth: **Soft sine**, **Warm triangle**, **Bright saw** and
  **Round pulse**, with **TONE**, **REVERB** and **VOLUME**. Switch it off for MIDI-only use with
  your own lead instrument. Tone controls brightness; the four waveforms retain their own sound.
- **ATTACK DELAY:** phrase attack delay in milliseconds. It also sets the approach length on
  APPROACH notes. The timing is included in exported MIDI.
- **MIDI CH.:** output note channel, 1 by default.
- **Sequencer:** off, the groove does not follow DAW playback (useful when the exported clip is
  already playing on the track). The plug-in's play button still previews the groove.
- **Standalone:** choose the audio device and MIDI ports in JUCE's audio/MIDI settings. With no
  DAW, use the internal tempo, LINK, or incoming MIDI clock for transport and timing.

## 10. Export and import

- **DRAG MIDI TO DAW:** drag the orange tile to a lead instrument track. **Notes:** all lanes in
  one track, or one track per lane. **Clip length:** the full repeating loop, 1–16 bars, or the
  full song. Full loop includes harmony, rhythm, fills and variations.
- **SAVE .MID...:** save the same MIDI clip to a chosen file. **SAVE .WAV...:** export the guide
  audio as a seamless loop, complete song or per-lane stems.
- **IMPORT...** (or drop a `.mid` on the window): imports its rhythm into suitable lanes and
  estimates the harmony of its first bars for the slots. It is a groove/harmony import, not an
  exact transcription: Lead generates pitches from the current motif, scale and harmony.
- **MY GROOVES:** save the grid as your own groove and load it in any project.
  **PRESETS:** save the complete setup. The DAW also saves the setup with its project.
- Dragged clips, presets and grooves live under **Documents/Fase Groove Lead/**. A failed file
  save preserves the previous file; exported MIDI closes all notes at the clip end.

## 11. LINK

**LINK**, in the transport, connects Lead with **Fase Groove Piano**, **Drums**, **Guitar** and
other instances: genre, swing, tempo, DAW sync and transport use the shared family protocol.
Change a shared setting in any linked instrument. **KEY**, enabled in the linked instruments,
shares key and scale too. To follow the piano's changing chords, route its MIDI output to Lead
and enable MIDI IN (section 4). LINK alone shares tonal settings, not the chord-note stream.

## 12. Shortcuts and options

- **Space:** play / pause. **Ctrl+Z / Ctrl+Shift+Z (Ctrl+Y):** undo / redo.
- **OPTIONS** (or a click on the logo): language (English, Italiano, Español, Français, Deutsch,
  Português), interface size, tips, automatic updates; **ABOUT**: credits and links.

## 13. Playing with the family

- **KEY**, beside Sequencer, shows key and scale. With LINK and KEY on in the instruments,
  changing one updates the others. Relative major/minor key changes retain the existing chords.
- Leave Piano, Guitar and Drums to their accompaniment parts and use Lead for the melodic line.
  Lead does not enable Guitar's piano-ducking mode.
- **PLAYING...**, in GUIDE SOUND & MIDI OUT:
  - *Hold hooks until the next lead note*: extends hook gates; the following note still closes
    the preceding pitch.
  - *Upper register*: raises the melody one octave to leave room for bass and piano.
  - *Push chord changes*: the last eighth-note region can use the next bar's harmony.
  - *Articulation keyswitches and expression swells*: optional external-instrument switches
    (Normal, Short, Ghost, Approach) and CC 11 swells. Choose each keyswitch note in the menu;
    configure the same mapping in the receiving instrument. Keyswitch notes are not played by
    the guide synth and can appear separately from melodic notes in MIDI.
- Use **LEVEL** for the dynamics sent to the receiving instrument and **VOLUME** for the internal
  guide mix. Save a preset when the lead and accompaniment sit together as intended.
