#!powershell.exe -ExecutionPolicy Bypass -File
param(
    [String]$Configuration="Debug"
)

. ./BuildInfo.ps1

function Find-GameInstallPath {
    $gameFolder = $null

    # Try Steam first
    try {
        $steamDir = Get-ItemProperty -Path "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam" -Name "InstallPath" -ErrorAction Stop | Select-Object -ExpandProperty InstallPath
        $steamGamePath = Join-Path $steamDir "steamapps/common/BlackMythWukong"
        if (Test-Path $steamGamePath) {
            Write-Host "Discovered game in Steam: $steamGamePath"
            return $steamGamePath
        }
    } catch {
        # Steam registry key not found, continue to Epic
    }

    # Try Epic Games via manifest files
    $epicManifestFolder = "C:\ProgramData\Epic\EpicGamesLauncher\Data\Manifests"
    if (Test-Path $epicManifestFolder) {
        $manifestFiles = Get-ChildItem -Path $epicManifestFolder -Filter "*.item"
        foreach ($manifestFile in $manifestFiles) {
            try {
                $manifest = Get-Content $manifestFile.FullName | ConvertFrom-Json
                if ($manifest.AppName -eq "f53c5471fd0e47619e72b6d21a527abe") {
                    $epicPath = $manifest.InstallLocation
                    if (Test-Path $epicPath) {
                        Write-Host "Discovered game in Epic Games: $epicPath"
                        return $epicPath
                    }
                }
            } catch {
                # Skip invalid manifest files
            }
        }
    }

    # Not found
    Write-Error "Could not find BlackMythWukong installation in Steam or Epic Games directories."
    Exit 1
}

$gamePath = Find-GameInstallPath
$destRoot = Join-Path $gamePath "b1"

$coopBase = "$env:APPDATA/ReadyM.Launcher/WukongMP"

# Perform copies
foreach ($item in $allFiles) {
    $files = $item[0]
    $sourceDir = $item[1]

    # Chain the replace operations. The output of the first becomes the input for the second.
    $destDir = $item[2] -replace '@GAME', $destRoot -replace '@APPDATA', $coopBase

    CopyFiles $files $sourceDir $destDir
}