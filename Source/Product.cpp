// Fase Groove Lead - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

// What the shared module (ThirdParty/fasegroove-common) needs to know about this product.

#include "ProductInfo.h"
#include "Branding.h"
#include "Localisation.h"

#include <BinaryData.h>

namespace
{
    constexpr const char* name = "Fase Groove Lead";

    // clang-format off
    const Localisation::Entry ownTexts[] {
#include "Translations.inc"
    };
    // clang-format on
}

const FaseGroove::ProductInfo& FaseGroove::product()
{
    static const ProductInfo info { name, "fasegroovelead", "FaseGrooveLead", "GROOVE LEAD",
                                    BinaryData::fgl_icon_svg, BinaryData::fgl_icon_svgSize,
                                    BinaryData::fs_pitch_logo_png, BinaryData::fs_pitch_logo_pngSize };
    return info;
}

namespace Branding
{
    const char* const appName   = name;
    const char* const tagline   = "house lead grooves for soulful, deep & funky house";
    const char* const sourceUrl = "https://github.com/wsx79/fasegroovelead";
}

Localisation::Table Localisation::productTranslations()
{
    return { ownTexts, sizeof (ownTexts) / sizeof (ownTexts[0]) };
}
