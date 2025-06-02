#!powershell.exe -ExecutionPolicy Bypass -File
param(
    [String]$Configuration="Debug"
)

. ./BuildInfo.ps1

$steamDir = Get-ItemProperty -Path "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam" -Name "InstallPath" | Select-Object -ExpandProperty InstallPath
$destRoot = "$steamDir/steamapps/common/BlackMythWukong/b1"

# Perform copies
foreach ($item in $allFiles) {
    $files = $item[0]
    $sourceDir = $item[1]
    $destDir = Join-Path $destRoot $item[2]

    CopyFiles $files $sourceDir $destDir
}
