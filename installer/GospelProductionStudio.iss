; Gospel Production Studio - Windows installer (Inno Setup 6)
; Built automatically by the GitHub workflow.

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

#define AppName "Gospel Production Studio"
#define AppExe  "Gospel Production Studio.exe"

[Setup]
AppId={{8F3C2A51-6B7D-4E19-9C2A-5D1E7B4A9F10}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Minister Kemamah
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=GospelProductionStudio-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
UninstallDisplayIcon={app}\{#AppExe}
CloseApplications=yes

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\build\GospelProductionStudio_artefacts\Release\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
; After an automatic (silent) update, open the new version straight away.
Filename: "{app}\{#AppExe}"; Flags: nowait; Check: WizardSilent
