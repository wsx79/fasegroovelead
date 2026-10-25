// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// On-the-fly material: random grooves by genre, turnaround variations and the
// song-section versions of a groove (intro / break / build-up / drop ...).

#include "Lead.h"

#include <array>
#include <cstdint>
#include <vector>

namespace fgl
{

//==============================================================================
enum class Section { main = 0, intro, breakdown, buildup, drop, outro };
constexpr int numSections = 6;
const char* sectionName (int section) noexcept;

/** Rewrites a groove for a song section. The lanes kept depend on the pattern's genre:
    funk favours ghost/pulse phrases, gospel hooks, dance low motifs and chill long notes.
    House keeps pulse, ghosts, answers and arpeggios. */
LeadPattern applySection (const LeadPattern& source, Section section);

/** Loudness of a section. */
double sectionVelocityScale (Section section) noexcept;
bool sectionAllowsFills (Section section) noexcept;

//==============================================================================
/** A brand new two-bar lead groove in the style of a genre (grooveCategories() index), from a
    seed. density 0..1 goes from sparse hooks to busy sixteenth-note phrases. Same seed = same groove. */
LeadPattern makeRandomPattern (std::uint32_t seed, float density, int genre);

/** A new two-bar line for one lane only, in the style of a genre (density as above). Never empty:
    a lane the genre rarely uses gets a light, fitting part. Same seed = same line. */
std::string makeRandomLane (Lane lane, std::uint32_t seed, float density, int genre);

/** A variation of a groove for the turnaround bar. New hits stay on the groove's grid
    (8ths or 16ths) and a lick or a scratch is added only when that genre plays one.
    Same seed = same variation. */
LeadPattern makeVariation (const LeadPattern& source, std::uint32_t seed);

/** A song section's own version of a groove: the same main chords, with the scratches, the
    up-repeats and the ghosts moved around a little. Same seed = same version. */
LeadPattern makeSectionVariation (const LeadPattern& source, std::uint32_t seed);

//==============================================================================
/** A groove is split into four equal parts, A B C D (half a bar each in a two-bar groove). */
constexpr int numSegments = 4;

/** Rebuilds the groove playing the parts in the given order (e.g. A A B C). */
LeadPattern applySegmentOrder (const LeadPattern& source, const std::array<int, numSegments>& order);

//==============================================================================
/** What the end of a fill bar plays. */
enum class FillStyle { motifBuild = 0, highRun, ghostBuild, pulseLift, dropOut, approachUp, mix,
                       noteStutter, melodyRun, arpeggioRun, lowRun, longOut, custom };
constexpr int numFillStyles = 13;
const char* fillStyleName (int style) noexcept;
/** First 16th (in the bar) a fill style takes over. */
int fillStartStep (FillStyle style) noexcept;

//==============================================================================
struct ArrangementPart
{
    Section section;
    int bars;
    std::uint32_t variation = 0;   // its own version of the groove (0: the groove as it is)
    int endFill = -1;              // fill on its last bar: -1 as set in the Song panel, -2 none, else a fill style
};

/** Intro 8 | Main 16 | Break 8 | Build-up 4 | Drop 16 | Outro 8 */
const std::vector<ArrangementPart>& defaultArrangement();

} // namespace fgl
