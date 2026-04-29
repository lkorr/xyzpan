; XYZPan Inno Setup Installer Script
; Installs VST3 plugin + factory presets to standard locations

#define MyAppName "XYZPan"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "pailiaq"

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

[Files]
; VST3 bundle — JUCE builds this as a directory tree
Source: "..\build\plugin\XYZPan_artefacts\Release\VST3\XYZPan.vst3\*"; \
    DestDir: "{app}\XYZPan.vst3"; Flags: ignoreversion recursesubdirs

[UninstallDelete]
; Clean up the VST3 bundle directory on uninstall
Type: filesandordirs; Name: "{app}\XYZPan.vst3"
