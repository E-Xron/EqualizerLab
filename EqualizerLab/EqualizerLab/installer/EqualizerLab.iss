; Windows installer: puts the VST3 where every DAW looks for it and installs the standalone app, in one step.
; Built by the GitHub Actions workflow: ISCC /DSrcDir=<folder with EqualizerLab.vst3 and EqualizerLab.exe> /DOutDir=<output>

#define AppName "EqualizerLab"
#define AppVersion "1.0.0"

[Setup]
AppId={{6F1C2B7E-3D4A-4E59-9B21-5E0A11000001}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=EqualizerLab
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir={#OutDir}
OutputBaseFilename=EqualizerLab-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}

[Types]
Name: "full"; Description: "VST3 plug-in and standalone app"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Ableton, FL Studio, Cubase, Studio One, Reaper, Bitwig...)"; Types: full custom; Flags: fixed
Name: "app"; Description: "Standalone app"; Types: full

[Files]
Source: "{#SrcDir}\EqualizerLab.vst3\*"; DestDir: "{commoncf64}\VST3\EqualizerLab.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SrcDir}\EqualizerLab.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\EqualizerLab.exe"; Components: app
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\EqualizerLab.exe"; Components: app; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Components: app; Flags: unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\EqualizerLab.vst3"

[Run]
Filename: "{app}\EqualizerLab.exe"; Description: "Open EqualizerLab now"; Components: app; Flags: nowait postinstall skipifsilent unchecked
