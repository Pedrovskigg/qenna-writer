; Inno Setup script — Mira Writing (Mira 2, C++ Qt preview)
; Gera um installer .exe com tudo embutido. Compile com:
;   "C:\Users\pedro\AppData\Local\Programs\Inno Setup 6\ISCC.exe" mira-writing.iss

#define MyAppName       "Mira Writing"
#define MyAppVersion    "0.1.0-preview"
#define MyAppPublisher  "Pedro"
#define MyAppExeName    "mira-writing.exe"
#define BuildDir        "..\build-release"
#define ResourceDir     "..\resources"

[Setup]
AppId={{A3F2B6E0-7C5D-4D9C-9A3B-MIRA2-PREVIEW0001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Mira Writing
DefaultGroupName=Mira Writing
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile={#ResourceDir}\mira.ico
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
LZMANumBlockThreads=2
WizardStyle=modern
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=mira-writing-setup-{#MyAppVersion}
DisableDirPage=no
DisableReadyPage=no
ShowLanguageDialog=auto

[Languages]
Name: "pt_BR"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "en";    MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Executável e DLLs na raiz
Source: "{#BuildDir}\{#MyAppExeName}";       DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\Qt6Core.dll";           DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\Qt6Gui.dll";            DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\Qt6Widgets.dll";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\Qt6Svg.dll";            DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\Qt6Multimedia.dll";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\Qt6Network.dll";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\libgcc_s_seh-1.dll";    DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\libstdc++-6.dll";       DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\libwinpthread-1.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\avcodec-61.dll";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\avformat-61.dll";       DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\avutil-59.dll";         DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\swresample-5.dll";      DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\swscale-8.dll";         DestDir: "{app}"; Flags: ignoreversion

; Qt plugins (subpastas obrigatórias)
Source: "{#BuildDir}\platforms\*";           DestDir: "{app}\platforms";           Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\styles\*";              DestDir: "{app}\styles";              Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\iconengines\*";         DestDir: "{app}\iconengines";         Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\imageformats\*";        DestDir: "{app}\imageformats";        Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\multimedia\*";          DestDir: "{app}\multimedia";          Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\networkinformation\*";  DestDir: "{app}\networkinformation";  Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\tls\*";                 DestDir: "{app}\tls";                 Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\generic\*";             DestDir: "{app}\generic";             Flags: ignoreversion recursesubdirs

; Assets do app
Source: "{#BuildDir}\fonts\*";               DestDir: "{app}\fonts";               Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\spell\*";               DestDir: "{app}\spell";               Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\ambience-sounds\*";     DestDir: "{app}\ambience-sounds";     Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\theme-images\*";        DestDir: "{app}\theme-images";        Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\logo\*";                DestDir: "{app}\logo";                Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\{#MyAppName}";              Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Desinstalar {#MyAppName}";  Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";        Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
