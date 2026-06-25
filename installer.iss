[Setup]
AppName=USSC Calendar
AppVersion=1.0
AppPublisher=Aronetics
AppPublisherURL=https://aronetics.com
AppId={{B7A2C4E1-3F8D-4A92-B0E5-D6C3F9A12345}
DefaultDirName={autopf}\USSC Calendar
DefaultGroupName=USSC Calendar
AllowNoIcons=no
OutputDir=installer
OutputBaseFilename=USSC_Calendar_Setup
SetupIconFile=app.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"

[Files]
Source: "redist\vc_redist.x64.exe";                  DestDir: "{tmp}"; Flags: deleteafterinstall
Source: "out\Release\ussc_calendar.exe";              DestDir: "{app}"; Flags: ignoreversion
Source: "out\Release\wxbase331u_vc_x64_custom.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "out\Release\wxmsw331u_core_vc_x64_custom.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "out\Release\jpeg62.dll";                     DestDir: "{app}"; Flags: ignoreversion
Source: "out\Release\liblzma.dll";                    DestDir: "{app}"; Flags: ignoreversion
Source: "out\Release\libpng16.dll";                   DestDir: "{app}"; Flags: ignoreversion
Source: "out\Release\libsharpyuv.dll";                DestDir: "{app}"; Flags: ignoreversion
Source: "out\Release\libwebp.dll";                    DestDir: "{app}"; Flags: ignoreversion
Source: "out\Release\libwebpdecoder.dll";             DestDir: "{app}"; Flags: ignoreversion
Source: "out\Release\libwebpdemux.dll";               DestDir: "{app}"; Flags: ignoreversion
Source: "out\Release\pcre2-16.dll";                   DestDir: "{app}"; Flags: ignoreversion
Source: "out\Release\tiff.dll";                       DestDir: "{app}"; Flags: ignoreversion
Source: "out\Release\z.dll";                          DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\USSC Calendar";       Filename: "{app}\ussc_calendar.exe"
Name: "{group}\Uninstall USSC Calendar"; Filename: "{uninstallexe}"
Name: "{autodesktop}\USSC Calendar"; Filename: "{app}\ussc_calendar.exe"; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; \
    StatusMsg: "Installing Visual C++ Runtime..."; Flags: waituntilterminated
Filename: "{app}\ussc_calendar.exe"; Description: "Launch USSC Calendar"; Flags: nowait postinstall skipifsilent
