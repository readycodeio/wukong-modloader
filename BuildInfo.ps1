#!powershell.exe -ExecutionPolicy Bypass -File

$solutionName = "EmbedCSharpLoader"
$zipName = "Loader"

# Define the source and destination directories
$nativeSourceDir = "x64/$Configuration"
$overridesSourceDir = "ReadyM.Loader.Executable/bin/$Configuration"
$toplevelSourceDir = "ReadyM.Loader.Executable/bin/$Configuration"
$facadesSourceDir = "ReadyM.Loader.Executable/Facades"
$originalSourceDir = "Original"
$pakSourceDir = "PakFiles"
$configSourceDir = "Config"

$nativeDestDir = "@GAME/Binaries/Win64"
$overridesDestDir = "@APPDATA/CSharpLoader/Overrides"
$facadesDestDir = "@APPDATA/CSharpLoader/Overrides"
$toplevelDestDir = "@APPDATA/CSharpLoader"
$pakDestDir = "@GAME/Content/Paks/LogicMods"

# Define the files to copy
$originalNativeFiles = @(
    "version.dll"
)
$nativeFiles = @(
    "dxgi.dll",
    @("dxgi.pdb", $true)
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
    "Mono.Cecil.dll",
    "Mono.Cecil.Mdb.dll",
    "Mono.Cecil.Pdb.dll", 
    "Mono.Cecil.Rocks.dll"
    "System.Buffers.dll",
    "System.Diagnostics.DiagnosticSource.dll",
    "System.IO.Pipelines.dll",
    "System.Memory.dll",
    "System.Numerics.Vectors.dll",
    "System.Runtime.CompilerServices.Unsafe.dll",
    "System.Text.Encodings.Web.dll",
    "System.Text.Json.dll",
    "System.Threading.Tasks.Extensions.dll",
    "System.ValueTuple.dll"
)
$facadesFiles = @(
    "System.Runtime.dll"
)
$toplevelFiles = @(
    "0Harmony.dll",
    "PreludeLib.dll",
    @("PreludeLib.pdb", $true),
    "SharpDX.dll", 
    "SharpDX.XInput.dll",
    "CSharpModBase.dll",
    @("CSharpModBase.pdb", $true),
    "CSharpModBaseV2.dll",
    @("CSharpModBaseV2.pdb", $true),
    "ReadyM.Loader.Wukong.Bootstrap.dll",
    @("ReadyM.Loader.Wukong.Bootstrap.pdb", $true),
    "ReadyM.Loader.Wukong.Managed.dll",
    @("ReadyM.Loader.Wukong.Managed.pdb", $true),
    "INIFileParser.dll"
)
$configFiles = @(
    "b1cs.ini",
    "debugger-agent.txt"
)
$pakFiles = @(
    "CoreMp.pak",
    "WukongMp.pak"
)

$allFiles = @(
    @($nativeFiles, $nativeSourceDir, $nativeDestDir),
    @($originalNativeFiles, $originalSourceDir, $nativeDestDir),
    @($overridesFiles, $overridesSourceDir, $overridesDestDir),
    @($facadesFiles, $facadesSourceDir, $facadesDestDir),
    @($toplevelFiles, $toplevelSourceDir, $toplevelDestDir),
    @($configFiles, $configSourceDir, $toplevelDestDir),
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
