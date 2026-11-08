#ifndef GameDir
  #error GameDir must point to the packaged Windows directory
#endif
#ifndef AppVersion
  #define AppVersion "0.5.6"
#endif
#ifndef OutputDir
  #define OutputDir "..\work\release"
#endif

[Setup]
AppId={{52B9BAE4-7204-4D88-95EC-C2CD5F225820}
AppName=Ça plane !
AppVersion={#AppVersion}
AppPublisher=Freyr86
AppPublisherURL=https://github.com/Freyr86/ca-plane-rc
AppSupportURL=https://github.com/Freyr86/ca-plane-rc/issues
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
Source: "{#GameDir}\*"; DestDir: "{app}"; Excludes: "\SimPlaneurUE\Saved\*"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Ça plane !"; Filename: "{app}\SimPlaneurUE.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Ça plane !"; Filename: "{app}\SimPlaneurUE.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\SimPlaneurUE.exe"; Description: "Lancer Ça plane !"; Flags: nowait postinstall skipifsilent
