// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// The tests' tiny harness, shared by every product's test programs (no JUCE needed):
// CHECK counts and prints a failure, main() returns failures == 0 ? 0 : 1.

#include <cstdio>

static int failures = 0;

#define CHECK(cond) \
    do { if (! (cond)) { ++failures; std::printf ("FAILED %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (false)
