// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// The updater's safeguards, as every plug-in has them (the arranger: tests/UpdaterTests.cpp).
// Include after TestSupport.h (CHECK).

#include "ProductInfo.h"
#include "Updater.h"

#include <juce_cryptography/juce_cryptography.h>

namespace FaseGrooveTests
{
    inline void checkUpdaterSafeguards()
    {
        namespace u = Updater;
        const auto& info = FaseGroove::product();
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("FaseGroove-updater-test", {}, false);
        CHECK (root.createDirectory().wasOk());

        // Only a download whose SHA-256 is the release's.
        const auto file = root.getChildFile ("download.zip");
        CHECK (file.replaceWithText ("release payload"));
        juce::String digest;
        if (auto in = file.createInputStream())
            digest = "sha256:" + juce::SHA256 (*in).toHexString();
        CHECK (u::testing::verifyDownload (file, digest));
        file.appendText ("changed");
        CHECK (! u::testing::verifyDownload (file, digest));
        CHECK (! u::testing::verifyDownload (file, "sha256:bad"));

        // This platform's asset of a published release, at its own GitHub address, with a checksum.
       #if JUCE_WINDOWS
        const auto name = juce::String (info.assetPrefix) + "-Windows.zip";
       #elif JUCE_MAC
        const auto name = juce::String (info.assetPrefix) + "-macOS.zip";
       #else
        const auto name = juce::String (info.assetPrefix) + "-Linux.zip";
       #endif
        const auto url = "https://github.com/wsx79/" + juce::String (info.repo) + "/releases/download/v9.9.9/" + name;
        juce::var release (new juce::DynamicObject), asset (new juce::DynamicObject);
        release.getDynamicObject()->setProperty ("tag_name", "v9.9.9");
        asset.getDynamicObject()->setProperty ("name", name);
        asset.getDynamicObject()->setProperty ("digest", "sha256:" + juce::String::repeatedString ("a", 64));
        auto setUrl = [&] (const juce::String& u)
        {
            asset.getDynamicObject()->setProperty ("browser_download_url", u);
            release.getDynamicObject()->setProperty ("assets", juce::var (juce::Array<juce::var> { asset }));
        };
        setUrl (url);
        juce::String picked;
        CHECK (u::testing::releaseAsset (release, u::InstallKind::portable, picked) == url && picked.startsWith ("sha256:"));
        release.getDynamicObject()->setProperty ("prerelease", true);
        CHECK (u::testing::releaseAsset (release, u::InstallKind::portable, picked).isEmpty());
        release.getDynamicObject()->setProperty ("prerelease", false);
        setUrl ("https://example.invalid/" + name);
        CHECK (u::testing::releaseAsset (release, u::InstallKind::portable, picked).isEmpty());
        asset.getDynamicObject()->removeProperty ("digest");
        setUrl (url);
        CHECK (u::testing::releaseAsset (release, u::InstallKind::portable, picked).isEmpty());

        // No zip entry outside the folder; a good one unpacks.
        const auto zip = root.getChildFile ("archive.zip"), folder = root.getChildFile ("unpacked");
        auto archive = [&] (const juce::String& path)
        {
            zip.deleteFile();
            juce::ZipFile::Builder builder;
            builder.addEntry (new juce::MemoryInputStream ("new", 3, false), 6, path, juce::Time::getCurrentTime());
            if (auto out = zip.createOutputStream())
                builder.writeToStream (*out, nullptr);
        };
        archive ("../outside.txt");
        CHECK (! u::testing::unpack (zip, folder) && ! root.getChildFile ("outside.txt").exists());
        archive ("VST3/part.txt");
        CHECK (u::testing::unpack (zip, folder) && folder.getChildFile ("VST3/part.txt").loadFileAsString() == "new");

        // A plug-in: a copy outside the setup's folders is a portable one.
        const auto copy = root.getChildFile ("copy").getChildFile (juce::String (info.productName) + ".vst3");
        CHECK (copy.createDirectory().wasOk());
        CHECK (u::testing::installKindFor (copy) == u::InstallKind::portable);
        CHECK (! info.updatesAppOnly);

        root.deleteRecursively();
    }
}
