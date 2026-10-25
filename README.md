# Fase Groove Lead

Monophonic lead melodies for the Fase Groove family. VST3 and standalone on Windows, macOS and Linux; AU on macOS. Built from Fase Groove Guitar's editor, transport, pattern tools, import/export and preset infrastructure, with the canonical `fasegroove/common` module vendored unchanged.

Choose a genre and press the groove dice for a repeatable two-bar melody. Density controls the rhythm; the riff seed chooses a melodic motif. Hook, Answer, Staccato, Pulse, Ghost, Approach, Long note, Arpeggio, Run, High motif, Low motif and Tonic are alternative phrase lanes of one monophonic voice. Concurrent lanes are resolved by the family engine; each new note releases the previous pitch. Harmony slots guide strong-beat resolutions, while passing notes follow Key and Scale. They never output chord accompaniment. The harmony browser shows the resulting melody instead of a circle of fifths.

The original family controls remain functional: editable steps, velocity, chance, rolls, mute, octave, lane level and audio volume, swing, humanize, accents, per-lane timing, A–D reordering, sections, fills, variations, arrangement, presets, undo/redo, MIDI import, groove templates, MIDI/WAV export and drag into a DAW. Attack delay sets phrase timing and approach length; Hold hooks extends their gate until the next note. Upper register raises the melody to leave room for bass and piano. Optional articulation keyswitches target external instruments and are excluded from the guide synth. Four guide sounds: Soft sine, Warm triangle, Bright saw and Round pulse, with Tone, Reverb and Volume.

LINK shares genre, swing, tempo and transport with the other family instruments; KEY shares tonal context. This instrument does not activate Guitar's piano-ducking protocol. With **Chords from MIDI in** enabled, held input notes supply harmonic context. Disable it to pass MIDI input through and play the guide manually.

## Build

CMake 3.22+, C++17 and JUCE 8.0.9 (downloaded automatically or `-DJUCE_DIR=/path/to/JUCE`). Linux dependencies match the other family plugins: ALSA, X11/Xrandr/Xinerama/Xcursor/Xcomposite, FreeType and Fontconfig development packages.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

Engine-only build: add `-DFGL_BUILD_PLUGIN=OFF`. Outputs are in `build/FaseGrooveLead_artefacts/Release/`. The GitHub Actions workflow builds and packages Windows, macOS and Linux. Installer scripts are in `installer/`.

## Shared code

`ThirdParty/fasegroove-common/` is copied byte for byte from [fasegroove](https://github.com/wsx79/fasegroove). Update it from a sibling checkout:

```sh
../fasegroove/scripts/sync.sh "$PWD"
python3 scripts/check-shared.py ../fasegroove
```

Source versions and adaptations are recorded in `ThirdParty/PROVENANCE.md`. AGPL-3.0-or-later; see LICENSE and THIRD_PARTY_NOTICES.md.
