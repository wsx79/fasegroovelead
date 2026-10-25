// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Generators.h"

#include <algorithm>

namespace fgl
{

namespace
{
    /** Small, fast and repeatable (same seed = same beat on every computer). */
    struct Random
    {
        // The seed is hashed into the starting state. (It used to be a plain multiple of the
        // step, so seed n + 1 replayed seed n's draws shifted by one: neighbouring seeds, and the
        // lanes of a lane die, gave near copies.)
        explicit Random (std::uint32_t seed) noexcept : state (mix ((std::uint64_t) seed ^ 0x632BE59BD9B4E019ULL)) {}

        static std::uint64_t mix (std::uint64_t z) noexcept
        {
            z = (z ^ (z >> 33)) * 0xFF51AFD7ED558CCDULL;
            z = (z ^ (z >> 33)) * 0xC4CEB9FE1A85EC53ULL;
            return z ^ (z >> 33);
        }

        std::uint32_t next() noexcept
        {
            state += 0x9E3779B97F4A7C15ULL;
            auto z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return (std::uint32_t) ((z ^ (z >> 31)) >> 32);
        }

        int nextInt (int n) noexcept           { return n > 0 ? (int) (next() % (std::uint32_t) n) : 0; }
        float nextFloat() noexcept             { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
        bool chance (float probability) noexcept { return nextFloat() < probability; }

        template <typename T, size_t N>
        T pick (const T (&items)[N]) noexcept { return items[nextInt ((int) N)]; }

        std::uint64_t state;
    };

    std::string compact (const char* s)
    {
        std::string out;
        for (; *s != 0; ++s)
            if (*s != ' ')
                out += *s;
        return out;
    }

    void soften (std::string& line)
    {
        for (auto& c : line)
            if (c == 'X')
                c = 'x';
    }

    // The genres, in grooveCategories() order.
    enum Genre { pianoHouse = 0, soulful, deep, gospel, jackin, jackinPiano, garage, afro, funky, nineties, neoSoul, chill, dance };

    void clearLine (std::string& line) { std::fill (line.begin(), line.end(), '.'); }
    bool hits (const std::string& line) { return std::any_of (line.begin(), line.end(), isHit); }

    /** A plain hit the accent groove will not bury: not a scratch, not a ghost, and not a
        weak 16th of an up-strum, chank or picked line. */
    bool hasSpeakingHit (const LeadPattern& p)
    {
        for (int l = 0; l < numLanes; ++l)
        {
            if (l == (int) Lane::scratch)
                continue;
            const bool accent = l == (int) Lane::answer || l == (int) Lane::chank || l == (int) Lane::arp;
            const auto& line = p.lanes[(size_t) l];
            for (int i = 0; i < (int) line.size(); ++i)
            {
                const char c = line[(size_t) i];
                if (c != 'X' && c != 'x' && c != 'f')
                    continue;
                if (! accent || i % 4 == 0 || i % 4 == 2)
                    return true;
            }
        }
        return false;
    }
}

//==============================================================================
const char* sectionName (int section) noexcept
{
    static const char* names[] { "Main", "Intro", "Break", "Build-up", "Drop", "Outro" };
    return section >= 0 && section < numSections ? names[section] : "";
}

double sectionVelocityScale (Section section) noexcept
{
    switch (section)
    {
        case Section::intro:     return 0.85;
        case Section::breakdown: return 0.75;
        case Section::buildup:   return 0.95;
        case Section::drop:      return 1.08;
        case Section::outro:     return 0.85;
        case Section::main:
        default:                 return 1.0;
    }
}

bool sectionAllowsFills (Section section) noexcept
{
    return section == Section::main || section == Section::drop;
}

LeadPattern applySection (const LeadPattern& source, Section section)
{
    LeadPattern p = source;
    const int len = p.length();
    const int genre = categoryIndex (source.category);

    // Keep these lanes (softened) and clear the rest.
    auto keep = [&p] (std::initializer_list<Lane> lanes)
    {
        for (int l = 0; l < numLanes; ++l)
        {
            bool stay = false;
            for (auto lane : lanes)
                stay = stay || (Lane) l == lane;
            if (stay)
                soften (p.lanes[(size_t) l]);
            else
                clearLine (p.lanes[(size_t) l]);
        }
    };
    auto anyHits = [&p] { return std::any_of (p.lanes.begin(), p.lanes.end(), hits); };

    switch (section)
    {
        case Section::intro:
        case Section::outro:
        {
            if (genre == gospel)
            {
                keep ({ Lane::hook, Lane::answer, Lane::root });
                if (! anyHits())
                    for (int i = 0; i < len; i += 2)
                        p[i % 4 == 0 ? Lane::hook : Lane::answer][(size_t) i] = 'x';
            }
            else if (genre == funky || genre == jackin || genre == jackinPiano)
            {
                keep ({ Lane::scratch, Lane::chank, Lane::answer });
                if (! hits (p[Lane::scratch]))
                    for (int i = 0; i < len; i += 4)
                        p[Lane::scratch][(size_t) i] = 'g';
                // A scratch of ghosts, or up-repeats only on the weak 16ths, sits under the
                // accent curve and can vanish. One chank on the "&" gives the intro a voice.
                if (! hasSpeakingHit (p))
                    for (int bar = 0; bar < len / 16; ++bar)
                        p[Lane::chank][(size_t) (bar * 16 + 2)] = 'x';
            }
            else if (genre == dance)
            {
                keep ({ Lane::power, Lane::stab, Lane::root });
                if (! hits (p[Lane::power]) && ! hits (p[Lane::stab]))
                    for (int i = 0; i < len; i += 4)
                        p[Lane::power][(size_t) i] = 'x';
            }
            else if (genre == deep || genre == chill)
            {
                keep ({ Lane::swell, Lane::arp });
                if (! anyHits())
                    for (int i = 0; i < len; i += 16)
                        p[Lane::swell][(size_t) i] = 'x';
            }
            else if (genre == afro)
                keep ({ Lane::arp, Lane::chank, Lane::root, Lane::answer });
            else if (genre == neoSoul)
                keep ({ Lane::arp, Lane::lick, Lane::swell, Lane::answer });
            else
            {
                // House and the rest: the light parts only.
                keep ({ Lane::chank, Lane::scratch, Lane::answer, Lane::arp });
            }
            if (! anyHits())
                for (int i = 2; i < len; i += 4)
                    p[Lane::chank][(size_t) i] = 'x';
            break;
        }
        case Section::breakdown:
        {
            // A held chord on every bar; only the picking goes on under it, softly.
            for (int l = 0; l < numLanes; ++l)
                if ((Lane) l == Lane::arp)
                    soften (p.lanes[(size_t) l]);
                else
                    clearLine (p.lanes[(size_t) l]);
            for (int i = 0; i < len; i += 16)
                p[Lane::swell][(size_t) i] = 'X';
            break;
        }
        case Section::buildup:
        {
            for (auto& line : p.lanes)
                clearLine (line);
            if (genre == gospel)
            {
                // Eighth-note repeats, twice as often by the end. No 16th scratch.
                for (int i = 0; i < len; ++i)
                {
                    const double at = (double) i / (double) std::max (1, len);
                    const int every = at < 0.5 ? 8 : at < 0.75 ? 4 : 2;
                    if (i % every == 0)
                        p[i % 4 == 0 ? Lane::hook : Lane::answer][(size_t) i] = i % 8 == 0 ? 'X' : 'x';
                }
            }
            else if (genre == dance)
            {
                for (int i = 0; i < len; ++i)
                {
                    const double at = (double) i / (double) std::max (1, len);
                    const int every = at < 0.65 ? 4 : 2;
                    if (i % every == 0)
                        p[Lane::power][(size_t) i] = 'X';
                }
            }
            else if (genre == deep || genre == chill || genre == neoSoul)
            {
                for (int i = 0; i < len; ++i)
                {
                    const double at = (double) i / (double) std::max (1, len);
                    if (i % 16 == 0)
                        p[Lane::swell][(size_t) i] = 'X';
                    else if (at >= 0.5 && i % (at < 0.75 ? 4 : 2) == 0)
                        p[Lane::arp][(size_t) i] = 'x';
                }
            }
            else if (genre == afro)
            {
                for (int i = 0; i < len; ++i)
                {
                    const double at = (double) i / (double) std::max (1, len);
                    const int every = at < 0.55 ? 4 : 2;
                    if (i % every == 0)
                        p[Lane::arp][(size_t) i] = i % 4 == 0 ? 'X' : 'x';
                }
            }
            else
            {
                // House, funk, soul: a muted scratch that doubles up, a strum on every bar.
                for (int i = 0; i < len; ++i)
                {
                    const double at = (double) i / (double) len;
                    const int every = at < 0.5 && len > 16 ? 4 : at < 0.75 ? 2 : 1;
                    if (i % 16 == 0)
                        p[Lane::hook][(size_t) i] = 'X';
                    else if (i % every == 0)
                        p[Lane::scratch][(size_t) i] = i % 4 == 0 ? 'X' : 'x';
                }
            }
            break;
        }
        case Section::drop:   // the full groove (the engine adds a big strum every 8 bars)
        case Section::main:
        default:
            break;
    }

    if (section != Section::main)
        p.name = source.name + " / " + sectionName ((int) section);
    return p;
}

//==============================================================================
namespace
{
    // One-bar cells and how common each is in house lead (the weight).
    struct Cell { const char* steps; int weight; };

    const Cell stabCells[] {
        { "..X. ..X. ..X. ..X.", 30 }, { "..X. ..X. ..X. .xX.", 10 }, { "..X. ..x. ..X. ..x.", 10 }, { "...X ..X. ...X ..X.", 6 },
        { "..X. ...X ..X. ....", 6 }, { "X... ..X. ..X. X...", 5 }, { "X..X ..X. .X.. X.X.", 4 } };
    const Cell twoStepCells[] {
        { "X... ..x. ...x ....", 10 }, { "X... ...x ..x. ....", 8 }, { "...X .... ..x. ...x", 6 }, { "X..x .... ..x. ....", 5 } };
    const Cell chankCells[] {
        { "..x. ..x. ..x. ..x.", 20 }, { "x.xx .x.x x.xx .x.x", 8 }, { "..xx ..x. ..xx ..x.", 8 }, { "x..x ..x. ..x. x...", 6 },
        { ".x.x .x.x .x.x .x.x", 5 }, { "..x. .xx. ..x. .x.x", 5 }, { "x.xx .xx. x.xx .x.x", 4 } };
    const Cell arpCells[] {
        { "x.x. x.x. x.x. x.x.", 18 }, { "x.xx x.xx x.xx x.xx", 10 }, { "xxxx xxxx xxxx xxxx", 5 }, { "x.x. xx.x x.x. xx.x", 8 },
        { "x.xx .xx. x.xx .xx.", 6 }, { "x.x. .x.x x.x. .x.x", 5 } };
    // Funk sixteenths: the hand goes down on the even 16ths and up on the odd ones.
    const Cell downCells[] {
        { "X... ..x. ..x. x...", 10 }, { "X... ..x. X... x.x.", 8 }, { "X.x. ..x. X... ..x.", 6 }, { "X... x... X... x...", 6 },
        { "X... .... X... ....", 5 } };
    const Cell upCells[] {
        { ".x.x ...x .x.. .x.x", 8 }, { "...x .x.. ...x ...x", 8 }, { "...x ...x ...x ...x", 6 }, { ".x.. .x.. .x.. .x.x", 5 },
        { ".... ..x. .... ..x.", 4 } };
    const Cell eighthDownCells[] { { "X... X... X... X...", 8 }, { "X... x... X... x...", 6 }, { "X... .... X... x...", 4 } };
    const Cell eighthUpCells[]   { { "..x. ..x. ..x. ..x.", 8 }, { ".... ..x. .... ..x.", 5 }, { "..x. ..x. ..x. .xx.", 4 } };
    const Cell powerCells[] {
        { "X.x. X.x. X.x. X.x.", 8 }, { "X... ..X. ..X. X...", 8 }, { "..X. ..X. ..X. ..X.", 6 }, { "X..X ..X. X..X ..X.", 5 } };
    const Cell octaveCells[] { { "X.x. x.x. X.x. x.x.", 8 }, { "x..x ..x. x..x ..x.", 6 }, { "X.xx x.xx X.xx x.xx", 4 } };
    const Cell rootCells[]   { { "X... .... X... ....", 10 }, { "X... .... X..x ....", 6 }, { "X... X... X... X...", 5 }, { "X... .... ..x. ....", 4 } };
    // Funk: the pick goes down on the even 16ths and up on the odd ones, and stays there.
    const Cell funkDown[] {
        { "X.x. ..x. X.x. ..x.", 8 }, { "X... x.x. ..x. x...", 6 }, { "X.x. X... ..x. x.x.", 6 }, { "X... ..x. X... x.x.", 5 },
        { "x.x. ..x. x.x. ..x.", 4 } };
    const Cell funkUp[] {
        { ".x.x .x.x .x.x .x.x", 8 }, { ".x.. .x.x .x.. .x.x", 6 }, { "...x .x.x ...x .x.x", 5 }, { ".x.x ...x .x.. .x.x", 4 } };
    const Cell funkPulse[] {
        { "..x. ..x. ..x. ..x.", 8 }, { "..xx ..x. ..xx ..x.", 5 }, { "..x. .xx. ..x. ..x.", 4 } };
    // Dance: power chords on the downbeats.
    const Cell gospelRoot[] { { "X... .... X... ....", 6 }, { "X... .... .... ....", 4 }, { "X... X... .... X...", 3 } };
    // 16th chank: the "&" is the accent, the 16ths around it are the scratch of the left hand.
    const Cell jackPulse[] {
        { "..x. ..xx ..x. .xx.", 6 }, { "x.xx ..x. x.xx ..x.", 5 }, { "..xx ..x. ..xx .x.x", 5 }, { ".x.x ..xx .x.x ..x.", 4 },
        { "..x. .xx. ..xx ..x.", 4 }, { "x..x ..xx x..x .xx.", 4 }, { "..xx .x.x ..x. ..xx", 3 }, { ".xx. ..x. .xx. ..xx", 3 } };
    const Cell funkScratch[] {
        { ".g.g .g.g .g.g .g.g", 5 }, { "g.g. g.g. g.g. g.g.", 4 }, { ".g.g g.g. .g.g g..g", 4 }, { "g... g.g. g... g.g.", 3 },
        { ".gg. .g.g .gg. .g.g", 3 }, { "g.g. .g.g g.g. .gg.", 3 } };
    const Cell garageSkips[] {
        { "X... .... ..x. ....", 6 }, { "X... ..x. .... ....", 5 }, { ".... ..X. .... ..x.", 5 }, { "X... .... .... ..x.", 4 },
        { ".... .... ..X. ....", 4 }, { "X... .... ..x. ..x.", 3 }, { "X... ..x. .... ..x.", 3 }, { ".... ..x. X... ....", 3 },
        { "X... .... ..X. ....", 3 }, { "..x. .... .... ..X.", 2 } };
    const Cell swellCells[] {
        { "X... .... .... ....", 6 }, { "X... .... X... ....", 4 }, { ".... .... X... ....", 3 }, { "X... .... .... X...", 3 } };
    // Afro: tumbao and partido alto, not a straight 16th arp.
    const Cell afroCells[] {
        { "x..x ..x. ..x. .x..", 8 }, { ".x.x ..x. x.x. ..x.", 7 }, { "x.x. ..x. ..x. x...", 5 }, { "..x. x..x ..x. .x.x", 4 },
        { "x..x ..x. x..x ..x.", 4 }, { "..x. ..x. x... ..x.", 3 }, { "x... ..x. ..x. x..x", 3 }, { ".x.. x..x ..x. .x..", 3 },
        { "x..x .x.. ..x. x...", 3 }, { "..x. x... ..x. .x.x", 2 } };
    const Cell afroRoot[] { { "x..x ..x. ..x. x...", 6 }, { "X... ..x. ..x. ....", 4 }, { "x... ..x. x... ..x.", 3 } };
    // Neo-soul and pads: space, not a grid filled in.
    const Cell neoCells[] {
        { "X... .... ..x. ....", 8 }, { ".... ..X. .... ....", 6 }, { "X... .... .... ..x.", 5 }, { "..x. .... .... ....", 4 },
        { "x... ..x. .... ....", 3 } };
    const Cell sparseArp[] {
        { "x... ..x. .... .x..", 6 }, { "x... .... x... ....", 5 }, { "x... ..x. ..x. ....", 4 }, { ".... ..x. .... ..x.", 3 },
        { "x... .... ..x. ....", 4 }, { ".... ..x. .... ....", 3 }, { "x... ..x. .... ....", 3 }, { ".... .... ..x. ....", 3 },
        { "..x. .... ..x. ....", 2 }, { "x... .... .... ..x.", 2 } };
    const Cell lickCellsExtra[] {
        { ".... .... .... x.x.", 5 }, { ".... ..x. .... .x.x", 4 }, { "..x. .x.. .... ....", 3 }, { ".... .... .xxx x...", 3 },
        { ".... .... ..x. .x..", 3 }, { ".... .x.. .... x...", 2 }, { ".... .... x... .x.x", 2 }, { "..x. .... .... .xx.", 2 } };

    // Gospel answers: on the 8ths only, as the strumming hand plays them.
    const Cell gospelRuns[] {
        { ".... .... x.x. x.x.", 5 }, { ".... ..x. .... x.x.", 4 }, { "..x. x... .... ..x.", 3 }, { ".... .... ..x. x.x.", 3 } };

    template <size_t N>
    const char* pickCell (Random& r, const Cell (&cells)[N])
    {
        int total = 0;
        for (const auto& c : cells) total += c.weight;
        int x = r.nextInt (total);
        for (const auto& c : cells)
            if ((x -= c.weight) < 0)
                return c.steps;
        return cells[0].steps;
    }

    /** Bar 1 and bar 2 each take a cell. The second draw is repeated until it is a real answer. */
    template <size_t N>
    void paintTwoBars (LeadPattern& p, Lane lane, Random& r, const Cell (&cells)[N])
    {
        const auto first = compact (pickCell (r, cells));
        std::string second = first;
        for (int n = 0; n < 8 && second == first; ++n)
            second = compact (pickCell (r, cells));
        auto put = [&] (const std::string& cell, int bar)
        {
            for (int i = 0; i < 16 && i < (int) cell.size(); ++i)
                p[lane][(size_t) (bar * 16 + i)] = cell[(size_t) i];
        };
        put (first, 0);
        put (second, 1);
    }

    /** Offbeat stabs: each "&" is an accent, a plain stab or a rest, and the two bars are drawn apart.
        Jackin' piano bounces: now and then a stab lands on the last 16th before a beat. '90s piano
        house sometimes has octaves on the beats under the stabs instead of the low root. */
    void paintOffbeatStaccatos (LeadPattern& p, Random& r, int genre)
    {
        const bool bounce = genre == jackinPiano;
        const bool octaves = genre == nineties && r.chance (0.45f);
        for (int bar = 0; bar < 2; ++bar)
        {
            int placed = 0;
            for (int q = 0; q < 4; ++q)
            {
                const int roll = (int) (r.next() % 100);
                if (roll >= 12)
                {
                    p[Lane::stab][(size_t) (bar * 16 + 2 + 4 * q)] = roll < 56 ? 'x' : 'X';
                    ++placed;
                }
                if (octaves)
                {
                    if (q == 0 || r.chance (0.7f))
                        p[Lane::octave][(size_t) (bar * 16 + 4 * q)] = q == 0 ? 'X' : 'x';
                }
                else if (r.next() % 100 < 35)
                    p[Lane::root][(size_t) (bar * 16 + 4 * q)] = 'X';
            }
            if (placed == 0)
                p[Lane::stab][(size_t) (bar * 16 + 2)] = 'X';
            if (bounce && r.chance (0.6f))
                p[Lane::stab][(size_t) (bar * 16 + r.pick ({ 3, 7, 11 }))] = 'x';   // pushes into the next beat
        }
    }

    /** Short dance stabs on the 8ths (downbeat and "&"), never a 16th and never a held power chord. */
    void paintDanceStaccatos (LeadPattern& p, Random& r)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            int placed = 0;
            for (int q = 0; q < 4; ++q)
            {
                const int down = (int) (r.next() % 100);
                const int off = (int) (r.next() % 100);
                if (down < 42)
                {
                    p[Lane::stab][(size_t) (bar * 16 + 4 * q)] = down < 18 ? 'x' : 'X';
                    ++placed;
                }
                if (off < 78)
                {
                    p[Lane::stab][(size_t) (bar * 16 + 4 * q + 2)] = off < 28 ? 'x' : 'X';
                    ++placed;
                }
            }
            if (placed < 2)
                p[Lane::stab][(size_t) (bar * 16 + 2)] = 'X';
        }
    }

    /** Gospel: down-strum on the beat, up-strum on the "&", 8ths only, each bar its own holes. */
    void paintGospelMotif (LeadPattern& p, Random& r)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            int placed = 0;
            for (int eighth = 0; eighth < 8; ++eighth)
            {
                const int step = bar * 16 + eighth * 2;
                const int roll = (int) (r.next() % 100);
                if (roll < 20)
                    continue;
                const bool down = eighth % 2 == 0;
                p[down ? Lane::hook : Lane::answer][(size_t) step] = (down && roll > 55) ? 'X' : 'x';
                ++placed;
            }
            if (placed < 3)
            {
                p[Lane::hook][(size_t) (bar * 16)] = 'X';
                p[Lane::answer][(size_t) (bar * 16 + 2)] = 'x';
                p[Lane::hook][(size_t) (bar * 16 + 8)] = 'X';
            }
        }
    }

    /** A few notes on the 8ths, behind the beat of a busy grid. The two bars are not copies. */
    void paintLaidBack (LeadPattern& p, Random& r, Lane lane)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            int placed = 0;
            for (int eighth = 0; eighth < 8; ++eighth)
            {
                const int roll = (int) (r.next() % 100);
                if (roll < 70)
                    continue;
                p[lane][(size_t) (bar * 16 + eighth * 2)] = roll > 88 ? 'X' : 'x';
                ++placed;
            }
            if (placed == 0)
                p[lane][(size_t) (bar * 16)] = 'X';
        }
    }

    /** One plain hit rests in each bar, so two clicks that drew the same cell still differ. */
    void omitOnePerBar (LeadPattern& p, Random& r)
    {
        for (int bar = 0; bar < 2; ++bar)
        {
            int lanes[48], steps[48], n = 0;
            for (int l = 0; l < numLanes; ++l)
            {
                if (l == (int) Lane::scratch || l == (int) Lane::root || l == (int) Lane::lick)
                    continue;
                const auto& line = p.lanes[(size_t) l];
                for (int i = bar * 16 + 1; i < bar * 16 + 16 && i < (int) line.size() && n < 48; ++i)
                    if (isHit (line[(size_t) i]))
                    {
                        lanes[n] = l;
                        steps[n] = i;
                        ++n;
                    }
            }
            if (n >= 3)
            {
                const int k = (int) (r.next() % (std::uint32_t) n);
                p.lanes[(size_t) lanes[k]][(size_t) steps[k]] = '.';
            }
        }
    }

    /** The main part a genre's soloist plays, and how likely each one is. */
    enum Role { funk16 = 0, offStaccato, chankLine, arpPick, swellPad, powerDrive, octaveDisco, strum8, twoStep, numRoles };

    struct GenreStyle
    {
        int weights[numRoles];
        float scratch;   // muted scratches filling the 16ths
        float root;      // a root note on the low string
        float lick;      // a lick answering at the end of bar 2
    };

    GenreStyle styleOf (int genre)
    {
        //                        funk stab chank arp swell power oct strum8 2step
        switch (genre)
        {
            case pianoHouse:  return { { 0, 14, 1, 0, 0, 0, 0, 0, 0 }, 0.2f,  0.25f, 0.05f };
            case soulful:     return { { 0, 2, 0, 4, 6, 0, 0, 0, 1 }, 0.05f, 0.3f,  0.3f };
            case deep:        return { { 0, 1, 0, 4, 7, 0, 0, 0, 1 }, 0.0f,  0.2f,  0.1f };
            case gospel:      return { { 0, 0, 0, 0, 0, 0, 0, 1, 0 }, 0.0f,  0.45f, 0.3f };
            case jackin:      return { { 4, 0, 8, 0, 0, 0, 2, 0, 0 }, 0.95f, 0.15f, 0.05f };
            case jackinPiano: return { { 0, 14, 0, 0, 0, 0, 0, 0, 0 }, 0.15f, 0.25f, 0.0f };
            case garage:      return { { 0, 0, 1, 0, 1, 0, 0, 0, 10 }, 0.25f, 0.2f,  0.1f };
            case afro:        return { { 0, 0, 2, 10, 0, 0, 0, 0, 0 }, 0.08f, 0.55f, 0.12f };
            case funky:       return { { 7, 0, 6, 0, 0, 0, 3, 0, 0 }, 0.95f, 0.3f,  0.12f };
            case nineties:    return { { 0, 12, 0, 0, 0, 0, 0, 0, 0 }, 0.1f,  0.3f,  0.0f };
            case neoSoul:     return { { 0, 2, 0, 3, 2, 0, 0, 0, 5 }, 0.0f,  0.2f,  0.95f };
            case chill:       return { { 0, 0, 0, 4, 7, 0, 0, 0, 0 }, 0.0f,  0.12f, 0.9f };
            case dance:       return { { 0, 12, 0, 0, 0, 0, 0, 0, 0 }, 0.0f,  0.0f,  0.0f };
            default:          return { { 2, 4, 3, 2, 1, 1, 1, 2, 1 }, 0.3f,  0.3f, 0.2f };
        }
    }

    /** Bar 2 answers bar 1 without leaving its metric class (a hit moves by a quarter note). */
    void varySecondBar (LeadPattern& p, Random& r)
    {
        auto same = [] (const std::string& line)
        {
            return line.size() >= 32 && line.compare (0, 16, line, 16, 16) == 0;
        };
        for (int l = 0; l < numLanes; ++l)
        {
            auto& line = p.lanes[(size_t) l];
            if ((int) line.size() < 32)
                continue;
            for (int n = 0; n < 3; ++n)
            {
                const int from = 16 + r.nextInt (16);
                if (! isHit (line[(size_t) from]))
                    continue;
                const int cls = from % 4;
                const int to = 16 + cls + 4 * r.nextInt (4);
                if (to != from && line[(size_t) to] == '.')
                {
                    line[(size_t) to] = line[(size_t) from];
                    if (r.chance (0.65f))
                        line[(size_t) from] = '.';
                }
            }
        }
        for (int l = 0; l < numLanes; ++l)
        {
            auto& line = p.lanes[(size_t) l];
            if (! same (line))
                return;
            for (int i = 0; i < 16; ++i)
            {
                if (! isHit (line[(size_t) i]))
                    continue;
                const int cls = i % 4;
                for (int k = 0; k < 4; ++k)
                {
                    const int to = 16 + (cls + 4 * k) % 16;
                    if (line[(size_t) to] == '.')
                    {
                        line[(size_t) to] = line[(size_t) i] == 'X' ? 'x' : line[(size_t) i];
                        return;
                    }
                }
                line[(size_t) (16 + i)] = '.';
                return;
            }
        }
    }

    bool gridHasSixteenths (const LeadPattern& p)
    {
        for (const auto& line : p.lanes)
            for (size_t i = 0; i < line.size(); ++i)
                if (isHit (line[i]) && i % 2 == 1)
                    return true;
        return false;
    }

    /** At low density some plain hits (never the accents, never the first of the bar) rest. */
    void thin (std::string& line, Random& r, float density)
    {
        const float drop = std::max (0.0f, 0.45f - density) * 0.9f;
        for (size_t i = 1; i < line.size(); ++i)
            if ((line[i] == 'x' || line[i] == 'g') && i % 16 != 0 && r.chance (drop))
                line[i] = '.';
    }

    /** Every groove needs one accent a player can hear. Ghosts stay ghosts. A plain hit on a
        weak 16th of an accent lane is promoted only when the bar has nothing stronger, so the
        "&" and the downbeat stay louder than the 16ths around them. */
    /** Every random groove has a main part a band can hear: a chord lane (strum, stab, chank
        or power chord) with plain hits. A groove of swells, a few arpeggio notes and scratches
        sounded like nothing at all; it gets a sparse chord in the genre's own lane, around
        what is there (its own dice, so the rest of the groove stays as it was). */
    void ensureChordPart (LeadPattern& p, int genre, std::uint32_t seed)
    {
        for (auto lane : { Lane::hook, Lane::answer, Lane::stab, Lane::chank, Lane::power })
            for (char c : p[lane])
                if (c == 'x' || c == 'X')
                    return;

        Random r (seed * 2654435761u + 0x9E3779B9u);
        const Lane lane = genre == dance ? Lane::power : genre == gospel ? Lane::hook : Lane::stab;
        const auto cell = compact (r.pick ({ "X... .... .... ....", "X... .... ..x. ....", "X... .... .... ..x.",
                                             "X... ..x. .... ....", "X... .... x... ...." }));
        auto& line = p[lane];
        for (size_t i = 0; i < line.size(); ++i)
            if (cell[i % cell.size()] != '.' && line[i] == '.')
                line[i] = cell[i % cell.size()];
    }

    void ensureAudiblePeak (LeadPattern& p, int genre)
    {
        for (int l = 0; l < numLanes; ++l)
            if (l != (int) Lane::scratch)
                for (char c : p.lanes[(size_t) l])
                    if (c == 'X')
                        return;

        auto scoreOf = [] (int lane, int step)
        {
            const bool accent = lane == (int) Lane::answer || lane == (int) Lane::chank || lane == (int) Lane::arp;
            const int cls = step % 4;
            int score = accent ? (cls == 0 || cls == 2 ? 40 : 15) : 70;
            if (lane == (int) Lane::hook || lane == (int) Lane::stab || lane == (int) Lane::power
                || lane == (int) Lane::root || lane == (int) Lane::octave)
                score += 10;
            if (lane == (int) Lane::swell)
                score -= 15;
            if (cls == 0)
                score += 8;
            else if (cls == 2)
                score += 5;
            return score;
        };

        bool promoted = false;
        const int bars = std::max (1, (int) p.lanes[0].size() / 16);
        for (int bar = 0; bar < bars; ++bar)
        {
            int bestLane = -1, bestStep = -1, bestScore = -1;
            for (int l = 0; l < numLanes; ++l)
            {
                if (l == (int) Lane::scratch)
                    continue;
                const auto& line = p.lanes[(size_t) l];
                for (int i = bar * 16; i < bar * 16 + 16 && i < (int) line.size(); ++i)
                {
                    const char c = line[(size_t) i];
                    if (c != 'x' && c != 'f')
                        continue;
                    const int score = scoreOf (l, i);
                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestLane = l;
                        bestStep = i;
                    }
                }
            }
            if (bestLane >= 0)
            {
                p.lanes[(size_t) bestLane][(size_t) bestStep] = 'X';
                promoted = true;
            }
        }
        if (promoted)
            return;

        const Lane lane = genre == dance ? Lane::power : genre == gospel ? Lane::hook : Lane::stab;
        if (! p[lane].empty())
            p[lane][0] = 'X';
        if ((int) p[lane].size() > 16 && p[lane][16] == '.')
            p[lane][16] = 'x';
    }
}

namespace
{
    LeadPattern randomPattern (std::uint32_t seed, float density, int genre, bool withChordPart);
}

LeadPattern makeRandomPattern (std::uint32_t seed, float density, int genre)
{
    return randomPattern (seed, density, genre, true);
}

namespace
{
LeadPattern randomPattern (std::uint32_t seed, float density, int genre, bool withChordPart)
{
    // One main part from that genre's cells, then a root, a scratch and a lick only when the
    // genre plays them. Bar 2 starts as bar 1 and then answers inside the same metric class.
    Random r (seed);
    density = std::clamp (density, 0.0f, 1.0f);
    genre = std::clamp (genre, 0, (int) grooveCategories().size() - 1);
    const auto style = styleOf (genre);
    const float more = 0.35f + 1.1f * density;

    LeadPattern p;
    p.name = "Random groove";
    p.category = grooveCategories()[(size_t) genre];
    for (auto& line : p.lanes)
        line.assign ((size_t) randomBeatLength, '.');

    auto setBar = [&p] (Lane lane, const char* cell)   // one bar, written into both bars
    {
        const auto c = compact (cell);
        for (int i = 0; i < 16 && i < (int) c.size(); ++i)
            p[lane][(size_t) i] = p[lane][(size_t) (i + 16)] = c[(size_t) i];
    };

    int total = 0;
    for (int w : style.weights) total += w;
    int x = r.nextInt (std::max (1, total)), role = 0;
    for (; role < numRoles - 1; ++role)
        if ((x -= style.weights[role]) < 0)
            break;

    const bool houseStaccato = genre == pianoHouse || genre == jackinPiano || genre == nineties;
    const bool sparseGenre = genre == soulful || genre == deep || genre == neoSoul || genre == chill;
    bool scratchSuits = true;
    switch ((Role) role)
    {
        case funk16:
            if (genre == funky)
            {
                paintTwoBars (p, Lane::hook, r, funkDown);
                paintTwoBars (p, Lane::answer, r, funkUp);
            }
            else
            {
                paintTwoBars (p, Lane::hook, r, downCells);
                paintTwoBars (p, Lane::answer, r, upCells);
            }
            break;
        case offStaccato:
            if (genre == dance)
                paintDanceStaccatos (p, r);
            else if (houseStaccato)
                paintOffbeatStaccatos (p, r, genre);
            else if (genre == neoSoul)
                paintLaidBack (p, r, Lane::stab);
            else if (sparseGenre)
                paintTwoBars (p, Lane::stab, r, neoCells);
            else
                paintTwoBars (p, Lane::stab, r, stabCells);
            break;
        case chankLine:
            if (genre == funky || genre == jackin)
                paintTwoBars (p, Lane::chank, r, jackPulse);
            else if (genre == afro)
                paintTwoBars (p, Lane::chank, r, afroCells);
            else
                paintTwoBars (p, Lane::chank, r, chankCells);
            if (genre != funky && genre != jackin && genre != afro && r.chance (0.3f * more))
                setBar (Lane::stab, "X... .... .... ....");
            break;
        case arpPick:
            if (genre == afro)
                paintTwoBars (p, Lane::arp, r, afroCells);
            else if (genre == neoSoul)
                paintLaidBack (p, r, Lane::arp);
            else if (sparseGenre)
                paintTwoBars (p, Lane::arp, r, sparseArp);
            else
                paintTwoBars (p, Lane::arp, r, arpCells);
            if (genre != afro && r.chance (0.5f))
                paintTwoBars (p, Lane::swell, r, swellCells);
            scratchSuits = false;
            break;
        case swellPad:
            paintTwoBars (p, Lane::swell, r, swellCells);
            if (genre == neoSoul)
                paintLaidBack (p, r, r.chance (0.5f) ? Lane::arp : Lane::stab);
            else if (sparseGenre)
                paintTwoBars (p, r.chance (0.5f) ? Lane::arp : Lane::stab, r, sparseArp);
            else
                setBar (r.chance (0.5f) ? Lane::chank : Lane::arp,
                        r.chance (0.5f) ? ".... ..x. .... ..x." : ".... .... x.x. x.x.");
            scratchSuits = false;
            break;
        case powerDrive:
            paintTwoBars (p, Lane::power, r, powerCells);
            scratchSuits = false;
            break;
        case octaveDisco:
            paintTwoBars (p, Lane::octave, r, octaveCells);
            break;
        case strum8:
            if (genre == gospel)
                paintGospelMotif (p, r);
            else
            {
                paintTwoBars (p, Lane::hook, r, eighthDownCells);
                paintTwoBars (p, Lane::answer, r, eighthUpCells);
            }
            break;
        case twoStep:
        default:
            if (genre == garage)
                paintTwoBars (p, Lane::stab, r, garageSkips);
            else if (genre == neoSoul)
                paintLaidBack (p, r, Lane::stab);
            else if (sparseGenre)
                paintTwoBars (p, Lane::stab, r, neoCells);
            else
                paintTwoBars (p, Lane::stab, r, twoStepCells);
            break;
    }
    for (auto& line : p.lanes)
        thin (line, r, density);

    // ---- The muted scratch between the hits, the root, a lick ----
    const bool scratchSure = style.scratch >= 0.8f;
    if (scratchSuits && (scratchSure || r.chance (style.scratch * more)))
    {
        if (genre == funky || genre == jackin)
            paintTwoBars (p, Lane::scratch, r, funkScratch);
        else
        {
            // A busy groove does not always scratch every 16th: often the hand leaves holes.
            if (density > 0.6f && r.chance (0.6f))
                paintTwoBars (p, Lane::scratch, r, funkScratch);
            else
                setBar (Lane::scratch, density > 0.6f ? "gggg gggg gggg gggg" : r.chance (0.5f) ? ".g.g .g.g .g.g .g.g" : "g... g... g... g.g.");
        }
    }
    if (role != powerDrive && role != octaveDisco && genre != dance && ! houseStaccato && r.chance (style.root * more))
    {
        if (genre == afro)
            paintTwoBars (p, Lane::root, r, afroRoot);
        else if (genre == gospel)
            paintTwoBars (p, Lane::root, r, gospelRoot);
        else
            paintTwoBars (p, Lane::root, r, rootCells);
    }
    const float lickChance = style.lick >= 0.7f ? style.lick : style.lick * more;
    if (style.lick > 0.0f && r.chance (lickChance))
    {
        const auto lick = compact (genre == gospel ? pickCell (r, gospelRuns) : pickCell (r, lickCellsExtra));
        for (int i = 0; i < 16; ++i)
            if (lick[(size_t) i] != '.')
                p[Lane::lick][(size_t) (16 + i)] = lick[(size_t) i];
    }

    // Dance: now and then octaves on the beats the stabs leave free (the chord on the "&").
    if (genre == dance && r.chance (0.4f))
        for (int i = 0; i < randomBeatLength; i += 4)
            if (! isHit (p[Lane::stab][(size_t) i]) && (i % 16 == 0 || r.chance (0.7f)))
                p[Lane::octave][(size_t) i] = i % 16 == 0 ? 'X' : 'x';

    // A pickup on the last 8th. A 16th answer only in the genres that already play 16ths.
    if (genre == gospel && r.chance (0.45f))
        p[Lane::answer][30] = 'x';
    else if (genre == dance && r.chance (0.35f))
        p[Lane::stab][28] = 'X';
    else if (genre == funky || genre == jackin || genre == afro)
        if (r.chance (0.3f + 0.4f * density))
            switch (r.nextInt (3))
            {
                case 0:  p[Lane::slide][30] = 'x'; break;
                case 1:  if (p[Lane::answer][genre == afro ? 30 : 31] == '.') p[Lane::answer][genre == afro ? 30 : 31] = 'x'; break;
                default: p[Lane::scratch][28] = p[Lane::scratch][29] = 'x'; break;
            }

    const bool procedural = houseStaccato || genre == dance || genre == gospel || genre == neoSoul;
    if (! procedural)
    {
        omitOnePerBar (p, r);
        // Flip one accent in each bar so two draws of the same cell are not the same click.
        for (int bar = 0; bar < 2; ++bar)
        {
            int lanes[48], steps[48], n = 0;
            for (int l = 0; l < numLanes; ++l)
            {
                if (l == (int) Lane::scratch || l == (int) Lane::lick)
                    continue;
                auto& line = p.lanes[(size_t) l];
                for (int i = bar * 16; i < bar * 16 + 16 && i < (int) line.size() && n < 48; ++i)
                    if (line[(size_t) i] == 'x' || line[(size_t) i] == 'X')
                    {
                        lanes[n] = l;
                        steps[n] = i;
                        ++n;
                    }
            }
            if (n > 0)
            {
                const int k = (int) (r.next() % (std::uint32_t) n);
                auto& symbol = p.lanes[(size_t) lanes[k]][(size_t) steps[k]];
                symbol = symbol == 'X' ? 'x' : 'X';
            }
        }
    }
    varySecondBar (p, r);
    if (sparseGenre && r.chance (0.5f))
    {
        // Soul, deep and neo-soul players anticipate: one chord on a beat (not the bar's first)
        // comes a 16th early, so the space between the hits is not always on the quarter.
        int lanes[16], steps[16], n = 0;
        for (auto lane : { Lane::stab, Lane::arp, Lane::chank, Lane::hook })
            for (int i = 4; i < randomBeatLength && n < 16; i += 4)
                if (i % 16 != 0 && isHit (p[lane][(size_t) i]) && p[lane][(size_t) (i - 1)] == '.')
                {
                    lanes[n] = (int) lane;
                    steps[n] = i;
                    ++n;
                }
        if (n > 0)
        {
            const int k = r.nextInt (n);
            auto& line = p.lanes[(size_t) lanes[k]];
            line[(size_t) (steps[k] - 1)] = line[(size_t) steps[k]];
            line[(size_t) steps[k]] = '.';
        }
    }
    int kept = 0;
    for (const auto& line : p.lanes)
        kept += (int) std::count_if (line.begin(), line.end(), isHit);
    if (kept < 2)
    {
        const Lane lane = genre == dance ? Lane::power : genre == gospel ? Lane::hook : Lane::stab;
        p[lane][0] = 'X';
        if (p[lane][16] == '.')
            p[lane][16] = 'x';
    }
    if (withChordPart)
        ensureChordPart (p, genre, seed);
    ensureAudiblePeak (p, genre);
    normalisePattern (p, randomBeatLength);
    return p;
}
}

std::string makeRandomLane (Lane lane, std::uint32_t seed, float density, int genre)
{
    // The genre's own idea of this lane first, from a few grooves...
    for (std::uint32_t attempt = 0; attempt < 12; ++attempt)
    {
        // (without the chord part a whole groove gets: that would be this lane's idea, not the genre's)
        const auto groove = randomPattern (seed * 31u + attempt * 7919u + (std::uint32_t) lane, density, genre, false);
        const auto& line = groove[lane];
        if (hits (line))
            return line;
    }

    // ...otherwise a light part that fits any house groove.
    Random r (seed + 99u);
    static const char* cells[numLanes][3] {
        { "X... .... X... ....", "X... ..x. ..x. x...", "X... ..x. X... x.x." },        // down strum
        { ".... ..x. .... ..x.", "..x. ..x. ..x. ..x.", ".x.x ...x .x.. .x.x" },        // up strum
        { "..X. .... ..X. ....", "..X. ..X. ..X. ..X.", "X..X ..X. .X.. X.X." },        // stab
        { "..x. ..x. ..x. ..x.", "..xx ..x. ..xx ..x.", "x.xx .x.x x.xx .x.x" },        // chank
        { "g... g... g... g...", ".g.g .g.g .g.g .g.g", "gggg gggg gggg gggg" },        // scratch
        { ".... .... .... ..X.", "...X .... .... ..x.", "...X .... ..x. ..X." },        // slide
        { "X... .... .... ....", "X... .... .... ....", "X... .... X... ...." },        // swell
        { "x... x... x... x...", "x.x. x.x. x.x. x.x.", "x.xx x.xx x.xx x.xx" },        // arpeggio
        { ".... .... .... x.x.", ".... ..x. .... .x.x", "..x. .x.. .xxx x..." },        // lick
        { "x... .... x... ....", "x..x ..x. x..x ..x.", "X.x. x.x. X.x. x.x." },        // octaves
        { "X... .... X... ....", "X... ..X. ..X. X...", "X.x. X.x. X.x. X.x." },        // power chord
        { "X... .... .... ....", "X... .... X... ....", "X... .... X..x ...." },        // root
    };
    const int choice = std::clamp ((int) (density * 2.99f) + (r.chance (0.3f) ? (r.chance (0.5f) ? -1 : 1) : 0), 0, 2);
    LeadPattern p;
    for (auto& l : p.lanes)
        l.assign ((size_t) randomBeatLength, '.');
    // Bar 2 is now and then the next lighter or busier cell, then answers bar 1 (a hit moves by
    // a quarter note), and a plain hit of a busy bar may rest: the same few cells, many lines.
    const int other = std::clamp (choice + (r.chance (0.45f) ? (r.chance (0.5f) ? -1 : 1) : 0), 0, 2);
    p[lane] = compact (cells[(int) lane][choice]) + compact (cells[(int) lane][other]);
    varySecondBar (p, r);
    auto& line = p[lane];
    for (int bar = 0; bar < 2; ++bar)
    {
        int steps[16], n = 0;
        for (int i = bar * 16 + 1; i < bar * 16 + 16; ++i)
            if (line[(size_t) i] == 'x' || line[(size_t) i] == 'X')
                steps[n++] = i;
        if (n >= 3 && r.chance (0.5f))
            line[(size_t) steps[r.nextInt (n)]] = '.';
    }
    if (genre == gospel || genre == dance)
    {
        // These genres stay on the 8ths: a 16th moves to the 8th before it, or rests.
        for (size_t i = 1; i < line.size(); i += 2)
            if (isHit (line[i]))
            {
                if (line[i - 1] == '.')
                    line[i - 1] = line[i];
                line[i] = '.';
            }
    }
    else if (density > 0.5f && lane != Lane::swell && lane != Lane::root)
        line[(size_t) (16 + r.pick ({ 11, 13, 14, 15 }))] = 'g';
    if (! hits (line))
        line[0] = 'X';
    return line;
}

LeadPattern makeVariation (const LeadPattern& source, std::uint32_t seed)
{
    Random r (seed * 2654435761u + 17u);
    LeadPattern p = source;
    const int len = p.length();
    if (len < 16)
        return p;

    const int genre = categoryIndex (p.category);
    const bool known = genre >= 0;
    const auto style = known ? styleOf (genre) : GenreStyle { { 2, 4, 3, 2, 1, 1, 1, 2, 1 }, 0.3f, 0.3f, 0.2f };
    const bool grid16 = gridHasSixteenths (p);
    const bool allowScratch = hits (source[Lane::scratch]) || ! known || style.scratch >= 0.25f;
    const bool allowRun = hits (source[Lane::lick]) || ! known || style.lick >= 0.15f;
    auto onGrid = [&] (int step) { return grid16 || step % 2 == 0; };

    const int bar = len - 16;   // the last bar plays the turnaround
    auto at = [&p, bar] (Lane lane, int step) -> char& { return p[lane][(size_t) (bar + step)]; };
    auto clearChordsFrom = [&] (int step)
    {
        for (int l = 0; l < numLanes; ++l)
            if (isChordLane (l))
                for (int i = step; i < 16; ++i)
                    at ((Lane) l, i) = '.';
    };

    bool did = false;
    const int moves = 1 + r.nextInt (2);
    for (int m = 0; m < moves; ++m)
    {
        switch (r.nextInt (5))
        {
            case 0:   // a slide into the next bar (the last 8th: on an 8th grid and a 16th one)
                clearChordsFrom (14);
                at (Lane::slide, 14) = 'X';
                did = true;
                break;
            case 1:   // a muted scratch run, only on this groove's grid, and only if scratches belong
                if (! allowScratch)
                    break;
                for (int i = 12; i < 16; ++i)
                    if (onGrid (i) && at (Lane::hook, i) == '.' && at (Lane::stab, i) == '.')
                        at (Lane::scratch, i) = i % 4 == 0 ? 'X' : 'x';
                did = true;
                break;
            case 2:   // an up-strum pick-up on the last 16th, or the last 8th when the groove is straight 8ths
                at (Lane::answer, grid16 ? 15 : 14) = 'x';
                did = true;
                break;
            case 3:
                if (! allowRun)
                    break;
                at (Lane::lick, 12) = 'x';
                at (Lane::lick, 14) = 'x';
                if (grid16)
                    at (Lane::lick, 15) = 'x';
                did = true;
                break;
            default:  // a stop and a stab on the last step of this grid
                clearChordsFrom (12);
                at (Lane::stab, grid16 ? 15 : 14) = 'X';
                did = true;
                break;
        }
    }
    if (! did)
        at (Lane::stab, grid16 ? 15 : 14) = 'X';
    return p;
}

LeadPattern makeSectionVariation (const LeadPattern& source, std::uint32_t seed)
{
    Random r (seed * 747796405u + 2891336453u);
    LeadPattern p = source;
    const int len = p.length();
    const int genre = categoryIndex (p.category);
    const bool known = genre >= 0;
    const auto style = known ? styleOf (genre) : GenreStyle { { 2, 4, 3, 2, 1, 1, 1, 2, 1 }, 0.3f, 0.3f, 0.2f };
    const bool grid16 = gridHasSixteenths (p);
    const bool allowScratch = hits (source[Lane::scratch]) || ! known || style.scratch >= 0.25f;
    const bool allowRun = hits (source[Lane::lick]) || ! known || style.lick >= 0.15f;

    // Light hits move only onto a step of the same class (8th vs 16th, downbeat vs upbeat).
    bool moved = false;
    for (auto lane : { Lane::scratch, Lane::answer, Lane::chank, Lane::arp })
        if (hits (p[lane]) && r.chance (0.6f))
        {
            auto& line = p[lane];
            for (int bar = 0; bar < len / 16; ++bar)
            {
                const int from = bar * 16 + r.nextInt (16);
                const int cls = from % 4;
                if (cls == 0 || (! grid16 && cls % 2 == 1))
                    continue;
                const int to = bar * 16 + cls + 4 * r.nextInt (4);
                if (from != to)
                    std::swap (line[(size_t) from], line[(size_t) to]);
                moved = true;
            }
        }
    if (! moved && allowScratch)
        for (int bar = 0; bar < len / 16; ++bar)
            p[Lane::scratch][(size_t) (bar * 16 + (grid16 ? r.pick ({ 3, 7, 11, 15 }) : r.pick ({ 2, 6, 10, 14 })))] = 'g';
    else if (! moved)
        for (int l = 0; l < numLanes && ! moved; ++l)
        {
            auto& line = p.lanes[(size_t) l];
            for (int i = 0; i < len; ++i)
            {
                if (! isHit (line[(size_t) i]) || i % 4 == 0 || (! grid16 && i % 2 == 1))
                    continue;
                const int to = (i / 16) * 16 + (i % 4) + 4 * r.nextInt (4);
                if (to != i && to < len && line[(size_t) to] == '.')
                {
                    line[(size_t) to] = line[(size_t) i];
                    line[(size_t) i] = '.';
                    moved = true;
                    break;
                }
            }
        }

    if (allowRun && r.chance (0.4f) && len >= 16)
        p[Lane::lick][(size_t) (r.nextInt (len / 16) * 16 + (grid16 ? r.pick ({ 7, 11, 13, 15 }) : r.pick ({ 6, 10, 14 })))] = 'x';

    return makeVariation (p, seed + 7);
}

LeadPattern applySegmentOrder (const LeadPattern& source, const std::array<int, numSegments>& order)
{
    const int len = source.length();
    if (len % numSegments != 0 || order == std::array<int, numSegments> { 0, 1, 2, 3 })
        return source;

    LeadPattern p = source;
    const int segment = len / numSegments;
    for (int k = 0; k < numSegments; ++k)
    {
        const int from = std::clamp (order[(size_t) k], 0, numSegments - 1);
        for (int l = 0; l < numLanes; ++l)
            for (int i = 0; i < segment; ++i)
                p.lanes[(size_t) l][(size_t) (k * segment + i)] = source.lanes[(size_t) l][(size_t) (from * segment + i)];
    }
    return p;
}

//==============================================================================
const char* fillStyleName (int style) noexcept
{
    static const char* names[] { "Motif build", "High run", "Ghost build", "Pulse lift", "Drop out", "Approach up", "Mix",
                                 "Staccato stutter", "Melodic run", "Arpeggio run", "Low run", "Long note", "Custom" };
    return style >= 0 && style < numFillStyles ? names[style] : "";
}

int fillStartStep (FillStyle style) noexcept
{
    switch (style)
    {
        case FillStyle::motifBuild:
        case FillStyle::ghostBuild:
        case FillStyle::melodyRun:
        case FillStyle::arpeggioRun:
        case FillStyle::longOut:  return 8;
        case FillStyle::custom:    return 0;   // from its first written step (see the engine)
        case FillStyle::highRun:
        case FillStyle::pulseLift:
        case FillStyle::dropOut:
        case FillStyle::approachUp:
        case FillStyle::noteStutter:
        case FillStyle::lowRun:
        case FillStyle::mix:
        default:                   return 12;
    }
}

const std::vector<ArrangementPart>& defaultArrangement()
{
    static const std::vector<ArrangementPart> parts {
        { Section::intro, 8 }, { Section::main, 16 }, { Section::breakdown, 8 },
        { Section::buildup, 4 }, { Section::drop, 16 }, { Section::outro, 8 },
    };
    return parts;
}

} // namespace fgl
