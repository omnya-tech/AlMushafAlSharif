; ============================================================================
; ملف تثبيت «المصحف الشريف» (ويندوز 7 إلى 11 — 32-bit و 64-bit)
; ============================================================================

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef OutputDir
  #define OutputDir "release"
#endif

#define AppNameAr "المصحف الشريف"
#define AppNameEn "Al-Mushaf Al-Sharif"
#define AppExeName "المصحف_الشريف.exe"
#define AppPublisher "Omnya Technology"

[Setup]
AppId={{AF7629F3-F29A-4FAD-ABF9-2A506A5375E6}
AppName={cm:AppName}
AppVersion={#AppVersion}
AppVerName={cm:AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppCopyright=© 2026 {#AppPublisher}
AppComments={cm:AppComment}
DefaultDirName={autopf}\AlMushafAlSharif
DefaultGroupName={cm:AppName}
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=AlMushafAlSharif-Setup-{#AppVersion}
SetupIconFile=assets\icon.ico
UninstallDisplayIcon={app}\assets\icon.ico
UninstallDisplayName={cm:AppName} {#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
MinVersion=6.1sp1
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
CloseApplications=yes
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppNameEn} Setup (Win7-Win11 x86/x64)
VersionInfoProductName={#AppNameEn}
VersionInfoProductVersion={#AppVersion}
ShowLanguageDialog=yes
LanguageDetectionMethod=uilanguage

[Languages]
Name: "arabic"; MessagesFile: "compiler:Languages\Arabic.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
arabic.AppName={#AppNameAr}
english.AppName={#AppNameEn}
arabic.AppComment=هدية من فريق {#AppPublisher} للباحثين وطلاب الدراسات العليا وعموم المسلمين
english.AppComment=A gift from the {#AppPublisher} team to researchers, graduate students and all Muslims

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "bin\AlMushafAlSharif-x64.exe"; DestDir: "{app}"; DestName: "{#AppExeName}"; Check: Is64BitInstallMode; Flags: ignoreversion
Source: "bin\nvdaControllerClient64.dll"; DestDir: "{app}"; Check: Is64BitInstallMode; Flags: ignoreversion

Source: "bin\AlMushafAlSharif-win7-x86.exe"; DestDir: "{app}"; DestName: "{#AppExeName}"; Check: not Is64BitInstallMode; Flags: ignoreversion
Source: "bin\nvdaControllerClient32.dll"; DestDir: "{app}"; Check: not Is64BitInstallMode; Flags: ignoreversion

Source: "assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{cm:AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\assets\icon.ico"; Comment: "{cm:AppComment}"
Name: "{autodesktop}\{cm:AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\assets\icon.ico"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\AlMushafAlSharif"; ValueType: string; ValueName: "InstallerLanguage"; ValueData: "{language}"; Flags: uninsdeletekey

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{cm:AppName}}"; Flags: nowait postinstall skipifsilent
