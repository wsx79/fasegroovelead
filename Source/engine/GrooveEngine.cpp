// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "GrooveEngine.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fgl
{

namespace
{
    long long floorDiv (long long a, long long b) noexcept
    {
        long long q = a / b;
        if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
        return q;
    }

    int mod (long long a, long long m) noexcept { return (int) (((a % m) + m) % m); }

    /** Deterministic 0..1 noise, so re-rendering a step always gives the same feel. */
    double noise (long long step, int salt) noexcept
    {
        auto x = (std::uint64_t) (step * 0x9E3779B97F4A7C15LL) ^ ((std::uint64_t) salt * 0xBF58476D1CE4E5B9ULL);
        x ^= x >> 31; x *= 0x94D049BB133111EBULL; x ^= x >> 29;
        return (double) (x >> 11) * (1.0 / 9007199254740992.0);
    }

    double symbolLevel (char symbol) noexcept
    {
        switch (symbol)
        {
            case 'X': return 1.0;
            case 'f': return 0.9;
            case 'g': return 0.42;
            case 'x':
            default:  return 0.8;
        }
    }

    /** How much a lane drifts with Humanize: the low strings stay with the kick, the strumming
        hand drifts more. */
    double laneLooseness (int lane) noexcept
    {
        switch ((Lane) lane)
        {
            case Lane::root:
            case Lane::power:     return 0.3;
            case Lane::hook:
            case Lane::stab:      return 0.6;
            case Lane::scratch:
            case Lane::answer:
            case Lane::chank:     return 1.1;
            default:              return 0.9;
        }
    }

    /** How long a hit rings, in 16ths. */
    double noteLength (int lane) noexcept
    {
        switch ((Lane) lane)
        {
            case Lane::hook: return 1.8;
            case Lane::answer:   return 0.9;
            case Lane::stab:      return 0.45;
            case Lane::chank:     return 0.4;
            case Lane::scratch:   return 0.22;
            case Lane::slide:     return 1.6;
            case Lane::swell:     return 15.5;
            case Lane::arp:       return 1.0;
            case Lane::lick:      return 0.9;
            case Lane::octave:    return 0.85;
            case Lane::power:     return 1.6;
            case Lane::root:
            default:              return 1.8;
        }
    }

    /** A lane's own loudness: the muted scratch is a whisper under the chords. */
    double laneLevel (int lane) noexcept
    {
        switch ((Lane) lane)
        {
            case Lane::scratch: return 0.72;
            case Lane::swell:   return 0.8;
            case Lane::answer: return 0.88;
            default:            return 1.0;
        }
    }

    int lcm (int a, int b) noexcept { return a / std::gcd (a, b) * b; }

    /** The level an up-strum, a scratch or a chank has at each 16th of the beat, as funk
        soloists play it: the offbeat 8th sings, the 16ths around it are softer. */
    double accentProfile (int lane, int stepInBeat) noexcept
    {
        static constexpr double repeats[] { 0.75, 0.45, 1.0, 0.55 };
        static constexpr double picking[] { 0.9, 0.6, 1.0, 0.7 };
        return (lane == (int) Lane::arp ? picking : repeats)[stepInBeat & 3];
    }

    /** Whether a lane keeps playing the groove under a fill. */
    bool fillKeeps (FillStyle style, int lane, const std::array<bool, numLanes>& customLanes) noexcept
    {
        const auto l = (Lane) lane;
        const bool low = l == Lane::root || l == Lane::power;
        switch (style)
        {
            case FillStyle::dropOut:
            case FillStyle::longOut:    return false;
            case FillStyle::lowRun:    return isChordLane (lane);
            case FillStyle::pulseLift:   return low || l == Lane::scratch;
            case FillStyle::melodyRun:
            case FillStyle::arpeggioRun:      return low || l == Lane::scratch;
            case FillStyle::custom:      return ! customLanes[(size_t) lane];
            default:                     return low;   // the low strings keep the floor
        }
    }

    bool followsAccentGroove (int lane) noexcept
    {
        switch ((Lane) lane)
        {
            case Lane::answer: case Lane::scratch: case Lane::chank: case Lane::arp: return true;
            default: return false;
        }
    }

    /** How a lane is played, for a sampled lead's keyswitches: the scratch and the ghost
        repeats dead, the chank palm muted, the slide sliding, the rest let ring. */
    int articulationOf (int lane, char symbol) noexcept
    {
        switch ((Lane) lane)
        {
            case Lane::scratch:   return (int) Articulation::dead;
            case Lane::chank:     return (int) Articulation::palmMute;
            case Lane::slide:     return (int) Articulation::slide;
            case Lane::hook:
            case Lane::answer:   return symbol == 'g' ? (int) Articulation::dead : (int) Articulation::sustain;
            default:              return (int) Articulation::sustain;
        }
    }

    /** Whether a lane's strum keeps ringing until the next chord hit (Let ring). */
    bool ringsOn (int lane) noexcept
    {
        return lane == (int) Lane::hook || lane == (int) Lane::answer || lane == (int) Lane::power;
    }

    /** The strings of a hit sound one after the other, as the pick crosses them: low to high on
        a down strum, high to low on an up strum and a scratch. One string: no strum. */
    bool repeatsUp (int lane, int stepInBar) noexcept
    {
        if (lane == (int) Lane::answer)
            return true;
        if (lane == (int) Lane::scratch || lane == (int) Lane::chank)
            return stepInBar % 2 == 1;   // the hand goes up on the odd 16ths
        return false;
    }
}

const char* articulationName (int articulation) noexcept
{
    static const char* names[] { "Normal", "Short", "Ghost", "Approach" };
    return articulation >= 0 && articulation < numArticulations ? names[articulation] : "";
}

LeadSettings::LeadSettings() noexcept
{
    for (int l = 0; l < numLanes; ++l)
    {
        levels[(size_t) l] = 1.0f;
        laneSwing[(size_t) l] = 1.0f;
        user[(size_t) l].fill ('.');
        userVelocity[(size_t) l].fill (0);
        userChance[(size_t) l].fill (0);
        userRoll[(size_t) l].fill (0);
        customFill[(size_t) l].fill ('.');
    }
    static const auto first = progressionChords (genreProgression (0), 9);   // (worked out once: no allocation later)
    numChords = (int) first.size();
    for (size_t i = 0; i < chords.size(); ++i)
        chords[i] = first[i % first.size()];
    grooveTemplate.level.fill (1.0f);
}

//==============================================================================
GrooveEngine::GrooveEngine()
{
    offTime.fill (-1.0);
    onTime.fill (0.0);
    laneOf.fill (0);
    graceOf.fill (false);
    pending.reserve (1024);
    scratch.reserve (128);
}

ChordTones GrooveEngine::chordAt (const LeadSettings& s, long long bar) noexcept
{
    if (s.useLiveChord && s.liveChord.mask != 0)
        return s.liveChord;
    return tonesOf (s.chords[(size_t) mod (bar, std::clamp (s.numChords, 1, maxChords))]);
}

int GrooveEngine::chordPeriod (const LeadSettings& s) noexcept
{
    const int n = std::clamp (s.numChords, 1, maxChords);
    for (int period = 1; period < n; ++period)
        if (n % period == 0)
        {
            bool repeats = true;
            for (int i = period; i < n && repeats; ++i)
                repeats = s.chords[(size_t) i] == s.chords[(size_t) (i % period)];
            if (repeats)
                return period;
        }
    return n;
}

const ChordVoicing& GrooveEngine::voicingAt (const LeadSettings& s, long long bar)
{
    if (s.useLiveChord && s.liveChord.mask != 0)
    {
        // The chord held on the MIDI input: its shapes move from the last chord held.
        const auto colour = s.pattern == randomPatternIndex() ? leadColourFor (s.randomGenre) : LeadColour::classic;
        if (! (s.liveChord == liveVoiced) || liveBand != s.roomForBand || liveColour != (int) colour)
        {
            const bool near = liveVoiced.mask != 0 && liveBand == s.roomForBand && liveColour == (int) colour;
            const auto v = voiceChord (s.liveChord, near ? &liveVoicing : nullptr, s.roomForBand, colour);
            liveVoicing = v;
            liveVoiced = s.liveChord;
            liveBand = s.roomForBand;
            liveColour = (int) colour;
        }
        return liveVoicing;
    }

    const int n = std::clamp (s.numChords, 1, maxChords);
    const int period = chordPeriod (s);
    const auto colour = s.pattern == randomPatternIndex() ? leadColourFor (s.randomGenre) : LeadColour::classic;
    bool same = n == voicedCount && voicedBand == s.roomForBand && voicedColour == (int) colour;
    for (int i = 0; i < n && same; ++i)
        same = s.chords[(size_t) i] == voicedChords[(size_t) i];
    if (! same)
    {
        // Round the progression twice: the first chord's shapes then come from the last one's,
        // so the loop joins up as smoothly as the rest.
        slotVoicings[0] = voiceChord (tonesOf (s.chords[0]), nullptr, s.roomForBand, colour);
        if (period > 1)
            for (int pass = 0; pass < 2; ++pass)
                for (int i = pass == 0 ? 1 : 0; i < period; ++i)
                {
                    const auto v = voiceChord (tonesOf (s.chords[(size_t) i]), &slotVoicings[(size_t) mod (i - 1, period)], s.roomForBand, colour);
                    slotVoicings[(size_t) i] = v;
                }
        voicedChords = s.chords;
        voicedCount = n;
        voicedBand = s.roomForBand;
        voicedColour = (int) colour;
    }
    return slotVoicings[(size_t) (mod (bar, n) % period)];
}

LeadPattern GrooveEngine::basePattern (const LeadSettings& s)
{
    const auto& presets = leadPatterns();

    const int length = patternLength (s);

    if (s.pattern == randomPatternIndex())
    {
        auto p = makeRandomPattern (s.riffSeed, s.density, s.randomGenre);
        fitPattern (p, length);
        if (length > randomBeatLength)   // four bars: the last one turns around
            p = makeVariation (p, s.riffSeed + 31u);
        return p;
    }

    if (s.pattern == userPatternIndex())
    {
        LeadPattern p;
        p.name = "User pattern";
        for (int l = 0; l < numLanes; ++l)
            p.lanes[(size_t) l].assign (s.user[(size_t) l].begin(), s.user[(size_t) l].end());
        fitPattern (p, length);
        return p;
    }

    // Every groove plays as two (or four) bars, a one-bar groove repeated: A-D are always a
    // quarter of it.
    auto p = presets[(size_t) std::clamp (s.pattern, 0, (int) presets.size() - 1)];
    fitPattern (p, length);
    return p;
}

LeadPattern GrooveEngine::resolvePattern (const LeadSettings& s)
{
    auto p = applySegmentOrder (basePattern (s), s.segmentOrder);
    if (s.sectionVariation != 0)
        p = makeSectionVariation (p, s.sectionVariation);
    return applySection (p, s.section);
}

int GrooveEngine::patternLength (const LeadSettings& s) noexcept
{
    return s.grooveBars >= 4 ? 64 : 32;
}

int GrooveEngine::loopLengthInSteps (const LeadSettings& s) noexcept
{
    int steps = patternLength (s);
    if (! s.useLiveChord)
    {
        // The chords come round after numChords bars (or fewer, when they repeat inside).
        steps = lcm (steps, chordPeriod (s) * 16);
    }
    if (s.variationEvery > 0)
        steps = lcm (steps, s.variationEvery * 16);
    if (s.fillEveryBars > 0)
        steps = lcm (steps, s.fillEveryBars * 16);
    return steps;
}

FillStyle GrooveEngine::effectiveFillStyle (FillStyle style, long long bar, int genre) noexcept
{
    if (style != FillStyle::mix)
        return style;

    // Mix: a different fill now and then, always the same one on the same bar.
    // The list is the weight: a style named twice is twice as likely.
    static constexpr FillStyle house[] { FillStyle::motifBuild, FillStyle::approachUp, FillStyle::ghostBuild, FillStyle::pulseLift,
                                         FillStyle::motifBuild, FillStyle::melodyRun, FillStyle::noteStutter, FillStyle::highRun };
    static constexpr FillStyle funk[]  { FillStyle::ghostBuild, FillStyle::pulseLift, FillStyle::ghostBuild, FillStyle::noteStutter,
                                         FillStyle::pulseLift, FillStyle::approachUp };
    static constexpr FillStyle gospel[] { FillStyle::motifBuild, FillStyle::motifBuild, FillStyle::longOut, FillStyle::highRun };
    static constexpr FillStyle dance[] { FillStyle::motifBuild, FillStyle::highRun, FillStyle::noteStutter, FillStyle::dropOut,
                                         FillStyle::motifBuild, FillStyle::highRun };
    static constexpr FillStyle afro[]  { FillStyle::arpeggioRun, FillStyle::lowRun, FillStyle::pulseLift, FillStyle::arpeggioRun, FillStyle::lowRun };
    static constexpr FillStyle neo[]   { FillStyle::melodyRun, FillStyle::longOut, FillStyle::arpeggioRun, FillStyle::melodyRun, FillStyle::longOut };
    static constexpr FillStyle pad[]   { FillStyle::longOut, FillStyle::arpeggioRun, FillStyle::dropOut, FillStyle::longOut };
    const FillStyle* choices = house;
    size_t n = std::size (house);
    switch (genre)
    {
        case 4: case 8:  choices = funk;   n = std::size (funk); break;     // jackin', funky
        case 3:          choices = gospel; n = std::size (gospel); break;
        case 12:         choices = dance;  n = std::size (dance); break;
        case 7:          choices = afro;   n = std::size (afro); break;
        case 10: case 11: choices = neo;   n = std::size (neo); break;      // neo-soul, chill
        case 2:          choices = pad;    n = std::size (pad); break;      // deep
        default: break;
    }
    return choices[(size_t) (noise (bar, 4242) * (double) n) % n];
}

bool GrooveEngine::samePattern (const LeadSettings& a, const LeadSettings& b) noexcept
{
    const bool usesSeed = a.pattern == randomPatternIndex();
    const bool usesUser = a.pattern == userPatternIndex();
    return a.pattern == b.pattern && a.section == b.section && a.segmentOrder == b.segmentOrder
        && a.sectionVariation == b.sectionVariation && a.grooveBars == b.grooveBars
        && a.riffSeed == b.riffSeed   // (the variation bar takes it too)
        && (! usesSeed || (a.density == b.density && a.randomGenre == b.randomGenre))
        && (! usesUser || a.user == b.user);
}

void GrooveEngine::prepare (Prepared& r, const LeadSettings& s)
{
    r.key = s;
    r.pattern = resolvePattern (s);
    r.variation = makeVariation (r.pattern, (std::uint32_t) std::max (0, s.pattern) * 7919u + s.riffSeed);
}

const GrooveEngine::Prepared& GrooveEngine::patternFor (const LeadSettings& s)
{
    if (prepared != nullptr && samePattern (prepared->key, s))
        return *prepared;
    if (! cacheValid || ! samePattern (cache.key, s))
    {
        prepare (cache, s);
        cacheValid = true;
    }
    return cache;
}

//==============================================================================
void GrooveEngine::collectStep (long long step, const LeadSettings& s, const LeadPattern& normal, const LeadPattern& variation)
{
    const long long bar = floorDiv (step, 16);
    const int stepInBar = mod (step, 16);
    const auto chord = chordAt (s, bar);
    const ChordVoicing& voicing = voicingAt (s, bar);
    // The chord of the next bar: the last 8th of a bar can push into it.
    const auto nextChord = chordAt (s, bar + 1);
    const ChordVoicing& nextVoicing = voicingAt (s, bar + 1);

    // Every N bars the last bar plays a variation of the groove (the turnaround).
    const bool varied = s.variationEvery > 0 && mod (bar, s.variationEvery) == s.variationEvery - 1
                     && variation.length() == normal.length();
    const LeadPattern& pattern = varied ? variation : normal;
    const int len = std::max (1, pattern.length());
    const int pos = mod (step, len);

    const auto& groove = s.grooveTemplate;
    const double swingDelay = groove.active ? (double) groove.timing[(size_t) stepInBar] * stepLength
                            : (step % 2 != 0) ? std::clamp (s.swing, 0.0f, 1.5f) * stepLength / 3.0 : 0.0;
    const double human = std::clamp (s.humanize, 0.0f, 1.0f);
    const int seedSalt = (int) (s.humanizeSeed % 100003u) * 7919;
    const double beatsPerMs = std::clamp (s.tempo, 20.0, 400.0) / 60000.0;

    // Fills: the end of every N bars (or a fill asked for on one bar).
    auto isFillBar = [&s] (long long b)
    {
        if (s.forcedFillBar >= 0)
            return b == s.forcedFillBar && s.forcedFillStyle >= 0;
        return s.fillEveryBars > 0 && sectionAllowsFills (s.section) && mod (b, s.fillEveryBars) == s.fillEveryBars - 1;
    };
    const bool fillBar = isFillBar (bar);
    const int fillGenre = categoryIndex (normal.category) >= 0 ? categoryIndex (normal.category) : s.randomGenre;
    const FillStyle style = (s.forcedFillBar >= 0 && bar == s.forcedFillBar && s.forcedFillStyle >= 0)
                                ? effectiveFillStyle ((FillStyle) std::clamp (s.forcedFillStyle, 0, numFillStyles - 1), bar, fillGenre)
                                : effectiveFillStyle (s.fillStyle, bar, fillGenre);
    // The custom fill takes over from its first written step.
    int fillStart = fillStartStep (style);
    std::array<bool, numLanes> customLanes {};
    if (style == FillStyle::custom)
    {
        fillStart = 16;
        for (int l = 0; l < numLanes; ++l)
            for (int i = 0; i < 16; ++i)
                if (isHit (s.customFill[(size_t) l][(size_t) i]))
                {
                    customLanes[(size_t) l] = true;
                    fillStart = std::min (fillStart, i);
                }
    }
    const bool inFill = fillBar && stepInBar >= fillStart;

    // A velocity set by hand on this step of the user pattern (stored per step as written,
    // before the A-D order). It stays on the variation bar and under a fill, except on the hits
    // they change. The same goes for the chance and the roll set on a step.
    auto userIndex = [&] (int lane) -> int
    {
        if (s.pattern != userPatternIndex() || len != patternLength (s))
            return -1;
        if (varied && variation.lanes[(size_t) lane][(size_t) pos] != normal.lanes[(size_t) lane][(size_t) pos])
            return -1;
        const int segment = len / numSegments;
        return std::clamp (s.segmentOrder[(size_t) (pos / segment)], 0, numSegments - 1) * segment + pos % segment;
    };
    auto handVelocity = [&] (int lane) -> int
    {
        const int index = userIndex (lane);
        return index < 0 ? 0 : s.userVelocity[(size_t) lane][(size_t) index];
    };

    auto velocityFor = [&] (int lane, double level, int salt)
    {
        const double sectionGain = sectionVelocityScale (s.section)
            * (s.section == Section::buildup ? 0.65 + 0.45 * (double) pos / (double) std::max (1, len - 1) : 1.0);
        const double laneGain = (double) s.levels[(size_t) lane] * laneLevel (lane);
        double v = s.velocity * level * laneGain * sectionGain;
        v += human * 12.0 * (noise (step, seedSalt + salt + 7) - 0.5) * 2.0;
        // Floor follows the Velocity knob, the lane and the section. A speaking hit cannot be
        // pulled to nothing by Humanize; a ghost (level under 0.55) still sits well below it.
        int floorVel = 1;
        if (level >= 0.85)
            floorVel = (int) std::lround (s.velocity * laneGain * sectionGain * 0.80);
        else if (level >= 0.55)
            floorVel = (int) std::lround (s.velocity * laneGain * sectionGain * 0.48);
        return std::clamp ((int) std::lround (v), std::clamp (floorVel, 1, 127), 127);
    };

    auto timeFor = [&] (int lane, int salt)
    {
        // Each lane takes its share of the swing and sits ahead of or behind the beat; humanize
        // only ever delays a hit a little (a player sits on or behind the grid).
        const double shift = std::clamp ((double) s.laneShift[(size_t) lane], -40.0, 40.0) * beatsPerMs;
        const double t = (double) step * stepLength + swingDelay * std::clamp ((double) s.laneSwing[(size_t) lane], 0.0, 2.0) + shift
                       + human * laneLooseness (lane) * 0.025 * noise (step, seedSalt + salt);
        return step >= 0 ? std::max (t, 0.0) : t;
    };

    // Life (with Humanize): over a four-bar phrase the soft notes come and go, as a soloist
    // plays a loop: ghosts, the scratches and up-repeats between the offbeats. The phrase repeats
    // (NEW FEEL makes another). The accents and the downbeats never change.
    const int phraseBar = mod (bar, 4);
    auto isSoft = [&] (int lane, char symbol)
    {
        if (symbol == 'X')
            return false;
        if (symbol == 'g')
            return true;
        return followsAccentGroove (lane) && stepInBar % 2 == 1;
    };
    auto lifeNoise = [&] (int lane, int salt) { return noise (phraseBar * 16 + stepInBar, seedSalt + lane * 101 + salt); };

    auto add = [&] (int lane, char symbol, int transpose = 0, double levelScale = 1.0, bool fromFill = false, int roll = 1)
    {
        if (s.muted[(size_t) lane] || ! isHit (symbol))
            return;
        if (! fromFill && human > 0.0 && isSoft (lane, symbol) && lifeNoise (lane, 77) < 0.4 * human)
            return;   // not in this bar of the phrase
        const int salt = lane * 13 + 1;
        if (! fromFill)
            if (const int index = userIndex (lane); index >= 0)
            {
                // A step that only plays now and then (the same way every time it comes round).
                const int chance = s.userChance[(size_t) lane][(size_t) index];
                if (chance > 0 && chance < 100 && noise (step, seedSalt + salt + 31) * 100.0 >= chance)
                    return;
                roll = std::max (roll, (int) s.userRoll[(size_t) lane][(size_t) index]);
            }
        const double t = timeFor (lane, salt) + std::clamp ((double) s.attackMs, 0.0, 40.0) * beatsPerMs;
        double level = symbolLevel (symbol) * levelScale;
        if (groove.active)
            level *= std::clamp ((double) groove.level[(size_t) stepInBar], 0.2, 1.6);
        if (const int set = fromFill ? 0 : handVelocity (lane); set > 0)
            level = set / 100.0;   // 100 = as the Velocity control
        else if (followsAccentGroove (lane) && symbol != 'g' && ! groove.active)
        {
            // The accent groove: written accents stay on top, the rest follows the beat.
            // The profile bottoms at 0.45, which used to leave a plain 16th near half level:
            // a groove made only of those 16ths was silent. The floor keeps a plain hit
            // speaking; the "&" (profile 1) stays the loudest, and ghosts never come here.
            const double a = std::clamp ((double) s.accentGroove, 0.0, 1.0);
            const double shape = 1.0 - a * (1.0 - accentProfile (lane, stepInBar % 4));
            const double curved = std::max (shape, 0.72);
            level *= symbol == 'X' ? std::max (curved, 0.9) : curved * (1.0 + 0.25 * a);
        }

        const bool chordHit = isChordLane (lane);
        // A chord hit on the last 8th of a bar plays the chord coming next (the push).
        const bool push = s.pushChanges && chordHit && stepInBar >= 14 && symbol != 'g' && ! (nextChord == chord);
        // Now and then (with Humanize) the last bar of the phrase answers with another top note.
        // The chance follows Humanize; a step keeps its choice for the whole phrase.
        const bool answer = human > 0.0 && phraseBar == 3
                         && (lane == (int) Lane::stab || lane == (int) Lane::slide || lane == (int) Lane::chank)
                         && lifeNoise (lane, 1) < 0.55 * human;
        NoteList notes;
        const int contour = s.pattern == randomPatternIndex() ? lickContourFor (s.randomGenre) : 0;
        const int segmentLength = std::max (1, len / numSegments);
        const int melodicStep = std::clamp (s.segmentOrder[(size_t) (pos / segmentLength)], 0, numSegments - 1) * segmentLength + pos % segmentLength;
        laneNotes ((Lane) lane, push ? nextChord : chord, push ? nextVoicing : voicing, melodicStep, s.octaves[(size_t) lane] + s.globalOctave,
                   notes, s.roomForBand, answer, contour, s.key, s.scale, s.riffSeed);
        for (auto& n : notes)
            n += transpose;
        if (symbol == 'g' && lane == (int) Lane::hook && notes.size() > 4)
        {
            // A ghost strum only brushes the top strings.
            NoteList top;
            for (int i = notes.size() - 4; i < notes.size(); ++i)
                top.add (notes[i]);
            notes = top;
        }
        const bool up = repeatsUp (lane, stepInBar);
        if (up)
            std::reverse (notes.begin(), notes.end());
        // The pick crosses the strings: a few ms each, a rake ('f') slower and every string heard,
        // an up strum and an accent quicker. A plain strum never takes longer than most of a 16th.
        double gap = std::clamp ((double) s.attackMs, 0.0, 60.0) * beatsPerMs * (symbol == 'f' ? 2.6 : 1.0)
                   * (lane == (int) Lane::scratch || lane == (int) Lane::stab ? 0.5 : 1.0)
                   * (up ? 0.7 : 1.0) * (symbol == 'X' ? 0.85 : 1.0);
        if (symbol != 'f' && notes.size() > 1)
            gap = std::min (gap, 0.75 * stepLength / (double) (notes.size() - 1));
        const int articulation = articulationOf (lane, symbol);

        auto strum = [&] (double at, double length, double strumLevel, int extraSalt)
        {
            const double middle = 0.4 * (double) (notes.size() - 1);
            double previous = at;
            for (int i = 0; i < notes.size(); ++i)
            {
                // A down strum digs into the middle strings; an up strum's first strings (the
                // high ones) sound most, the pick slowing down after them.
                const double stringLevel = strumLevel * (up ? 1.0 - 0.06 * (double) i : 1.0 - 0.06 * std::abs ((double) i - middle));
                // Each string a hair early or late (with Humanize), never before the one before.
                double when = at + gap * (double) i;
                if (i > 0)
                    when = std::max (previous, when + human * 1.5 * beatsPerMs * (noise (step, seedSalt + salt + extraSalt + 7 * i + 3) - 0.5) * 2.0);
                previous = when;
                pending.push_back ({ when, notes[i], velocityFor (lane, stringLevel, salt + extraSalt + i),
                                     std::max (0.6 * length, length - gap * (double) i), lane, chordHit && i == 0 });
                pending.back().articulation = articulation;
            }
        };

        if (symbol == 'f' && roll <= 1) roll = 2;
        roll = std::clamp (roll, 1, 4);
        if (roll > 1)
        {
            // A roll: that many quick repeats in the 16th, the first one the loudest.
            const double each = stepLength / roll;
            for (int k = 0; k < roll; ++k)
                strum (t + each * k, each * 0.9, level * (k == 0 ? 1.0 : 0.82), k * 17);
            return;
        }
        if (lane == (int) Lane::slide)
        {
            // A semitone below, slid up into the chord: the grace note still sounds when the
            // chord arrives (a sampled lead hears the overlap as a legato slide).
            const double overlap = 0.0;
            for (int i = 0; i < notes.size(); ++i)
            {
                // A slide on the downbeat used to start before the clip and get dropped.
                const double target = t + gap * (double) i;
                const double start = step >= 0 ? std::max (0.0, target - std::max (0.002, (double) s.attackMs * beatsPerMs)) : target - std::max (0.002, (double) s.attackMs * beatsPerMs);
                pending.push_back ({ start, notes[i] - 1, velocityFor (lane, 0.6 * level, salt + 5), target - start + overlap, lane, i == 0 });
                pending.back().articulation = articulation;
                pending.back().grace = true;
            }
        }
        if (lane == (int) Lane::swell && s.keyswitches)
        {
            // The volume swell, as a sampled lead needs it: CC 11 from nothing to full.
            const double rise = 350.0 * beatsPerMs;
            for (int k = 0; k <= 7; ++k)
            {
                pending.push_back ({ t + rise * k / 7.0, 11, (int) std::lround (127.0 * k / 7.0), 0.0, lane });
                pending.back().controller = 11;
            }
        }
        const double length = s.letRing && ringsOn (lane) ? 16.0 * stepLength : noteLength (lane) * stepLength;
        strum (t, length, level, 0);
    };

    // ---- The groove as written (what a fill leaves of it) ----
    auto plays = [&] (int lane)
    {
        return isHit (pattern.lanes[(size_t) lane][(size_t) pos]) && ! s.muted[(size_t) lane]
            && ! (inFill && ! fillKeeps (style, lane, customLanes));
    };
    // The big strum: after a fill, and every 8 bars of the drop.
    const bool resolveNote = stepInBar == 0 && s.resolveAfterFill && pattern.lanes[(size_t) Lane::hook][(size_t) pos] == '.'
                       && ! s.muted[(size_t) Lane::hook]
                       && (isFillBar (bar - 1) || (s.section == Section::drop && mod (bar - s.sectionStartBar, 8) == 0));

    // Whether the fill strikes a chord on this step (the scratch then leaves it to the fill).
    auto fillStrikesChord = [&]
    {
        if (! inFill)
            return false;
        const int i = stepInBar;
        switch (style)
        {
            case FillStyle::motifBuild:
            case FillStyle::ghostBuild:
            case FillStyle::pulseLift:    return true;
            case FillStyle::dropOut:      return i == 15;
            case FillStyle::approachUp:      return i == 12 || i == 14;
            case FillStyle::noteStutter:  return i != 13;
            case FillStyle::longOut:     return i == 8;
            case FillStyle::custom:
                for (int l = 0; l < numLanes; ++l)
                    if (isChordLane (l) && isHit (s.customFill[(size_t) l][(size_t) i]))
                        return true;
                return false;
            default:                      return false;
        }
    };
    const bool fillChord = fillStrikesChord();

    for (int lane = 0; lane < numLanes; ++lane)
    {
        char symbol = pattern.lanes[(size_t) lane][(size_t) pos];
        if (inFill && ! fillKeeps (style, lane, customLanes))
            continue;
        if (resolveNote && isChordLane (lane))
            continue;   // the big strum plays for every chord hit of the step
        if (lane == (int) Lane::scratch && isHit (symbol))
        {
            // The scratch fills the gaps: where another chord hit plays, the hand plays that.
            bool covered = fillChord;
            for (int other = 0; other < numLanes; ++other)
                covered = covered || (other != lane && isChordLane (other) && plays (other));
            if (covered)
                continue;
        }
        else if (isChordLane (lane) && plays (lane))
        {
            // One hand: two chord hits on the same step are one strum, the louder one.
            bool louder = false;
            for (int other = 0; other < numLanes && ! louder; ++other)
            {
                if (other == lane || other == (int) Lane::scratch || ! isChordLane (other) || ! plays (other))
                    continue;
                const double mine = symbolLevel (symbol), theirs = symbolLevel (pattern.lanes[(size_t) other][(size_t) pos]);
                louder = theirs > mine || (theirs == mine && other < lane);
            }
            if (louder)
                continue;
        }
        // ...and now and then a soft scratch that isn't written, just before the next bar.
        if (symbol == '.' && lane == (int) Lane::scratch && human > 0.0 && ! inFill && stepInBar >= 13 && stepInBar % 2 == 1
            && phraseBar % 2 == 1 && std::any_of (pattern.lanes[(size_t) lane].begin(), pattern.lanes[(size_t) lane].end(), isHit)
            && lifeNoise (lane, 91) < 0.3 * human)
        {
            bool free = true;
            for (int other = 0; other < numLanes; ++other)
                free = free && ! (isChordLane (other) && plays (other));
            if (free)
                symbol = 'g';
        }
        add (lane, symbol);
    }

    // ---- The fill ----
    if (inFill)
    {
        const int start = fillStart;
        const double ramp = 0.62 + 0.45 * (double) (stepInBar - start) / (double) std::max (1, 15 - start);
        const int i = stepInBar;
        const bool minor = ((chord.mask >> 3u) & 1u) != 0 && ((chord.mask >> 4u) & 1u) == 0;
        const int run = std::min (3, std::max (0, i - 12));   // the last four 16ths of a run

        switch (style)
        {
            case FillStyle::motifBuild:   // 16th repeats, down and up, louder and louder
                add (i % 2 == 0 ? (int) Lane::hook : (int) Lane::answer, i >= 14 ? 'X' : 'x', 0, ramp, true);
                break;
            case FillStyle::highRun:    // octaves climbing the chord into the next bar
            {
                static constexpr int minorRun[] { 0, 3, 5, 7 }, majorRun[] { 0, 4, 5, 7 };
                add ((int) Lane::octave, i == 15 ? 'X' : 'x', (minor ? minorRun : majorRun)[run], ramp, true);
                break;
            }
            case FillStyle::ghostBuild:
                add ((int) Lane::scratch, i == 15 ? 'X' : 'x', 0, ramp + 0.2, true);
                break;
            case FillStyle::pulseLift:
                add ((int) Lane::chank, i == 14 ? 'X' : 'x', 0, ramp, true);
                break;
            case FillStyle::dropOut:      // silence, then one stab on the very last 16th
                if (i == 15)
                    add ((int) Lane::stab, 'X', 0, 1.0, true);
                break;
            case FillStyle::approachUp:
                if (i == 14)
                    add ((int) Lane::slide, 'X', 0, 1.0, true);
                else if (i == 12)
                    add ((int) Lane::scratch, 'x', 0, 1.0, true);
                break;
            case FillStyle::noteStutter:
                if (i != 13)
                    add ((int) Lane::stab, i == 15 ? 'X' : 'x', 0, ramp, true, i >= 14 ? 2 : 1);
                break;
            case FillStyle::melodyRun:
                add ((int) Lane::lick, i == 15 ? 'X' : 'x', 0, ramp, true);
                break;
            case FillStyle::arpeggioRun:
                add ((int) Lane::arp, i % 4 == 0 ? 'X' : 'x', 0, ramp, true);
                break;
            case FillStyle::lowRun:     // the low string walks up to the next bar
            {
                static constexpr int minorWalk[] { 0, 2, 3, 5 }, majorWalk[] { 0, 2, 4, 5 };
                add ((int) Lane::root, i == 15 ? 'X' : 'x', (minor ? minorWalk : majorWalk)[run], ramp, true);
                break;
            }
            case FillStyle::longOut:     // half a bar held, then nothing
                if (i == 8)
                    add ((int) Lane::swell, 'X', 0, 1.0, true);
                break;
            case FillStyle::custom:
                for (int l = 0; l < numLanes; ++l)
                    add (l, s.customFill[(size_t) l][(size_t) i], 0, 1.0, true);
                break;
            case FillStyle::mix:
            default:
                break;
        }
    }

    // ---- A big strum: after a fill, and every 8 bars of the drop ----
    if (resolveNote)
        add ((int) Lane::hook, 'X', 0, 1.0, true);
}

//==============================================================================
void GrooveEngine::flushOffsUpTo (double ppq, bool inclusive, std::vector<NoteEvent>& out)
{
    scratch.clear();
    for (int n = 0; n < 128; ++n)
    {
        const double off = offTime[(size_t) n];
        if (off >= 0.0 && (off < ppq || (inclusive && off <= ppq)))
            scratch.push_back (n);
    }

    std::sort (scratch.begin(), scratch.end(),
               [this] (int a, int b) { return offTime[(size_t) a] < offTime[(size_t) b]; });

    for (int n : scratch)
    {
        out.push_back ({ offTime[(size_t) n], n, 0, laneOf[(size_t) n] });
        offTime[(size_t) n] = -1.0;
    }
}

void GrooveEngine::render (double startPpq, double endPpq, const LeadSettings& s, std::vector<NoteEvent>& out)
{
    if (endPpq <= startPpq)
        return;

    const auto& resolved = patternFor (s);
    pending.clear();

    // Include every delayed string, even for a slow rake at the fastest host tempo.
    // The extra beat covers swing, rolls and humanization; shifts are expressed in ms.
    const double beatsPerMs = std::clamp (s.tempo, 20.0, 400.0) / 60000.0;
    double templateDelay = 0.0;
    if (s.grooveTemplate.active)
        for (float timing : s.grooveTemplate.timing)
            templateDelay = std::max (templateDelay, (double) timing * stepLength * 2.0);
    const double maxDelay = 1.0 + templateDelay + beatsPerMs
        * (40.0 + 6.0 * 2.6 * std::clamp ((double) s.attackMs, 0.0, 60.0));
    const auto firstStep = (long long) std::floor ((startPpq - maxDelay) / stepLength);
    // A negative template plus a 40 ms push and slide grace notes can precede three steps.
    const auto lastStep  = (long long) std::floor (endPpq / stepLength) + 3;
    for (auto step = firstStep; step <= lastStep; ++step)
        collectStep (step, s, resolved.pattern, resolved.variation);

    pending.erase (std::remove_if (pending.begin(), pending.end(),
                                   [&] (const PendingOn& p) { return p.ppq < startPpq || p.ppq >= endPpq || p.note < 0 || p.note > 127; }),
                   pending.end());
    for (size_t i = 0; i < pending.size(); ++i)
        pending[i].order = i;
    std::sort (pending.begin(), pending.end(), [] (const PendingOn& a, const PendingOn& b)
    { return a.ppq < b.ppq || (a.ppq == b.ppq && a.order < b.order); });

    // Several fill gestures can land together. Keep the strongest melodic attack so
    // MIDI export contains one audible pitch, without zero-length notes at the same time.
    size_t written = 0;
    for (size_t begin = 0; begin < pending.size();)
    {
        size_t end = begin + 1, best = pending.size();
        while (end < pending.size() && pending[end].ppq == pending[begin].ppq) ++end;
        for (size_t i = begin; i < end; ++i)
            if (pending[i].controller < 0 && (best == pending.size() || pending[i].velocity > pending[best].velocity)) best = i;
        const PendingOn melody = best < pending.size() ? pending[best] : PendingOn {};
        for (size_t i = begin; i < end; ++i)
            if (pending[i].controller >= 0) pending[written++] = pending[i];
        if (best < pending.size()) pending[written++] = melody;
        begin = end;
    }
    pending.resize (written);

    for (const auto& p : pending)
    {
        flushOffsUpTo (p.ppq, true, out);

        if (p.controller >= 0)
        {
            out.push_back ({ p.ppq, p.note, p.velocity, p.lane, p.controller });
            continue;
        }

        // A lead is strictly monophonic, including approaches, rolls and overlapping fills.
        // Release the previous pitch before the next note-on, also across audio blocks.
        for (int n = 0; n < 128; ++n)
            if (offTime[(size_t) n] >= 0.0 && laneOf[(size_t) n] != keyswitchLane)
                offTime[(size_t) n] = p.ppq;
        flushOffsUpTo (p.ppq, true, out);

        // The articulation keyswitch, sent when it changes, just before the note it is for.
        if (s.keyswitches && p.articulation != lastArticulation)
        {
            const int ks = std::clamp (s.keyswitch[(size_t) std::clamp (p.articulation, 0, numArticulations - 1)], 0, 127);
            if (ks != p.note)
            {
                if (offTime[(size_t) ks] >= 0.0)
                    out.push_back ({ p.ppq, ks, 0, laneOf[(size_t) ks] });
                out.push_back ({ p.ppq, ks, 100, keyswitchLane });
                offTime[(size_t) ks] = p.ppq + 0.03;
                onTime[(size_t) ks] = p.ppq;
                laneOf[(size_t) ks] = keyswitchLane;
                graceOf[(size_t) ks] = false;
            }
            lastArticulation = p.articulation;
        }

        auto& off = offTime[(size_t) p.note];
        if (off >= 0.0)
            out.push_back ({ p.ppq, p.note, 0, laneOf[(size_t) p.note] });   // the same string again

        out.push_back ({ p.ppq, p.note, p.velocity, p.lane, -1, p.articulation });
        off = p.ppq + p.duration;
        onTime[(size_t) p.note] = p.ppq;
        laneOf[(size_t) p.note] = p.lane;
        graceOf[(size_t) p.note] = p.grace;
    }

    flushOffsUpTo (endPpq, false, out);
}

void GrooveEngine::allNotesOff (double ppq, std::vector<NoteEvent>& out)
{
    for (int n = 0; n < 128; ++n)
        if (offTime[(size_t) n] >= 0.0)
        {
            out.push_back ({ ppq, n, 0, laneOf[(size_t) n] });
            offTime[(size_t) n] = -1.0;
        }
    lastArticulation = -1;   // after a stop or a jump the next note sends its keyswitch again
}

bool GrooveEngine::anyNoteSounding() const noexcept
{
    return std::any_of (offTime.begin(), offTime.end(), [] (double t) { return t >= 0.0; });
}

//==============================================================================
std::vector<NoteEvent> GrooveEngine::renderClip (const LeadSettings& s, double lengthInBeats)
{
    GrooveEngine engine;
    std::vector<NoteEvent> events;
    engine.render (0.0, lengthInBeats, s, events);
    engine.allNotesOff (lengthInBeats, events);
    return events;
}

std::vector<NoteEvent> GrooveEngine::renderArrangement (const LeadSettings& s, const std::vector<ArrangementPart>& parts,
                                                        std::vector<Marker>* markers)
{
    GrooveEngine engine;
    std::vector<NoteEvent> events;
    long long bar = 0;

    for (const auto& part : parts)
    {
        if (part.bars <= 0)
            continue;

        auto settings = s;
        settings.section = part.section;
        settings.sectionStartBar = bar;
        settings.sectionVariation = part.variation;
        settings.forcedFillBar = -1;
        if (part.endFill != -1)
        {
            // Its own fill on its last bar (or none at all).
            settings.forcedFillBar = bar + part.bars - 1;
            settings.forcedFillStyle = part.endFill;
        }

        const double start = (double) bar * 4.0, end = (double) (bar + part.bars) * 4.0;
        if (markers != nullptr)
            markers->push_back ({ start, sectionName ((int) part.section) });

        engine.render (start, end, settings, events);
        engine.allNotesOff (end, events);
        bar += part.bars;
    }
    return events;
}

} // namespace fgl
