#!powershell.exe -ExecutionPolicy Bypass -File

$solutionName = "EmbedCSharpLoader"
$zipName = "Loader"

# Define the source and destination directories
$nativeSourceDir = "x64/$Configuration"
$bootstrapSourceDir = "ReadyM.Loader.Wukong.Bootstrap/bin/$Configuration/net472/win-x64"
$managedSourceDir = "ReadyM.Loader.Wukong.Managed/bin/$Configuration/net472/win-x64"
$pakSourceDir = "PakFiles"

$nativeDestDir = "Binaries/Win64"
$bootstrapDestDir = "Binaries/Win64/CSharpLoader"
$managedDestDir = "Binaries/Win64/CSharpLoader"
$pakDestDir = "Content/Paks/LogicMods"

# Define the files to copy
$nativeFiles = @(
    "version.dll",
    "version.pdb"
)
$bootstrapFiles = @(
    "ReadyM.Loader.Wukong.Bootstrap.dll",
    "ReadyM.Loader.Wukong.Bootstrap.pdb"
)
$managedFiles = @(
    "0Harmony.dll",
    "ReadyM.Loader.Wukong.Managed.dll",
    "ReadyM.Loader.Wukong.Managed.pdb",
    "INIFileParser.dll",
    "CSharpModBase.dll",
    "CSharpModBase.pdb",
    "Mono.Cecil.dll",
    "Mono.Cecil.Mdb.dll",
    "Mono.Cecil.Pdb.dll", 
    "Mono.Cecil.Rocks.dll",
    "SharpDX.dll", 
    "SharpDX.XInput.dll"
)
$pakFiles = @(
    "WukongMp.pak"
)

$allFiles = @(
    @($nativeFiles, $nativeSourceDir, $nativeDestDir),
    @($bootstrapFiles, $bootstrapSourceDir, $bootstrapDestDir),
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
