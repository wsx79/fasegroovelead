// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// Automatic updates from the GitHub releases page.
// A background thread asks GitHub for the latest release; if it is newer than this build it
// downloads the right package into the Downloads folder (the Windows setup when the product was
// installed with it, otherwise the zip, already unpacked) and reports back on the message thread
// with the release notes. Nothing is installed until the user confirms.
// Safeguards: only a published release's own asset, checked against its SHA-256; size and time
// limits on every download; no zip entry outside its folder; a failed swap puts every old file
// back. What differs per product (names, plug-in copies, setup, relaunch): ProductInfo.h.
// It can be turned off in OPTIONS -> SETTINGS; nothing else is ever sent or received.

#include <juce_events/juce_events.h>

namespace Updater
{
    /** How this copy was installed, which decides how it is updated:
        installer = Windows setup (runs again silently, keeping your choices),
        portable  = files copied by hand (they are replaced in place),
        manual    = no write access (the download is shown, you copy it). */
    enum class InstallKind { installer, portable, manual };
    InstallKind installKind();

    struct Result
    {
        enum class Status { upToDate, noRelease, failed, ready };
        Status status = Status::failed;
        juce::String version;   // e.g. "1.15.0"
        juce::String notes;     // what's new, in the UI language when available
        juce::File file;        // the downloaded installer or zip
        juce::File staged;      // portable: the zip unpacked, ready to swap in
        InstallKind kind = InstallKind::manual;
        juce::String digest;    // "sha256:..." of the release asset
    };

    struct InstallReport
    {
        bool done = false;           // false: nothing replaced, the download was shown instead
        bool installerStarted = false;
        bool relaunching = false;    // standalone app: it must quit now, the new version opens by itself
        juce::StringArray updated;   // portable: what was replaced
    };

    /** Automatic check when the product opens (on by default). */
    bool autoCheckEnabled();
    void setAutoCheckEnabled (bool);

    /** "Skip this version": that version is no longer offered automatically. */
    void skipVersion (const juce::String& version);

    /** True when version a (e.g. "v1.15.0") is newer than b (e.g. "1.14.0"). */
    bool isNewer (const juce::String& a, const juce::String& b);

    /** The "## [x.y.z]" section of a changelog, as plain text. */
    juce::String changelogSection (const juce::String& changelog, const juce::String& version);

    /** Installs a downloaded update: the silent installer, the in-place swap of a portable copy,
        or (when neither is possible) shows the download. */
    InstallReport install (const Result&);

    namespace testing
    {
        /** The portable swap, with a failure injected after N steps (-1: none). For the tests. */
        bool replaceAll (const juce::File& staged, const juce::Array<juce::File>& targets, int failAfterSteps);
        /** Unpacks a zip; false (nothing written) when an entry would land outside the folder. */
        bool unpack (const juce::File& zip, const juce::File& folder);
        /** The file is there, not too big, and its SHA-256 is the release's ("sha256:..."). */
        bool verifyDownload (const juce::File&, const juce::String& digest);
        /** The download URL of this platform's asset of a published release (and its digest), or "". */
        juce::String releaseAsset (const juce::var& release, InstallKind, juce::String& digest);
        InstallKind installKindFor (const juce::File& bundle);
    }

    /** True in the standalone app (false inside a DAW). */
    bool isStandaloneApp();

    /** Removes the files a portable update left behind (Windows can't delete a loaded file). */
    void cleanupAfterUpdate();

    class Checker : private juce::Thread
    {
    public:
        Checker();
        ~Checker() override;

        /** Starts a check. manual = true also reports "up to date" / "failed" and ignores skipped versions. */
        void start (bool manual);

        bool isBusy() const { return isThreadRunning(); }

        /** Called on the message thread when a check ends. */
        std::function<void (const Result&, bool manual)> onResult;

    private:
        void run() override;
        bool manualCheck = false;

        JUCE_DECLARE_WEAK_REFERENCEABLE (Checker)
    };

    /** True only the first time it is called in this process: one automatic check per session. */
    bool firstAutoCheckThisSession();
}
