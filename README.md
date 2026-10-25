<p align="center"><img src="Resources/icon_512.png" alt="Fase Groove Lead" width="160"></p>

<h1 align="center">Fase Groove Lead</h1>

<p align="center">
  <b>House lead melodies for soulful, deep, funky &amp; jackin' house, ready to drag into your DAW.</b><br>
  by <b>Fabio 'Pitch' Serra</b> · VST3 / AU · Windows, macOS &amp; Linux · free and open source
</p>

<p align="center"><b>🇬🇧 English</b> · <a href="README.it.md">🇮🇹 Italiano</a></p>

<p align="center">
  <a href="docs/GUIDE.md"><b>📖 User guide</b></a> ·
  <a href="CHANGELOG.md">🆕 What's new</a> ·
  <a href="#download">⬇️ Download</a> ·
  <a href="#support-the-project">☕ Support the project</a> ·
  <a href="#credits">Credits</a> ·
  <a href="PRIVACY.md">Privacy</a> ·
  <a href="#license">License</a>
</p>

---

**Current version: [v0.1.0](https://github.com/wsx79/fasegroovelead/releases/tag/v0.1.0).** Shared buttons, menus, colours and layout follow the Fase Groove family.

**Fase Groove Lead** writes **monophonic lead melodies in MIDI**, alongside
[Fase Groove Piano](https://github.com/wsx79/fasegroovepiano),
[Drums](https://github.com/wsx79/fasegroovedrums) and
[Guitar](https://github.com/wsx79/fasegrooveguitar). Choose a groove or roll a new motif, set the
harmony, add swing, humanize, fills and variations, hear the internal synth, then **drag the MIDI
into your DAW** to play your own lead instrument. VST3 / AU and Standalone, with the family's **LINK**.

## Features

- 🎼 **13 house genres**, with preset grooves, random motifs and the thirteen coordinated `Arranger: …` styles.
- 🎹 **12 melodic lanes**: Hook, Answer, Staccato, Pulse, Ghost, Approach, Long note, Arpeggio, Run,
  High motif, Low motif and Tonic. Generated phrases stay monophonic; strong beats follow the harmony,
  passing notes follow the key and scale. The harmony browser shows a melody preview.
- 🎲 **Repeatable random generation**: density, seeds, per-lane and A–D dice, locks and part ordering.
- 🥁 **Feel**: swing, velocity, accents, humanize, a new feel, per-lane timing and audio/MIDI groove templates.
- ✏️ **Editing**: step grid, velocity, chance, repeats, octave, mute, lane level, mixer, custom fills and undo/redo.
- 🎵 **Song sections**: intro, main, break, build-up, drop and outro, with variations, fills and full-song export.
- 🔊 **Four guide synth sounds**: Soft sine, Warm triangle, Bright saw and Round pulse; Tone, Reverb,
  Volume, attack delay and optional external-instrument articulation keyswitches.
- 📤 **MIDI and WAV export**, drag to the DAW, per-lane tracks/stems, MIDI rhythm/harmony import and presets.
- 🔗 **LINK and KEY**: shared family genre, swing, tempo, transport, key and scale; MIDI input follows held harmony.
- 🌍 **Six interface languages**, scalable interface, full screen, tips and the shared verified updater.

The [user guide](docs/GUIDE.md) covers installation and every control, including the distinction
between MIDI dynamics and guide-audio volume, and the harmonic context used by the melody.

## Download

Builds are on the **[Releases](https://github.com/wsx79/fasegroovelead/releases/latest)** page.
Development builds are under **[Actions](https://github.com/wsx79/fasegroovelead/actions) → Build → Artifacts**.

| System | Package | Contents |
|---|---|---|
| Windows 10/11 (64-bit) | `FaseGrooveLead-Windows-Setup.exe` (installer, recommended) | VST3 + Standalone |
| Windows 10/11 (64-bit) | `FaseGrooveLead-Windows.zip` | VST3 + Standalone (manual install) |
| macOS 10.13+ (Intel & Apple Silicon) | `FaseGrooveLead-macOS.zip` | VST3 + AU + Standalone |
| Linux | `FaseGrooveLead-Linux.zip` | VST3 + Standalone |

To install, follow the [guide](docs/GUIDE.md#1-installation).

## Support the project

Fase Groove Lead is **free**. If you find it useful and want to support its development, you can
make a donation of any amount:

<p align="center">
  <a href="https://ko-fi.com/fabios1979"><b>☕ Ko-fi – ko-fi.com/fabios1979</b></a> ·
  <a href="https://paypal.me/fabios79"><b>💙 PayPal – paypal.me/fabios79</b></a>
</p>

The same links are in the plug-in: **ABOUT → DONATE: KO-FI / DONATE: PAYPAL**.

### Listen to my music

- 🎧 **Listen to my productions on Spotify:** [Fabio 'Pitch' Serra on Spotify](https://open.spotify.com/artist/29iQGuXML8LEw3xRp1dNRP)
- 🛒 **Download my tracks** from all the major digital music stores: search for **Fabio 'Pitch' Serra**.

## Credits

| | |
|---|---|
| **Idea, music direction and sound** | **Fabio 'Pitch' Serra** – DJ & Producer |
| **Code** | **Fase Groove contributors** – shared family code and product adaptations |
| **Framework** | [JUCE](https://juce.com) |

In the plug-in, open the credits with **ABOUT** (or click the logo at the top left).

## Privacy

The plug-in **collects no data**. Its optional GitHub update check connects to the internet (switch it off in OPTIONS → SETTINGS). See the [privacy policy](PRIVACY.md).

## License

Fase Groove Lead is **free software**: you can use, study, modify and redistribute it under the
terms of the **GNU Affero General Public License v3.0 or later** ([LICENSE](LICENSE)).

- you can use it **for free**, including for commercial music: the license covers the code, not
  the songs you make. The MIDI and the music you create are **yours**;
- if you distribute a modified version, you must publish its source code under the same license;
- the software is provided **"as is", without warranty**.

The name **Fase Groove Lead**, the app logo and the **FS PITCH** logo belong to
Fabio 'Pitch' Serra and are not covered by the code license. If you publish a modified version,
please use a different name and logo.

Third-party components (JUCE, VST 3 SDK, Inter font) and trademarks: see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
*VST is a registered trademark of Steinberg Media Technologies GmbH.*

---

## For developers

### Building from source

You need CMake ≥ 3.22 and a C++17 compiler (Visual Studio 2022 on Windows, Xcode on macOS).
JUCE 8 is downloaded automatically (or use a local copy with `-DJUCE_DIR=/path/to/JUCE`).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release        # engine tests + plug-in tests
```

The built plug-ins are in `build/FaseGrooveLead_artefacts/Release/`.

On Linux you also need: `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev
libxext-dev libfreetype-dev libfontconfig1-dev libxcomposite-dev`.

### Publishing a release

1. Add the new version at the top of [CHANGELOG.md](CHANGELOG.md) and
   [CHANGELOG.it.md](CHANGELOG.it.md), and update `VERSION` in `CMakeLists.txt`.
2. On GitHub open **Actions → Build → Run workflow** and enter the version in *release_tag*
   (e.g. `v0.2.0`), or push a tag: `git tag v0.2.0 && git push origin v0.2.0`.

macOS signing and notarization work as in Fase Groove Piano (repository secrets
`MACOS_CERTIFICATE`, `MACOS_CERTIFICATE_PASSWORD`, `MACOS_SIGN_IDENTITY`, `APPLE_ID`,
`APPLE_APP_PASSWORD`, `APPLE_TEAM_ID`); without them the build is signed ad hoc.

### Code layout

```
Source/engine/Lead.*        melodic lanes, tonal context, progressions and preset grooves (pure C++)
Source/engine/Generators.*    random grooves, variations, song sections, fill styles, arrangement
Source/engine/GrooveEngine.*  monophonic sequencer: motifs, swing, humanize and fills -> note events
Source/GuideLead.h          the guide lead synth (sine, triangle, saw, pulse)
Source/PluginProcessor.*      parameters, tempo sync, MIDI in (held chords) / out, clip export, user grid
Source/PluginEditor.*         user interface: grid, chord slots, views, song window, options
Source/Product.cpp            name, links and texts of this product for the shared module
ThirdParty/fasegroove-common/ shared family module (updater, translations, logo, theme, circle of fifths),
                              copied from the fasegroove repo by its scripts/sync.sh: edit it there
tests/EngineTests.cpp         engine tests (no JUCE dependency)
tests/PluginTests.cpp         plug-in tests: state, grid, chords, MIDI / audio export, transport, MIDI in
```

To add a groove, add a `groove (...)` line to `leadPatterns()` in `Lead.cpp`: one line per lane,
each character a 16th note: `X` accent, `x` hit, `g` ghost, `f` repeat, `.` rest. A one-bar line in a
two-bar groove plays twice. Add new grooves **at the end** of the list, so saved projects keep theirs.
To add a progression, add it at the end of `progressions()`.

Shared module provenance and adaptations: [ThirdParty/PROVENANCE.md](ThirdParty/PROVENANCE.md).
