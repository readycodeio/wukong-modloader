#!powershell.exe -ExecutionPolicy Bypass -File
param(
    [String]$Configuration="Release",
    # Full recompile of every project, the C++ ones included. Off by default, because that is
    # minutes of native compilation that an incremental build skips.
    [switch]$Rebuild
)

$scriptDir = $PSScriptRoot

. (Join-Path $scriptDir 'BuildInfo.ps1')

function Find-MSBuild($vswhere, [string]$Pattern, [bool]$Prerelease)
{
    $arguments = @('-latest', '-products', '*', '-requires', 'Microsoft.Component.MSBuild', '-find', $Pattern)
    if ($Prerelease)
    {
        $arguments = @('-prerelease') + $arguments
    }

    $found = & $vswhere @arguments 2>$null | Select-Object -First 1
    if ($found -and (Test-Path $found))
    {
        return $found
    }

    return $null
}

function Resolve-MSBuild
{
    # A Developer PowerShell puts MSBuild on PATH, a plain one does not, so ask the VS installer
    # where it is. The C++ projects need full MSBuild, so `dotnet build` is not a substitute.
    $onPath = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($onPath)
    {
        return $onPath.Source
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere))
    {
        Write-Error "MSBuild.exe is not on PATH and vswhere.exe is not at $vswhere. Install Visual Studio, or run this from a Developer PowerShell."
        exit 1
    }

    # amd64 first: the 32-bit MSBuild can run out of memory on the native projects.
    foreach ($pattern in @('MSBuild\**\Bin\amd64\MSBuild.exe', 'MSBuild\**\Bin\MSBuild.exe'))
    {
        foreach ($prerelease in @($false, $true))
        {
            $found = Find-MSBuild $vswhere $pattern $prerelease
            if ($found)
            {
                return $found
            }
        }
    }

    Write-Error "vswhere.exe found no MSBuild with C++ support. Install the 'Desktop development with C++' workload."
    exit 1
}

# 1. Build solution
$solutionPath = Join-Path $scriptDir "$solutionName.sln"
if (-not (Test-Path $solutionPath)) {
    Write-Error "Solution file not found at $solutionPath"
    exit 1
}

$msbuild = Resolve-MSBuild
$target = if ($Rebuild) { 'Rebuild' } else { 'Build' }

Write-Output "MSBuild: $msbuild"
Write-Output "Building solution $solutionPath in configuration $Configuration (target $target)..."

& $msbuild $solutionPath `
    /property:Configuration=$Configuration `
    /property:Platform=x64 `
    /target:$target `
    /maxcpucount `
    /nologo `
    /verbosity:minimal | Tee-Object -FilePath (Join-Path $scriptDir 'build.log')

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE. See build.log."
    exit 1
}

# 2. Read the version off the assembly that was just built.
#
# Not scraped from the build output any more: Directory.Build.props derives the version from the
# clock, so the build log reports a freshly evaluated value even for projects that were not
# recompiled. The assembly is the only thing that knows which version actually shipped.
$versionSource = Join-Path $scriptDir "$toplevelSourceDir/ReadyM.Loader.Wukong.Managed.dll"
if (-not (Test-Path $versionSource)) {
    Write-Error "Cannot read the build version, $versionSource was not produced."
    exit 1
}

$version = (Get-Item $versionSource).VersionInfo.FileVersion
if ($version -notmatch '^\d+(\.\d+){3}$') {
    Write-Error "Unexpected version '$version' on $versionSource."
    exit 1
}

Write-Output "Build version: $version"

# 3. Prepare temporary output directory
$outputRoot = Join-Path $scriptDir 'Output'
if (-not (Test-Path $outputRoot)) {
    New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
}
$destRoot = Join-Path $outputRoot "$zipName-$version"
New-Item -ItemType Directory -Path $destRoot -Force | Out-Null

foreach ($item in $allFiles) {
    $destDir = Join-Path $destRoot $item[2]
    if (-not (Test-Path $destDir)) { New-Item -ItemType Directory -Path $destDir -Force | Out-Null }
}

# 4. Perform copies
foreach ($item in $allFiles) {
    $files = $item[0]
    $sourceDir = Join-Path $scriptDir $item[1]
    $destDir = Join-Path $destRoot $item[2]

    CopyFiles $files $sourceDir $destDir
}

# 5. Zip files
$zipName = "$zipName-$version.7z"
$zipPath = Join-Path $outputRoot $zipName
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

7z a -t7z -mx=9 -ms=on -mmt=on $zipPath (Join-Path $destRoot '*')
Write-Output "Created $zipName"

# 7. Open Output folder in explorer
Start-Process -FilePath "explorer.exe" -ArgumentList $outputRoot
