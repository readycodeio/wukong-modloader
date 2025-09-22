#!powershell.exe -ExecutionPolicy Bypass -File

$solutionName = "EmbedCSharpLoader"
$zipName = "Loader"

# Define the source and destination directories
$nativeSourceDir = "x64/$Configuration"
$bootstrapSourceDir = "ReadyM.Loader.Wukong.Bootstrap/bin/$Configuration/netstandard2.0/win-x64"
$csharpModBaseSourceDir = "CSharpModBase/bin/$Configuration/netstandard2.0"
$csharpModBaseV2SourceDir = "CSharpModBaseV2/bin/$Configuration/netstandard2.0"
$managedSourceDir = "ReadyM.Loader.Executable/bin/$Configuration"
$overridesSourceDir = "ReadyM.Loader.Wukong.Managed/bin/$Configuration/netstandard2.0/win-x64"
$pakSourceDir = "PakFiles"

$nativeDestDir = "Binaries/Win64"
$bootstrapDestDir = "Binaries/Win64/CSharpLoader"
$csharpModBaseDestDir = "Binaries/Win64/CSharpLoader"
$csharpModBaseV2DestDir = "Binaries/Win64/CSharpLoader"
$managedDestDir = "Binaries/Win64/CSharpLoader"
$overridesDestDir = "Binaries/Win64/CSharpLoader/Overrides"
$pakDestDir = "Content/Paks/LogicMods"

# Define the files to copy
$nativeFiles = @(
    "version.dll",
    @("version.pdb", $true)
)
$bootstrapFiles = @(
    "ReadyM.Loader.Wukong.Bootstrap.dll",
    @("ReadyM.Loader.Wukong.Bootstrap.pdb", $true)
)
$csharpModBaseFiles = @(
    "0Harmony.dll",
    "CSharpModBase.dll",
    @("CSharpModBase.pdb", $true),
    "SharpDX.dll", 
    "SharpDX.XInput.dll",
    "MonoMod.Backports.dll",
    "MonoMod.ILHelpers.dll",
    "MonoMod.Utils.dll",
    "PreludeLib.dll"
)
$csharpModBaseV2Files = @(
    "CSharpModBaseV2.dll",
    @("CSharpModBaseV2.pdb", $true)
)
$managedFiles = @(
    "ReadyM.Loader.Wukong.Managed.dll",
    @("ReadyM.Loader.Wukong.Managed.pdb", $true),
    "INIFileParser.dll",
    "CSharpModBase.dll",
    @("CSharpModBase.pdb", $true),
    "CSharpModBaseV2.dll",
    @("CSharpModBaseV2.pdb", $true),
    "Mono.Cecil.dll",
    "Mono.Cecil.Mdb.dll",
    "Mono.Cecil.Pdb.dll", 
    "Mono.Cecil.Rocks.dll"
)
$overridesFiles = @(
    "Microsoft.Bcl.AsyncInterfaces.dll",
    "Microsoft.Extensions.Configuration.dll",
    "Microsoft.Extensions.Configuration.Abstractions.dll",
    "Microsoft.Extensions.Configuration.Binder.dll",
    "Microsoft.Extensions.DependencyInjection.dll",
    "Microsoft.Extensions.DependencyInjection.Abstractions.dll",
    "Microsoft.Extensions.Logging.dll",
    "Microsoft.Extensions.Logging.Abstractions.dll",
    "Microsoft.Extensions.Logging.Configuration.dll",
    "Microsoft.Extensions.Logging.Console.dll",
    "Microsoft.Extensions.Options.dll",
    "Microsoft.Extensions.Options.ConfigurationExtensions.dll",
    "Microsoft.Extensions.Primitives.dll"
    "System.Buffers.dll",
    "System.ComponentModel.Annotations.dll",
    "System.Diagnostics.DiagnosticSource.dll",
    "System.IO.Pipelines.dll",
    "System.Memory.dll",
    "System.Numerics.Vectors.dll",
    "System.Runtime.CompilerServices.Unsafe.dll",
    "System.Text.Encodings.Web.dll",
    "System.Text.Json.dll",
    "System.Threading.Tasks.Extensions.dll",
    "Mono.Cecil.dll",
    "Mono.Cecil.Mdb.dll",
    "Mono.Cecil.Pdb.dll", 
    "Mono.Cecil.Rocks.dll"
)
$pakFiles = @(
    "WukongMp.pak"
)

$allFiles = @(
    @($nativeFiles, $nativeSourceDir, $nativeDestDir),
    @($bootstrapFiles, $bootstrapSourceDir, $bootstrapDestDir),
    @($csharpModBaseFiles, $csharpModBaseSourceDir, $csharpModBaseDestDir),
    @($csharpModBaseV2Files, $csharpModBaseV2SourceDir, $csharpModBaseV2DestDir),
    @($managedFiles, $managedSourceDir, $managedDestDir),
    @($overridesFiles, $overridesSourceDir, $overridesDestDir),
    @($pakFiles, $pakSourceDir, $pakDestDir)
)

function CopyFiles($files, $sourceDir, $destDir) {
    # Create the destination directory if it doesn't exist
    if (!(Test-Path -Path $destDir)) {
        New-Item -ItemType Directory -Path $destDir -Force
    }
    
    # Copy each file to the destination directory
    foreach ($item in $files) {
        if ($item.Count -eq 2) {
            $file = $item[0]
            $optional = $item[1]
        } else {
            $file = $item
            $optional = $false
        }

        # do not include any PDBs in the release build
        if ($optional) {
            continue
        }
        
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
            if ($optional) {
                continue
            }
            Write-Output "[Error] $file does not exist in $sourceDir"
        }
    }
}
