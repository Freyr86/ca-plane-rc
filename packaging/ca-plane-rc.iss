#ifndef GameDir
  #error GameDir must point to the packaged Windows directory
#endif
#ifndef AppVersion
  #define AppVersion "0.5.10"
#endif
#ifndef OutputDir
  #define OutputDir "..\work\release"
#endif
#ifndef AppVersionNumber
  #define AppVersionNumber "0.5.10.0"
#endif

[Setup]
AppId={{52B9BAE4-7204-4D88-95EC-C2CD5F225820}
AppName=Ça plane !
AppVersion={#AppVersion}
VersionInfoVersion={#AppVersionNumber}
AppPublisher=Freyr86
AppPublisherURL=https://github.com/Freyr86/ca-plane-rc
AppSupportURL=https://ca-plane-assistance.endlessbow16.chatgpt.site/
DefaultDirName={localappdata}\Programs\CaPlaneRC
DefaultGroupName=Ça plane !
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=CaPlaneRC-{#AppVersion}-Windows-Setup
Compression=lzma2/fast
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\unreal\SimPlaneurUE\Build\Windows\Application.ico
UninstallDisplayIcon={app}\SimPlaneurUE.exe
DisableProgramGroupPage=yes
SetupLogging=yes
CloseApplications=yes

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Créer un raccourci sur le Bureau"; GroupDescription: "Raccourcis :"

[Files]
Source: "{#GameDir}\*"; DestDir: "{app}"; Excludes: "\SimPlaneurUE\Saved\*,\compatibility-check.json"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Ça plane !"; Filename: "{app}\CaPlaneRCLauncher.exe"; WorkingDir: "{app}"; IconFilename: "{app}\SimPlaneurUE.exe"
Name: "{group}\Ça plane ! — Signaler un problème"; Filename: "{app}\CaPlaneRCLauncher.exe"; Parameters: "--report"; WorkingDir: "{app}"; IconFilename: "{app}\SimPlaneurUE.exe"
Name: "{autodesktop}\Ça plane !"; Filename: "{app}\CaPlaneRCLauncher.exe"; WorkingDir: "{app}"; IconFilename: "{app}\SimPlaneurUE.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\CaPlaneRCLauncher.exe"; Description: "Lancer Ça plane !"; Flags: nowait postinstall skipifsilent; Check: not ShouldRestartGame
Filename: "{app}\CaPlaneRCLauncher.exe"; Flags: nowait; Check: ShouldRestartGame

[Code]
function ShouldRestartGame: Boolean;
begin
  Result := ExpandConstant('{param:RESTARTGAME|0}') = '1';
end;
