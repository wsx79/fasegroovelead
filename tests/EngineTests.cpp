// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GrooveEngine.h"
#include "TestSupport.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <set>
using namespace fgl;

static void validate (const std::vector<NoteEvent>& events)
{
    std::set<int> notes;
    double previous = -1e9;
    for (const auto& e : events)
    {
        CHECK (e.ppq >= previous); previous = e.ppq;
        CHECK (e.note >= 0 && e.note <= 127);
        if (e.isController() || e.lane == keyswitchLane) continue;
        if (e.isNoteOn())
        {
            CHECK (notes.empty());
            CHECK (e.velocity > 0 && e.velocity <= 127);
            notes.insert (e.note);
        }
        else { CHECK (notes.count (e.note) == 1); notes.erase (e.note); }
    }
    CHECK (notes.empty());
}
static bool equal (const std::vector<NoteEvent>& a, const std::vector<NoteEvent>& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::abs (a[i].ppq-b[i].ppq)>1e-8 || a[i].note!=b[i].note
            || a[i].velocity!=b[i].velocity || a[i].lane!=b[i].lane) return false;
    return true;
}
int main()
{
    LeadSettings s;
    // A global register change preserves rhythm, dynamics and relative lane offsets.
    s.roomForBand=false;s.fillEveryBars=0;s.variationEvery=0;
    const auto original=GrooveEngine::renderClip(s,8);
    s.globalOctave=1;const auto higher=GrooveEngine::renderClip(s,8);
    CHECK(original.size()==higher.size());
    for(size_t i=0;i<original.size();++i){CHECK(original[i].ppq==higher[i].ppq);CHECK(original[i].velocity==higher[i].velocity);CHECK(original[i].lane==higher[i].lane);if(!original[i].isController()&&original[i].lane!=keyswitchLane)CHECK(higher[i].note==original[i].note+12);}
    CHECK(s.octaves==LeadSettings().octaves);
    s=LeadSettings();
    for (int p = 0; p < (int) leadPatterns().size(); ++p)
    {
        s.pattern = p;
        auto e = GrooveEngine::renderClip (s, 32); CHECK (!e.empty()); validate (e);
    }
    s.pattern = randomPatternIndex();
    for (int genre = 0; genre < (int) grooveCategories().size(); ++genre)
        for (int seed = 1; seed <= 8; ++seed)
            for (float density : {0.0f, 0.5f, 1.0f})
            {
                s.randomGenre = genre; s.riffSeed = (std::uint32_t) seed; s.density = density;
                for (int section = 0; section < numSections; ++section)
                {
                    s.section = (Section) section;
                    auto e = GrooveEngine::renderClip (s, 16); CHECK (!e.empty()); validate (e);
                    CHECK (equal (e, GrooveEngine::renderClip (s, 16)));
                }
            }
    s = LeadSettings(); s.pattern = randomPatternIndex(); s.humanize = 0.5f; s.swing = 0.7f;
    s.attackMs = 28; s.fillEveryBars = 2; s.variationEvery = 4;
    for (int fill = 0; fill < numFillStyles; ++fill)
    {
        s.fillStyle = (FillStyle) fill;
        auto whole = GrooveEngine::renderClip (s, 32); validate (whole);
        GrooveEngine engine; std::vector<NoteEvent> chunks;
        for (int i = 0; i < 2048; ++i) engine.render (i / 64.0, (i+1) / 64.0, s, chunks);
        engine.allNotesOff (32, chunks);
        CHECK (equal (whole, chunks)); validate (chunks);
    }
    s = LeadSettings(); s.pattern = userPatternIndex();
    for (int lane = 0; lane < numLanes; ++lane)
    {
        for (auto& row : s.user) row.fill ('.');
        s.user[(size_t) lane][0] = 'X';
        auto e = GrooveEngine::renderClip (s, 8); CHECK (!e.empty()); validate (e);
        s.muted[(size_t) lane] = true;
        CHECK (GrooveEngine::renderClip (s, 8).empty());
        s.muted[(size_t) lane] = false;
    }
    // Tonal context is effective on passing notes and seed changes the motif.
    NoteList a, b;
    const auto chord = tonesOf (Chord{}); const auto v = voiceChord (chord);
    laneNotes (Lane::lick, chord, v, 3, 0, a, false, false, 0, 0, 0, 1);
    laneNotes (Lane::lick, chord, v, 3, 0, b, false, false, 0, 0, 2, 1);
    CHECK (a.size() == 1 && b.size() == 1);
    bool seedChanges = false;
    for (int i = 0; i < 32; ++i)
    {
        laneNotes (Lane::hook, chord, v, i, 0, a, false, false, 0, 0, 0, 1);
        laneNotes (Lane::hook, chord, v, i, 0, b, false, false, 0, 0, 0, 987);
        seedChanges = seedChanges || !(a == b);
    }
    CHECK (seedChanges);
    s = LeadSettings(); s.keyswitches = true;
    validate (GrooveEngine::renderClip (s, 16));
    std::vector<GrooveEngine::Marker> markers;
    validate (GrooveEngine::renderArrangement (s, defaultArrangement(), &markers));
    CHECK (markers.size() == defaultArrangement().size());
    std::printf ("Lead engine: %d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
