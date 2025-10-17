#!powershell.exe -ExecutionPolicy Bypass -File
param(
    [String]$Configuration="Debug"
)

. ./BuildInfo.ps1

$steamDir = Get-ItemProperty -Path "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam" -Name "InstallPath" | Select-Object -ExpandProperty InstallPath
$destRoot = "$steamDir/steamapps/common/BlackMythWukong/b1"

$coopBase = "$env:APPDATA/ReadyM.Launcher/WukongMP"

# Perform copies
foreach ($item in $allFiles) {
    $files = $item[0]
    $sourceDir = $item[1]

    # Chain the replace operations. The output of the first becomes the input for the second.
    $destDir = $item[2] -replace '@GAME', $destRoot -replace '@APPDATA', $coopBase

    CopyFiles $files $sourceDir $destDir
}