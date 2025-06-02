#!powershell.exe -ExecutionPolicy Bypass -File

$solutionName = "CSharpLoader"
$zipName = "Loader"

# Define the source and destination directories
$nativeSourceDir = "x64/$Configuration"
$managedSourceDir = "EmbedCSharpLoader.Managed/bin/$Configuration/net472"
$pakSourceDir = "PakFiles"

$nativeDestDir = "Binaries/Win64"
$managedDestDir = "Binaries/Win64/CSharpLoader"
$pakDestDir = "Content/Paks/LogicMods"

# Define the files to copy
$nativeFiles = @("version.dll")
$managedFiles = @(
    "0Harmony.dll",
    "EmbedCSharpManager.Managed.dll",
    "EmbedCSharpManager.Managed.pdb",
    "CSharpModBase.dll",
    "CSharpModBase.pdb",
    "Mono.Cecil.dll",
    "Mono.Cecil.Mdb.dll",
    "Mono.Cecil.Pdb.dll", 
    "Mono.Cecil.Rocks.dll",
    "SharpDX.dll", 
    "SharpDX.XInput.dll",
    "System.Runtime.CompilerServices.Unsafe.dll"
)
$pakFiles = @(
    "ScreenshotSaverMod.pak", 
    "WukongMp.pak"
)

$allFiles = @(
    @($nativeFiles, $nativeSourceDir, $nativeDestDir),
    @($managedFiles, $managedSourceDir, $managedDestDir),
    @($pakFiles, $pakSourceDir, $pakDestDir)
)

function CopyFiles($files, $sourceDir, $destDir) {
    # Create the destination directory if it doesn't exist
    if (!(Test-Path -Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force
    }
    
    # Copy each file to the destination directory
    foreach ($file in $files) {
        $sourceFile = Join-Path -Path $sourceDir -ChildPath $file
        $destFile = Join-Path -Path $destDir -ChildPath $file
        if ($file -eq "*") {
            if (Test-Path -Path $destDir) {
                Remove-Item -Path $destDir -Recurse -Force
            }
            New-Item -ItemType Directory -Path $destDir -Force
            Copy-Item -Path $sourceFile -Destination $destDir -Recurse -Force
            Write-Output "Copied $file to $destDir (recursive)"
        } elseif (Test-Path -Path $sourceFile -PathType Leaf) {
            Copy-Item -Path $sourceFile -Destination $destFile -Force
            Write-Output "Copied $file to $destDir"
        } elseif (Test-Path -Path $sourceFile -PathType Container) {
            if (Test-Path -Path $destFile) {
                Remove-Item -Path $destFile -Recurse -Force
            }
            Copy-Item -Path $sourceFile -Destination $destFile -Recurse -Force
            Write-Output "Copied $file to $destDir (recursive)"
        } else {
            Write-Output "[Error] $file does not exist in $sourceDir"
        }
    }
}
