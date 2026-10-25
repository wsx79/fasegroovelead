// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>

// Canonical palette from Fase Groove Arranger 0.4.7. Existing plugin presets
// keep their original indexes and behaviour; these are explicit new choices.
namespace fgfamily {
struct GenreSound {
    int piano,bass,guitar,kit,voicing,strings;
    double pianoTone,bassTone,guitarTone,drumTone,gate,strum,density;
    double pianoGain,bassGain,stringsGain,guitarGain;
};
inline constexpr int numGenres=13;
inline constexpr std::array<const char*,numGenres> names{
    "Arranger: Piano House", "Arranger: Soulful", "Arranger: Deep", "Arranger: Gospel",
    "Arranger: Jackin'", "Arranger: Jackin' Piano", "Arranger: Garage & UK", "Arranger: Afro & Latin",
    "Arranger: Funky & Disco", "Arranger: '90s Piano House", "Arranger: Neo Soul & Broken",
    "Arranger: Chill & Pads", "Arranger: Dance Stabs"
};
inline constexpr std::array<GenreSound,numGenres> sounds{{

        {1,3,0,0,0,6, .68,.16,.48,.58,.48,.12,.57, .56,.86,.17,.20}, // piano house: grand / 909
        {0,0,1,2,1,3, .38,.28,.32,.42,.67,.25,.50, .60,.84,.20,.30}, // soulful: Rhodes / live kit
        {3,3,1,1,2,2, .24,.05,.24,.28,.42,.08,.38, .47,.92,.18,.16}, // deep: organ / 808
        {5,0,1,2,1,6, .57,.30,.42,.48,.72,.28,.56, .48,.82,.22,.19}, // gospel: Leslie / live kit
        {1,4,0,0,0,0, .72,.54,.65,.68,.18,.00,.62, .56,.80,.12,.22}, // jackin: dry piano chops / articulate electric bass / 909
        {2,3,0,0,0,5, .75,.20,.60,.66,.30,.08,.64, .53,.86,.12,.21}, // piano jack: M1 / 909
        {0,3,0,1,2,5, .49,.04,.52,.62,.24,.05,.58, .48,.98,.12,.23}, // UK: chopped Rhodes / 808 sub
        {4,3,3,2,4,2, .35,.10,.40,.48,.52,.24,.48, .40,.88,.17,.35}, // Afro: dark keys / nylon / percussion
        {4,4,2,2,6,9, .61,.52,.70,.60,.40,.16,.61, .42,.78,.31,.48}, // disco: Wurli / chorus guitar / strings
        {2,3,0,0,0,1, .83,.22,.62,.70,.50,.08,.60, .60,.87,.20,.15}, // 90s: M1 / 909
        {0,4,1,3,2,3, .28,.35,.28,.22,.78,.34,.42, .58,.78,.18,.28}, // neo soul: dark Rhodes / lo-fi / jazz
        {6,0,3,3,3,3, .24,.18,.22,.18,1.1,.40,.30, .44,.73,.23,.24}, // chill: warm pad / nylon / lo-fi
        {2,3,0,0,8,4, .80,.18,.72,.72,.20,.03,.65, .56,.90,.12,.18}  // dance stabs: upper triads / 909
}};
inline const GenreSound& sound(int genre) { return sounds[(std::size_t)std::clamp(genre,0,numGenres-1)]; }
inline constexpr std::array<int,numGenres> extensions{2,2,2,2,1,0,1,1,1,0,3,2,0};
inline constexpr std::array<int,numGenres> bassLines{8,23,16,11,22,13,18,15,20,12,10,16,12};
inline constexpr std::array<int,numGenres> tempos{124,122,120,123,126,126,132,122,120,126,98,100,128};
inline constexpr std::array<float,numGenres> swing{.45f,.2f,.5f,.3f,.8f,.6f,.75f,.35f,.3f,.45f,.9f,.3f,.1f};
inline constexpr std::array<double,13> drumLevels{.60,.90,.85,.90,.85,.85,.75,.90,.75,.72,.72,.72,.72};
inline constexpr std::array<int,numGenres> progressions{4,2,0,8,17,28,9,0,33,27,29,6,4};
inline constexpr std::array<const char*,numGenres> progressionNames{"Turnaround  I - vi - ii - V","Soulful Descent  IV - iii - ii - I","Dorian Vamp  i - IV","Gospel Lift  IV - V - vi - I","Jackin' Minor  i - iv","Jackin' Dorian  i - IV - i - v","Mixo Groove  I - VII - IV - I","Dorian Vamp  i - IV","Disco Jack  ii - V - I - vi","Deep House Classic  vi - IV - I - V","Neo Soul  ii - iii - IV - V","Minor Flow  i - VI - III - VII","Turnaround  I - vi - ii - V"};
inline int bassFeel(int genre) { return genre==4?1:genre==2||genre==6||genre==7||genre==12?2:0; }
inline float bassGate(int genre) { return genre==4?.30f:.55f; }
inline float pianoReverb(int genre) { return genre==11?.24f:genre==4?.025f:.10f; }
inline float pianoChorus(int genre) { return genre==1||genre==10?.12f:0.f; }
}
