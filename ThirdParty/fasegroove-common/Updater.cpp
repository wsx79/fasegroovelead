// Fase Groove family - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Updater.h"
#include "Localisation.h"
#include "ProductInfo.h"

#include <juce_cryptography/juce_cryptography.h>

#include <array>
#include <atomic>
#if JUCE_WINDOWS
 #include <process.h>
#else
 #include <unistd.h>
#endif

// This build's version: the plug-ins' and the apps' own macro.
#if defined (JucePlugin_VersionString)
 #define FASEGROOVE_VERSION_STRING JucePlugin_VersionString
#else
 #define FASEGROOVE_VERSION_STRING JUCE_APPLICATION_VERSION_STRING
#endif

namespace Updater
{
namespace
{
    const auto& info = FaseGroove::product();
    const juce::String repo (info.repo);
    const juce::String assetPrefix (info.assetPrefix);
    const juce::String latestReleaseApi ("https://api.github.com/repos/wsx79/" + repo + "/releases/latest");
    const juce::String rawBase ("https://raw.githubusercontent.com/wsx79/" + repo + "/");

    juce::String assetName (InstallKind kind)
    {
       #if JUCE_WINDOWS
        return assetPrefix + (kind == InstallKind::installer ? "-Windows-Setup.exe" : "-Windows.zip");
       #elif JUCE_MAC
        juce::ignoreUnused (kind);
        return assetPrefix + "-macOS.zip";
       #else
        juce::ignoreUnused (kind);
        return assetPrefix + "-Linux.zip";
       #endif
    }

    const juce::String productName (info.productName);

    // Limits: a release page or changelog, a download, and how long each may take.
    constexpr juce::int64 maxTextBytes = 1024 * 1024, maxDownloadBytes = 256 * 1024 * 1024;

    /** The plug-in bundle (or app) this code is running from. */
    juce::File runningBundle()
    {
        auto module = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        juce::File bundle = module;
        for (auto f = module.getParentDirectory(); f != f.getParentDirectory(); f = f.getParentDirectory())
            if (f.hasFileExtension ("vst3;component;app"))
                bundle = f;   // the outermost one
        return bundle;
    }

    /** The same product in the unpacked zip (VST3/, AU/, Standalone/). */
    juce::File stagedCounterpart (const juce::File& staged, const juce::File& target)
    {
        if (! info.updatesAppOnly)
        {
            if (target.hasFileExtension ("vst3"))      return staged.getChildFile ("VST3").getChildFile (target.getFileName());
            if (target.hasFileExtension ("component")) return staged.getChildFile ("AU").getChildFile (target.getFileName());
        }
        return staged.getChildFile ("Standalone").getChildFile (target.getFileName());
    }

   #if JUCE_MAC
    bool runTool (const juce::StringArray& args)
    {
        juce::ChildProcess p;
        return p.start (args, 0) && p.waitForProcessToFinish (120000) && p.getExitCode() == 0;
    }
   #endif

    bool unpack (const juce::File& zip, const juce::File& folder)
    {
        // Every entry stays inside the folder: no absolute path, drive or "..".
        {
            juce::ZipFile checked (zip);
            if (checked.getNumEntries() == 0)
                return false;
            for (int i = 0; i < checked.getNumEntries(); ++i)
            {
                const auto name = checked.getEntry (i)->filename.replace ("\\", "/");
                juce::StringArray parts;
                parts.addTokens (name, "/", {});
                if (name.startsWithChar ('/') || name.containsChar (':') || parts.contains (".."))
                    return false;
            }
        }
        folder.deleteRecursively();
        if (! folder.createDirectory())
            return false;
       #if JUCE_MAC
        return runTool ({ "/usr/bin/ditto", "-x", "-k", zip.getFullPathName(), folder.getFullPathName() });   // keeps bundles intact
       #else
        juce::ZipFile archive (zip);
        if (archive.getNumEntries() == 0 || archive.uncompressTo (folder).failed())
            return false;
        #if JUCE_LINUX || JUCE_BSD
        for (const auto& f : folder.findChildFiles (juce::File::findFiles, true))   // zip entries lose the execute bit
            if (f.getFileExtension().isEmpty() || f.hasFileExtension ("so"))
                f.setExecutePermission (true);
        #endif
        return true;
       #endif
    }

    /** Every copy of the plug-in we can update: the running one, its neighbours from the same zip,
        and the usual plug-in folders (an app: only itself). */
    juce::Array<juce::File> updateTargets (const juce::File& staged)
    {
        juce::Array<juce::File> candidates;
        const auto running = runningBundle();
        candidates.add (running);

        if (! info.updatesAppOnly)
        {
            const auto zipRoot = running.getParentDirectory().getParentDirectory();
            candidates.add (zipRoot.getChildFile ("VST3").getChildFile (productName + ".vst3"));
            candidates.add (zipRoot.getChildFile ("AU").getChildFile (productName + ".component"));
           #if JUCE_WINDOWS
            candidates.add (zipRoot.getChildFile ("Standalone").getChildFile (productName + ".exe"));
            candidates.add (running.getSiblingFile (productName + ".exe"));
            candidates.add (running.getSiblingFile (productName + ".vst3"));
           #elif JUCE_MAC
            candidates.add (zipRoot.getChildFile ("Standalone").getChildFile (productName + ".app"));
            const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
            for (const auto& base : { home, juce::File ("/") })
            {
                candidates.add (base.getChildFile ("Library/Audio/Plug-Ins/VST3").getChildFile (productName + ".vst3"));
                candidates.add (base.getChildFile ("Library/Audio/Plug-Ins/Components").getChildFile (productName + ".component"));
            }
            candidates.add (juce::File ("/Applications").getChildFile (productName + ".app"));
            candidates.add (home.getChildFile ("Applications").getChildFile (productName + ".app"));
           #else
            candidates.add (zipRoot.getChildFile ("Standalone").getChildFile (productName));
            candidates.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".vst3").getChildFile (productName + ".vst3"));
           #endif
        }

        juce::Array<juce::File> targets;
        for (const auto& f : candidates)
            if (f.exists() && f.getParentDirectory().hasWriteAccess() && stagedCounterpart (staged, f).exists()
                && ! targets.contains (f))
                targets.add (f);
        return targets;
    }

    /** Replaces copies of the plug-in as one transaction: everything is checked first, every
        change is recorded, and if any step fails all of them are undone (including the copies
        already replaced), so a failed update never leaves a half-new plug-in. */
    class Transaction
    {
    public:
        explicit Transaction (int failAfterSteps = -1) : failAfter (failAfterSteps) {}
        ~Transaction() { if (! committed) rollback(); }

        /** Checks that every file can be replaced before anything is touched. */
        static bool preflight (const juce::File& staged, const juce::Array<juce::File>& targets)
        {
            for (const auto& target : targets)
            {
                const auto fresh = stagedCounterpart (staged, target);
                if (! fresh.exists() || ! target.getParentDirectory().hasWriteAccess())
                    return false;
               #if JUCE_WINDOWS
                // Leftovers of an earlier update that are still loaded would block the renames.
                juce::Array<juce::File> files;
                if (fresh.isDirectory()) files = fresh.findChildFiles (juce::File::findFiles, true);
                else                     files.add (fresh);
                for (const auto& f : files)
                {
                    const auto dest = fresh.isDirectory() ? target.getChildFile (f.getRelativePathFrom (fresh)) : target;
                    const auto old = dest.getSiblingFile (dest.getFileName() + ".old");
                    if (old.exists() && ! old.deleteFile())
                        return false;
                }
               #endif
            }
            return true;
        }

        bool replace (const juce::File& fresh, const juce::File& target)
        {
           #if JUCE_WINDOWS
            // A loaded DLL (or the running .exe) can't be overwritten and its folder can't be
            // renamed, but the file itself can: move each old file aside, copy the new one in.
            juce::Array<juce::File> files;
            if (fresh.isDirectory()) files = fresh.findChildFiles (juce::File::findFiles, true);
            else                     files.add (fresh);

            for (const auto& f : files)
            {
                const auto dest = fresh.isDirectory() ? target.getChildFile (f.getRelativePathFrom (fresh)) : target;
                if (! step() || ! dest.getParentDirectory().createDirectory())
                    return false;

                juce::File backup;
                if (dest.exists())
                {
                    backup = dest.getSiblingFile (dest.getFileName() + ".old");
                    if (backup.exists() || ! dest.moveFileTo (backup))
                        return false;
                }
                changes.push_back ({ dest, backup });
                if (! f.copyFileTo (dest))
                    return false;
            }
            return true;
           #else
            // macOS / Linux: build the new copy next to the old one, then swap them by renaming
            // (the running code keeps working from the old files until the DAW restarts).
            const auto incoming = target.getSiblingFile (target.getFileName() + ".new");
            const auto backup = target.getSiblingFile (target.getFileName() + ".old");
            incoming.deleteRecursively();
            backup.deleteRecursively();
            if (! step())
                return false;
            #if JUCE_MAC
            if (! runTool ({ "/usr/bin/ditto", fresh.getFullPathName(), incoming.getFullPathName() }))
            {
                incoming.deleteRecursively();
                return false;
            }
            runTool ({ "/usr/bin/xattr", "-dr", "com.apple.quarantine", incoming.getFullPathName() });
            #else
            if (! (fresh.isDirectory() ? fresh.copyDirectoryTo (incoming) : fresh.copyFileTo (incoming)))
            {
                incoming.deleteRecursively();
                return false;
            }
            for (const auto& f : incoming.isDirectory() ? incoming.findChildFiles (juce::File::findFiles, true) : juce::Array<juce::File> { incoming })
                if (f.getFileExtension().isEmpty() || f.hasFileExtension ("so") || f == incoming)
                    f.setExecutePermission (true);
            #endif
            if (! step() || ! target.moveFileTo (backup))
            {
                incoming.deleteRecursively();
                return false;
            }
            changes.push_back ({ target, backup });
            if (! step() || ! incoming.moveFileTo (target))
            {
                incoming.deleteRecursively();
                return false;
            }
            return true;
           #endif
        }

        /** Keeps the new files; the old ones are deleted (or, on Windows, at the next start if loaded). */
        void commit()
        {
            committed = true;
            for (const auto& c : changes)
                if (c.backup != juce::File())
                    c.backup.deleteRecursively();
        }

        void rollback()
        {
            for (auto it = changes.rbegin(); it != changes.rend(); ++it)
            {
                it->dest.deleteRecursively();
                if (it->backup != juce::File())
                    it->backup.moveFileTo (it->dest);
            }
            changes.clear();
            committed = true;
        }

    private:
        struct Change { juce::File dest, backup; };   // backup empty: dest is a new file

        bool step() { return failAfter < 0 || steps++ < failAfter; }   // failure injection for the tests

        std::vector<Change> changes;
        int failAfter = -1, steps = 0;
        bool committed = false;
    };

    bool replaceAll (const juce::File& staged, const juce::Array<juce::File>& targets, int failAfterSteps)
    {
        if (targets.isEmpty() || ! Transaction::preflight (staged, targets))
            return false;

        Transaction transaction (failAfterSteps);
        for (const auto& target : targets)
            if (! transaction.replace (stagedCounterpart (staged, target), target))
            {
                transaction.rollback();
                return false;
            }
        transaction.commit();
        return true;
    }

   #if JUCE_LINUX || JUCE_BSD
    // The plug-in is built without libcurl (no extra dependency), so on Linux the system's
    // curl command does the HTTPS work.
    bool curl (juce::Thread& thread, juce::StringArray args, juce::String* output)
    {
        args.insert (0, "curl");
        args.insert (1, "-sSfL");
        args.insert (2, "--max-time");
        args.insert (3, output != nullptr ? "15" : "600");   // a page, or a download
        args.insert (4, "-A");
        args.insert (5, assetPrefix + "-Updater");
        // Gives up on a dead or crawling connection, and on a file bigger than any release.
        args.addArray ({ "--connect-timeout", "10", "--speed-time", "15", "--speed-limit", "1024",
                         "--max-filesize", juce::String (maxDownloadBytes) });

        juce::ChildProcess process;
        if (! process.start (args, output != nullptr ? juce::ChildProcess::wantStdOut : 0))
            return false;

        juce::MemoryOutputStream out;
        while (process.isRunning())
        {
            if (thread.threadShouldExit())
            {
                process.kill();
                return false;
            }
            if (output != nullptr)
            {
                char buffer[4096];
                const int n = process.readProcessOutput (buffer, (int) sizeof (buffer));
                if (n > 0) out.write (buffer, (size_t) n);
                else juce::Thread::sleep (20);
                if ((juce::int64) out.getDataSize() > maxTextBytes)
                {
                    process.kill();
                    return false;
                }
            }
            else
            {
                juce::Thread::sleep (50);
            }
        }

        if (output != nullptr)
        {
            char buffer[4096];
            for (int n; (n = process.readProcessOutput (buffer, (int) sizeof (buffer))) > 0;)
                out.write (buffer, (size_t) n);
            *output = out.toUTF8();
        }
        return process.getExitCode() == 0;
    }
   #endif

   #if ! (JUCE_LINUX || JUCE_BSD)
    std::unique_ptr<juce::InputStream> open (const juce::String& url)
    {
        return juce::URL (url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                      .withConnectionTimeoutMs (15000)
                                                      .withExtraHeaders ("User-Agent: " + assetPrefix + "-Updater\r\nAccept: application/vnd.github+json"));
    }
   #endif

    bool fetchText (juce::Thread& thread, const juce::String& url, juce::String& text)
    {
       #if JUCE_LINUX || JUCE_BSD
        return curl (thread, { url }, &text) && text.isNotEmpty();
       #else
        auto stream = open (url);
        if (stream == nullptr)
            return false;
        juce::MemoryOutputStream data;
        while (! thread.threadShouldExit() && ! stream->isExhausted())
        {
            char buffer[4096];
            const int n = stream->read (buffer, (int) sizeof (buffer));
            if (n <= 0)
                break;
            data.write (buffer, (size_t) n);
            if ((juce::int64) data.getDataSize() > maxTextBytes)
                return false;
        }
        text = data.toUTF8();
        return text.isNotEmpty() && ! thread.threadShouldExit();
       #endif
    }

    bool fetchFile (juce::Thread& thread, const juce::String& url, const juce::File& target)
    {
        auto partial = target.getSiblingFile (target.getFileName() + ".part");
        partial.deleteFile();

       #if JUCE_LINUX || JUCE_BSD
        if (! curl (thread, { "-o", partial.getFullPathName(), url }, nullptr))
        {
            partial.deleteFile();
            return false;
        }
       #else
        {
            auto stream = open (url);
            if (stream == nullptr)
                return false;

            juce::FileOutputStream out (partial);
            if (! out.openedOk())
                return false;

            juce::HeapBlock<char> buffer (65536);
            for (;;)
            {
                if (thread.threadShouldExit() || out.getPosition() > maxDownloadBytes)
                {
                    out.flush();
                    partial.deleteFile();
                    return false;
                }
                const int n = stream->read (buffer, 65536);
                if (n <= 0)
                    break;
                out.write (buffer, (size_t) n);
            }
            out.flush();
            if (out.getStatus().failed() || ! stream->isExhausted())
            {
                partial.deleteFile();
                return false;
            }
        }
       #endif

        target.deleteFile();
        return partial.getSize() > 0 && partial.moveFileTo (target);
    }

    juce::File downloadsFolder()
    {
        auto downloads = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads");
        if (! downloads.isDirectory())
            downloads = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        return downloads.getChildFile (productName + " updates");
    }

    /** Markdown -> plain text that reads well in a text box. */
    juce::String plain (juce::String text)
    {
        text = text.replace ("\r\n", "\n").replace ("**", "").replace ("*", "").replace ("`", "");
        juce::StringArray raw, lines;
        raw.addLines (text);
        for (auto& line : raw)
        {
            // Changelog lines are wrapped at ~100 columns: join them back (the text box wraps).
            if (line.startsWith ("  ") && line.trim().isNotEmpty() && ! lines.isEmpty() && lines[lines.size() - 1].isNotEmpty())
                lines.getReference (lines.size() - 1) << " " << line.trim();
            else
                lines.add (line);
        }
        for (auto& line : lines)
        {
            if (line.startsWith ("### ")) line = Localisation::upper (line.substring (4));
            else if (line.startsWith ("## ")) line = line.substring (3);
            else if (line.startsWith ("- ")) line = juce::String::fromUTF8 ("\xe2\x80\xa2 ") + line.substring (2);
        }
        return lines.joinIntoString ("\n").trim();
    }
}

bool autoCheckEnabled()
{
    return Localisation::setting ("autoUpdate", "1") != "0";
}

void setAutoCheckEnabled (bool enabled)
{
    Localisation::setSetting ("autoUpdate", enabled ? "1" : "0");
}

void skipVersion (const juce::String& version)
{
    Localisation::setSetting ("skippedVersion", version);
}

bool isNewer (const juce::String& a, const juce::String& b)
{
    auto parts = [] (juce::String v)
    {
        v = v.trim().trimCharactersAtStart ("vV");
        juce::StringArray tokens;
        tokens.addTokens (v.upToFirstOccurrenceOf ("-", false, false), ".", {});
        std::array<int, 3> n {};
        for (int i = 0; i < 3 && i < tokens.size(); ++i)
            n[(size_t) i] = tokens[i].getIntValue();
        return n;
    };
    return parts (a) > parts (b);
}

juce::String changelogSection (const juce::String& changelog, const juce::String& version)
{
    const auto v = version.trimCharactersAtStart ("vV");
    juce::StringArray lines;
    lines.addLines (changelog);

    juce::StringArray section;
    bool inside = false;
    for (auto& line : lines)
    {
        if (line.startsWith ("## "))
        {
            if (inside)
                break;
            inside = line.startsWith ("## [" + v + "]") || line.startsWith ("## " + v + " ") || line == "## " + v;
            continue;
        }
        if (inside)
            section.add (line);
    }
    return plain (section.joinIntoString ("\n"));
}

InstallKind installKind()
{
    return testing::installKindFor (runningBundle());
}

InstallKind testing::installKindFor (const juce::File& bundle)
{
   #if JUCE_WINDOWS
    if (info.setupMarker != nullptr)
    {
        // The setup leaves its marker and its uninstaller next to the app, in any folder.
        if (bundle.getSiblingFile (info.setupMarker).existsAsFile() && bundle.getSiblingFile ("unins000.exe").existsAsFile())
            return InstallKind::installer;
    }
    else
    {
        // The setup installs into Program Files (VST3 in Common Files); anything else was copied by hand.
        for (auto location : { juce::File::globalApplicationsDirectory, juce::File::globalApplicationsDirectoryX86 })
            if (bundle.isAChildOf (juce::File::getSpecialLocation (location)))
                return InstallKind::installer;
    }
   #endif
    return bundle.exists() && bundle.getParentDirectory().hasWriteAccess() ? InstallKind::portable : InstallKind::manual;
}

bool isStandaloneApp()
{
    return juce::JUCEApplicationBase::isStandaloneApp();
}

namespace
{
    /** Opens the (just updated) standalone app again: once this process has ended
        (relaunchWhenClosed: the path goes as an argument, never into shell code), else a moment
        after this one has quit. */
    bool relaunchLater (const juce::File& app)
    {
        juce::ChildProcess p;
        if (info.relaunchWhenClosed)
        {
           #if JUCE_WINDOWS
            const auto helper = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getNonexistentChildFile (assetPrefix + "-relaunch", ".ps1", false);
            if (! helper.replaceWithText ("param([int]$OwnerPid,[string]$App)\nWait-Process -Id $OwnerPid -ErrorAction SilentlyContinue\n"
                                          "Start-Process -FilePath $App\nRemove-Item -LiteralPath $PSCommandPath\n"))
                return false;
            return p.start (juce::StringArray { "powershell.exe", "-NoProfile", "-NonInteractive", "-WindowStyle", "Hidden",
                                                "-ExecutionPolicy", "Bypass", "-File", helper.getFullPathName(),
                                                "-OwnerPid", juce::String (_getpid()), "-App", app.getFullPathName() }, 0);
           #else
            #if JUCE_MAC
            const juce::String launch ("exec /usr/bin/open -n \"$1\"");
            #else
            const juce::String launch ("exec \"$1\"");
            #endif
            return p.start (juce::StringArray { "/bin/sh", "-c", "while kill -0 \"$2\" 2>/dev/null; do sleep 0.2; done; " + launch,
                                                "fg-update", app.getFullPathName(), juce::String ((juce::int64) getpid()) }, 0);
           #endif
        }
       #if JUCE_MAC
        return p.start (juce::StringArray { "/bin/sh", "-c", "sleep 1; open -n \"" + app.getFullPathName() + "\"" }, 0);
       #elif JUCE_WINDOWS
        // The old .exe was renamed by the swap: the new one is at the same path.
        return app.startAsProcess();
       #else
        return p.start (juce::StringArray { "/bin/sh", "-c", "sleep 1; \"" + app.getFullPathName() + "\" &" }, 0);
       #endif
    }
}

InstallReport install (const Result& result)
{
    InstallReport report;

    // Only the file checked against the release: anything else is neither run nor unpacked.
    if (! testing::verifyDownload (result.file, result.digest))
        return report;
    // Linux resolves /proc/self/exe to the renamed old image after a swap: the path now.
    const auto app = runningBundle();

    if (result.kind == InstallKind::installer && result.file.existsAsFile())
    {
        // Same setup, no wizard: it keeps the folders and parts you chose last time and asks to
        // close the DAW only if the plug-in is in use. In the standalone app the setup reopens it
        // when it is done (RELAUNCH), and the app quits so its files can be replaced.
        const bool standalone = isStandaloneApp();
        juce::String args (standalone ? "/SILENT /NORESTART /CLOSEAPPLICATIONS /RELAUNCH=1" : "/SILENT /NORESTART");
        if (info.setupMarker != nullptr)
            args << " /DIR=\"" << app.getParentDirectory().getFullPathName() << "\"";   // the folder it was installed in
        report.installerStarted = report.done = result.file.startAsProcess (args);
        report.relaunching = report.done && standalone;
        if (report.done)
            return report;
        if (info.setupMarker != nullptr)
        {
            result.file.revealToUser();
            return report;
        }
    }

    if (result.kind == InstallKind::portable && result.staged.isDirectory())
    {
        // All copies or none: on any error everything already replaced is put back.
        const auto targets = updateTargets (result.staged);
        if (replaceAll (result.staged, targets, -1))
        {
            for (const auto& t : targets)
                report.updated.add (t.getFullPathName());
            report.done = true;
            report.relaunching = isStandaloneApp() && relaunchLater (app);
            return report;
        }
    }

    // Fallback: show the download so it can be installed by hand.
   #if JUCE_MAC
    result.file.startAsProcess();   // Archive Utility unzips it next to the zip
    result.file.getParentDirectory().revealToUser();
   #elif JUCE_WINDOWS
    if (result.file.hasFileExtension ("exe"))
        report.installerStarted = result.file.startAsProcess();
    else
        result.file.revealToUser();
   #else
    result.file.revealToUser();
   #endif
    return report;
}

namespace testing
{
    bool replaceAll (const juce::File& staged, const juce::Array<juce::File>& targets, int failAfterSteps)
    {
        return Updater::replaceAll (staged, targets, failAfterSteps);
    }

    bool unpack (const juce::File& zip, const juce::File& folder)
    {
        return Updater::unpack (zip, folder);
    }

    bool verifyDownload (const juce::File& file, const juce::String& digest)
    {
        const auto hash = digest.fromFirstOccurrenceOf ("sha256:", false, false).toLowerCase();
        if (! digest.startsWith ("sha256:") || hash.length() != 64 || ! hash.containsOnly ("0123456789abcdef")
            || ! file.existsAsFile() || file.getSize() <= 0 || file.getSize() > maxDownloadBytes)
            return false;
        auto input = file.createInputStream();
        return input != nullptr && juce::SHA256 (*input).toHexString() == hash;
    }

    juce::String releaseAsset (const juce::var& release, InstallKind kind, juce::String& digest)
    {
        // A published release (no draft, no pre-release), its own asset for this platform, at the
        // address GitHub gives it, with its checksum.
        digest.clear();
        const auto tag = release.getProperty ("tag_name", {}).toString();
        if (tag.isEmpty() || ! tag.containsOnly ("vV0123456789.") || (bool) release.getProperty ("draft", false)
            || (bool) release.getProperty ("prerelease", false))
            return {};
        const auto name = assetName (kind);
        const auto expected = "https://github.com/wsx79/" + repo + "/releases/download/" + tag + "/" + name;
        if (auto* assets = release.getProperty ("assets", {}).getArray())
            for (const auto& a : *assets)
                if (a.getProperty ("name", {}).toString() == name && a.getProperty ("browser_download_url", {}).toString() == expected)
                {
                    digest = a.getProperty ("digest", {}).toString();
                    if (digest.startsWith ("sha256:") && digest.length() == 71)
                        return expected;
                }
        digest.clear();
        return {};
    }
}

void cleanupAfterUpdate()
{
    const auto bundle = runningBundle();
    juce::Array<juce::File> leftovers;
    if (bundle.isDirectory())
        leftovers = bundle.findChildFiles (juce::File::findFiles, true, "*.old");
    leftovers.add (bundle.getSiblingFile (bundle.getFileName() + ".old"));
    for (const auto& f : leftovers)
        if (f.exists())
            f.deleteRecursively();   // fails quietly while something still has it loaded
}

bool firstAutoCheckThisSession()
{
    static std::atomic<bool> checked { false };
    return ! checked.exchange (true);
}

//==============================================================================
Checker::Checker() : juce::Thread (productName + " update check") {}

Checker::~Checker()
{
    stopThread (info.updatesAppOnly ? 18000 : 4000);   // (a plug-in must not hold up the DAW)
}

void Checker::start (bool manual)
{
    if (isThreadRunning())
        return;
    manualCheck = manual;
    startThread (juce::Thread::Priority::low);
}

void Checker::run()
{
    Result result;
    const bool manual = manualCheck;

    [&]
    {
        juce::String json;
        if (! fetchText (*this, latestReleaseApi, json))
            return;

        const auto release = juce::JSON::parse (json);
        const auto tag = release.getProperty ("tag_name", {}).toString();
        if (release.getProperty ("message", {}).toString() == "Not Found")
        {
            result.status = Result::Status::noRelease;
            return;
        }
        if (tag.isEmpty() || (bool) release.getProperty ("draft", false) || (bool) release.getProperty ("prerelease", false))
            return;

        result.version = tag.trimCharactersAtStart ("vV");
        result.kind = installKind();
        if (! isNewer (tag, FASEGROOVE_VERSION_STRING))
        {
            result.status = Result::Status::upToDate;
            return;
        }
        if (! manual && Localisation::setting ("skippedVersion") == result.version)
        {
            result.status = Result::Status::upToDate;
            return;
        }

        const auto assetUrl = testing::releaseAsset (release, result.kind, result.digest);
        if (assetUrl.isEmpty())
            return;

        // What's new: the changelog in the UI language when there is one, else the release notes.
        if (Localisation::current() == Localisation::Language::italian)
        {
            juce::String changelog;
            if (fetchText (*this, rawBase + tag + "/CHANGELOG.it.md", changelog))
                result.notes = changelogSection (changelog, tag);
        }
        if (result.notes.isEmpty())
            result.notes = plain (release.getProperty ("body", {}).toString());

        const auto folder = downloadsFolder();
        folder.createDirectory();
        auto file = folder.getChildFile (juce::File::createLegalFileName (juce::String (assetName (result.kind))
                                             .replace (assetPrefix + "-", assetPrefix + "-" + result.version + "-")));
        if (! testing::verifyDownload (file, result.digest))   // (one already downloaded and intact is kept)
            if (! fetchFile (*this, assetUrl, file) || ! testing::verifyDownload (file, result.digest))
            {
                file.deleteFile();
                return;
            }

        result.file = file;

        // A portable copy: unpack now, so INSTALL NOW only has to swap the files.
        if (result.kind == InstallKind::portable)
        {
            const auto staged = folder.getChildFile (result.version);
            if (unpack (file, staged))
                result.staged = staged;
            else
                result.kind = InstallKind::manual;
        }
        result.status = Result::Status::ready;
    }();

    if (threadShouldExit())
        return;

    juce::WeakReference<Checker> self (this);
    juce::MessageManager::callAsync ([self, result, manual]
    {
        if (self != nullptr && self->onResult != nullptr)
            self->onResult (result, manual);
    });
}
}
