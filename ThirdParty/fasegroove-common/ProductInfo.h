// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// What tells one Fase Groove product from another inside the shared code (fasegroove-common).
// Each product defines FaseGroove::product() once (Source/Product.cpp); the shared modules
// (Updater, Localisation, Branding) read their names, URLs and resources from it, so the
// shared files stay byte-identical in every product.

#include <cstddef>

namespace FaseGroove
{
    struct ProductInfo
    {
        const char* productName;   // "Fase Groove Drums": settings folder, bundle names, update folder
        const char* repo;          // "fasegroovedrums": github.com/wsx79/<repo>
        const char* assetPrefix;   // "FaseGrooveDrums": release assets and the updater's User-Agent
        const char* wordmark;      // "GROOVE DRUMS": second line of the logo

        const void* iconSvg = nullptr;          // the square app icon (BinaryData)
        int iconSvgSize = 0;
        const void* artistLogoPng = nullptr;    // the FS PITCH logo (BinaryData)
        int artistLogoPngSize = 0;

        // The updater (Updater.cpp). A plug-in updates every copy of itself it finds (VST3, AU,
        // standalone app); an app only itself.
        bool updatesAppOnly = false;
        /** Windows: a file the setup writes next to the app ("FaseGrooveArranger.install.ini"). With
            it (and the uninstaller) the copy counts as installed by the setup wherever its folder
            is, and the setup runs again into that folder. Null: a copy in Program Files counts. */
        const char* setupMarker = nullptr;
        /** The app reopens itself after a portable update only once this process has ended (the
            new executable at the same path); else it is started again at once. */
        bool relaunchWhenClosed = false;

        /** Localisation::tr(): false (the plug-ins) finds the English text exactly. True (the
            arranger) finds it in any case, or by its Italian text, keeps a text written in
            capitals in capitals, and translates "Name 2" / "Name B" as "Name" plus the number or
            letter; a row whose English text matches exactly still comes first. */
        bool looseTranslationKeys = false;
    };

    /** Defined by each product. */
    const ProductInfo& product();
}
