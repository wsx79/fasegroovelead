; Fase Groove Lead - Windows installer (Inno Setup 6)
; Copyright (C) 2026 Fabio 'Pitch' Serra - SPDX-License-Identifier: AGPL-3.0-or-later
;
; Built by GitHub Actions:
;   iscc /DAppVersion=1.3.0 /DBuildDir=<...>\FaseGrooveLead_artefacts\Release /DRepoDir=<repo> FaseGrooveLead.iss
; The user chooses VST3 + Standalone, VST3 only or Standalone only.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\FaseGrooveLead_artefacts\Release"
#endif
#ifndef RepoDir
  #define RepoDir "..\.."
#endif

#define AppName "Fase Groove Lead"
#define Publisher "FS Pitch - Fabio 'Pitch' Serra"
#define AppUrl "https://github.com/wsx79/fasegroovelead"

[Setup]
AppId={{A402678C-AE49-4DB9-8347-C3D215A3FB42}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}/releases/latest
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile={#RepoDir}\LICENSE
OutputDir={#RepoDir}\installer-output
OutputBaseFilename=FaseGrooveLead-Windows-Setup
SetupIconFile=FaseGrooveLead.ico
UninstallDisplayIcon={app}\FaseGrooveLead.ico
WizardStyle=modern
WizardImageFile=wizard-large.bmp,wizard-large-2x.bmp
WizardSmallImageFile=wizard-small.bmp,wizard-small-2x.bmp
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
ShowLanguageDialog=auto
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#Publisher}
VersionInfoDescription={#AppName} installer

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"; InfoAfterFile: "after-install-en.txt"
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"; InfoAfterFile: "after-install-it.txt"

[CustomMessages]
en.TypeFull=VST3 plug-in + Standalone app (recommended)
en.TypeVst3=VST3 plug-in only (for your DAW)
en.TypeStandalone=Standalone app only (no DAW needed)
en.TypeCustom=Custom installation
en.CompVst3=VST3 plug-in (Common Files\VST3)
en.CompStandalone=Standalone app
en.UserGuide=User guide
it.TypeFull=Plugin VST3 + app Standalone (consigliato)
it.TypeVst3=Solo plugin VST3 (per la tua DAW)
it.TypeStandalone=Solo app Standalone (senza DAW)
it.TypeCustom=Installazione personalizzata
it.CompVst3=Plugin VST3 (Common Files\VST3)
it.CompStandalone=App Standalone
it.UserGuide=Guida di utilizzo

[Types]
Name: "full"; Description: "{cm:TypeFull}"
Name: "vst3"; Description: "{cm:TypeVst3}"
Name: "standalone"; Description: "{cm:TypeStandalone}"
Name: "custom"; Description: "{cm:TypeCustom}"; Flags: iscustom

[Components]
Name: "vst3"; Description: "{cm:CompVst3}"; Types: full vst3 custom
Name: "standalone"; Description: "{cm:CompStandalone}"; Types: full standalone custom

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: standalone; Flags: unchecked

[Files]
; VST3 bundle -> the standard VST3 folder every Windows DAW scans
Source: "{#BuildDir}\VST3\Fase Groove Lead.vst3\*"; DestDir: "{commoncf64}\VST3\Fase Groove Lead.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
; Standalone app
Source: "{#BuildDir}\Standalone\Fase Groove Lead.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
; Docs, license and icon (always)
Source: "FaseGrooveLead.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoDir}\docs\GUIDE.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#RepoDir}\docs\GUIDA.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#RepoDir}\README.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#RepoDir}\CHANGELOG.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#RepoDir}\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "{#RepoDir}\PRIVACY.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#RepoDir}\THIRD_PARTY_NOTICES.md"; DestDir: "{app}\docs"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\Fase Groove Lead.exe"; Components: standalone
Name: "{group}\{cm:UserGuide}"; Filename: "{app}\docs\GUIDE.md"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\Fase Groove Lead.exe"; Tasks: desktopicon; Components: standalone

[Run]
Filename: "{app}\Fase Groove Lead.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent; Components: standalone
; Automatic update from the standalone app (/RELAUNCH=1): open it again when the setup is done.
Filename: "{app}\Fase Groove Lead.exe"; Flags: nowait runasoriginaluser; Components: standalone; Check: RelaunchRequested

[Code]
function RelaunchRequested: Boolean;
begin
  Result := ExpandConstant('{param:RELAUNCH|0}') = '1';
end;

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Fase Groove Lead.vst3"
