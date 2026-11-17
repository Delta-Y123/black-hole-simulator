; Inno Setup script for Black Hole Simulator (B-tier core build)
#define AppName "Black Hole Simulator"
#define AppVersion "0.1.0"
#define AppExe "BlackHoleSimulator.exe"

[Setup]
AppId={{8E4B2C71-3F5A-4C1D-9A21-6B7E5D0C4A88}
AppName={#AppName}
AppVersion={#AppVersion}
DefaultDirName={autopf}\BlackHoleSimulator
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
OutputDir=installer_out
OutputBaseFilename=BlackHoleSimulator_Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务:"

[Files]
Source: "dist\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "启动 {#AppName}"; Flags: nowait postinstall skipifsilent
