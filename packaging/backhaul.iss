; The Windows installer (Inno Setup). Built by the packaging workflow from the folder
; out\backhaul (the program with everything it needs). Installs for the current user
; only, so no administrator rights are asked for: that is also what lets the updater
; replace the program without a prompt.
;   ISCC /DAppVersion=0.2.0 packaging\backhaul.iss
#ifndef AppVersion
  #error Pass the version: ISCC /DAppVersion=x.y.z
#endif

[Setup]
; Never change AppId: it is how a newer installer finds the older installation.
AppId={{8C5B6E0E-6C1B-4B7B-9E55-6B0C2B7D1A41}
AppName=backhaul
AppVersion={#AppVersion}
AppPublisher=SEVENTHWELL
AppPublisherURL=https://backhaul.seventh-well.com/
AppSupportURL=https://backhaul-gate.seventh-well.com/support
DefaultDirName={localappdata}\Programs\backhaul
DefaultGroupName=backhaul
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\out
OutputBaseFilename=backhaul-{#AppVersion}-setup
SetupIconFile=..\assets\logo\backhaul.ico
UninstallDisplayIcon={app}\backhaul.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; An update is installed while nothing of the old version runs: the updater closes the
; program first. Should it still run (started again by hand), Setup asks to close it.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\out\backhaul\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\backhaul"; Filename: "{app}\backhaul.exe"
Name: "{autodesktop}\backhaul"; Filename: "{app}\backhaul.exe"; Tasks: desktopicon

[Run]
; Installed by hand: offer to start it. Installed by the updater: start it again.
Filename: "{app}\backhaul.exe"; Description: "{cm:LaunchProgram,backhaul}"; Flags: nowait postinstall skipifsilent
Filename: "{app}\backhaul.exe"; Flags: nowait; Check: StartedByTheUpdater

[Code]
// The updater runs Setup with /relaunch=1 (see packaging/make_feed.py). A silent
// installation by anybody else does not start the program.
function StartedByTheUpdater: Boolean;
begin
  Result := ExpandConstant('{param:relaunch|0}') = '1';
end;
