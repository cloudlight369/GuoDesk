#define AppName "GuoDesk"
#define AppVersion "3.18.0"
#ifndef SourceDir
#define SourceDir "..\artifacts\Release"
#endif
#ifndef OutSuffix
#define OutSuffix ""
#endif

[Setup]
AppId={{7E1F9C52-3D8A-4B7E-9C41-2A6F0D5B8E13}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=cloudlight369
DefaultDirName={autopf}\{#AppName}
PrivilegesRequired=lowest
OutputDir=..\artifacts\installer
OutputBaseFilename={#AppName}-{#AppVersion}{#OutSuffix}-setup
LicenseFile=..\LICENSE
SetupIconFile=..\src\GuoDesk\app.ico
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
UninstallDisplayIcon={app}\GuoDesk.exe
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.pdb,*.exp,*.lib"

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\GuoDesk.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\GuoDesk.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\GuoDesk.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
