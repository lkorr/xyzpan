; XYZPan Inno Setup Installer Script
; Installs VST3 plugin + factory presets to standard locations

#define MyAppName "XYZPan"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "XYZAudio"

[Setup]
AppId={{B7E4A3C1-9F2D-4E8B-A6D5-3C1F7E9B2A4D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={commoncf}\VST3
DefaultGroupName={#MyAppPublisher}
DisableProgramGroupPage=yes
OutputDir=output
OutputBaseFilename=XYZPan-{#MyAppVersion}-Setup
Compression=lzma
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayName={#MyAppName} {#MyAppVersion}
DisableDirPage=no

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "plugin"; Description: "XYZPan VST3 Plugin"; Types: full custom; Flags: fixed
Name: "presets"; Description: "Factory Presets"; Types: full custom

[Files]
; VST3 bundle — JUCE builds this as a directory tree
Source: "..\build\plugin\XYZPan_artefacts\Release\VST3\XYZPan.vst3\*"; \
    DestDir: "{app}\XYZPan.vst3"; Flags: ignoreversion recursesubdirs; \
    Components: plugin

; Factory presets → ProgramData (shared across all users)
Source: "..\presets\factory\*.xml"; \
    DestDir: "{commonappdata}\VST3 Presets\XYZAudio\XYZPan"; \
    Flags: ignoreversion; Components: presets

[UninstallDelete]
; Clean up the VST3 bundle directory on uninstall
Type: filesandordirs; Name: "{app}\XYZPan.vst3"
; Clean up factory presets directory
Type: filesandordirs; Name: "{commonappdata}\VST3 Presets\XYZAudio\XYZPan"
