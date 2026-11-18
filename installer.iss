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
; التثبيت في Program Files لكل مستخدمي الجهاز (يطلب ويندوز إذن المسؤول مرة واحدة عند التثبيت)
PrivilegesRequired=admin
; لا يعود إلى مجلد تثبيت سابق في حساب المستخدم (AppData\Local\Programs)، بل يثبّت في Program Files
UsePreviousAppDir=no
CloseApplications=yes
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppNameEn} Setup (Win7-Win11 x86/x64)
VersionInfoProductName={#AppNameEn}
VersionInfoProductVersion={#AppVersion}
ShowLanguageDialog=yes
LanguageDetectionMethod=uilanguage

[Languages]
; صفحة اتفاقية الترخيص تظهر بلغة التثبيت المختارة، ولا يكمل التثبيت إلا بالموافقة عليها
Name: "arabic"; MessagesFile: "compiler:Languages\Arabic.isl"; LicenseFile: "legal\LICENSE-ar.txt"
Name: "english"; MessagesFile: "compiler:Default.isl"; LicenseFile: "legal\LICENSE-en.txt"

[CustomMessages]
arabic.AppName={#AppNameAr}
english.AppName={#AppNameEn}
arabic.AppComment=هدية من فريق {#AppPublisher} للباحثين وعموم المسلمين
english.AppComment=A gift from the {#AppPublisher} team to researchers and all Muslims

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "bin\AlMushafAlSharif-x64.exe"; DestDir: "{app}"; DestName: "{#AppExeName}"; Check: Is64BitInstallMode; Flags: ignoreversion
Source: "bin\nvdaControllerClient64.dll"; DestDir: "{app}"; Check: Is64BitInstallMode; Flags: ignoreversion

Source: "bin\AlMushafAlSharif-win7-x86.exe"; DestDir: "{app}"; DestName: "{#AppExeName}"; Check: not Is64BitInstallMode; Flags: ignoreversion
Source: "bin\nvdaControllerClient32.dll"; DestDir: "{app}"; Check: not Is64BitInstallMode; Flags: ignoreversion

Source: "assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs

; نص الاتفاقية بالعربية والإنجليزية يبقى مع البرنامج المثبَّت
Source: "legal\LICENSE-ar.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "legal\LICENSE-en.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{cm:AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\assets\icon.ico"; Comment: "{cm:AppComment}"
Name: "{autodesktop}\{cm:AppName}"; Filename: "{app}\{#AppExeName}"; IconFilename: "{app}\assets\icon.ico"; Tasks: desktopicon

[InstallDelete]
; قد يكون اسم الاختصار قد تغيّر مع لغة البرنامج، فلا يبقى اختصاران بعد التحديث
Type: files; Name: "{autoprograms}\{#AppNameAr}.lnk"
Type: files; Name: "{autoprograms}\{#AppNameEn}.lnk"
Type: files; Name: "{autodesktop}\{#AppNameAr}.lnk"
Type: files; Name: "{autodesktop}\{#AppNameEn}.lnk"

[UninstallDelete]
Type: files; Name: "{autoprograms}\{#AppNameAr}.lnk"
Type: files; Name: "{autoprograms}\{#AppNameEn}.lnk"
Type: files; Name: "{autodesktop}\{#AppNameAr}.lnk"
Type: files; Name: "{autodesktop}\{#AppNameEn}.lnk"

[Registry]
Root: HKCU; Subkey: "Software\AlMushafAlSharif"; ValueType: string; ValueName: "InstallerLanguage"; ValueData: "{language}"; Flags: uninsdeletekey

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{cm:AppName}}"; Flags: nowait postinstall skipifsilent

[Code]
// الترقية من الإصدارات التي كانت تُثبَّت في حساب المستخدم (AppData\Local\Programs):
// قبل نسخ الملفات يُزال ذلك التثبيت القديم بصمت، فلا تبقى نسختان ولا سطران في «إزالة البرامج».
// إعدادات القارئ وموضع قراءته في AppData\Roaming لا يمسّها المزيل، فتبقى كما هي بعد التحديث.
const
  OldUninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{AF7629F3-F29A-4FAD-ABF9-2A506A5375E6}_is1';

procedure RemoveOldPerUserInstall();
var
  Uninstaller: String;
  ResultCode: Integer;
begin
  if not RegQueryStringValue(HKCU, OldUninstallKey, 'UninstallString', Uninstaller) then
    exit;
  Uninstaller := RemoveQuotes(Uninstaller);
  if not FileExists(Uninstaller) then
    exit;
  Exec(Uninstaller, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    RemoveOldPerUserInstall();
end;
