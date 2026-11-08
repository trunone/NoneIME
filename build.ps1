param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidateSet('Win32', 'x64')]
    [string]$Platform = 'x64',
    [string]$Target
)

$ErrorActionPreference = 'Stop'

$commitHash = (git rev-parse --short=7 HEAD 2>$null)
if ($commitHash) {
    $commitHash = $commitHash.Trim()
    $rcPath = Join-Path $PSScriptRoot 'NoneIME.rc'
    if (Test-Path $rcPath) {
        $rcContent = Get-Content -Path $rcPath -Raw -Encoding UTF8
        $rcContent = $rcContent -replace 'VALUE "FileVersion", "[^"]*"', "VALUE `"FileVersion`", `"$commitHash`""
        $rcContent = $rcContent -replace 'VALUE "ProductVersion", "[^"]*"', "VALUE `"ProductVersion`", `"$commitHash`""
        Set-Content -Path $rcPath -Value $rcContent -Encoding UTF8
    }
}

$cargoCmd = Get-Command cargo -ErrorAction SilentlyContinue
if (-not $cargoCmd) {
    throw 'Cargo was not found in PATH. Please install the Rust toolchain.'
}

# Determine cargo target triple
if (-not $Target) {
    $defaultTarget = (rustc -vV | Select-String '^host:\s*(.+)$').Matches.Groups[1].Value.Trim()
    if ($defaultTarget -like '*msvc*') {
        $Target = if ($Platform -eq 'x64') { 'x86_64-pc-windows-msvc' } else { 'i686-pc-windows-msvc' }
    } elseif ($defaultTarget -like '*gnu*') {
        $Target = if ($Platform -eq 'x64') { 'x86_64-pc-windows-gnu' } else { 'i686-pc-windows-gnu' }
    } else {
        $Target = if ($Platform -eq 'x64') { 'x86_64-pc-windows-msvc' } else { 'i686-pc-windows-msvc' }
    }
}

Write-Host "Building NoneIME using Cargo (Target: $Target, Profile: $Configuration)..."

$cargoArgs = @('build', '--lib', '--target', $Target)
if ($Configuration -eq 'Release') {
    $cargoArgs += '--release'
}

& cargo @cargoArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$profileDir = if ($Configuration -eq 'Release') { 'release' } else { 'debug' }
$sourceDll = Join-Path $PSScriptRoot "target\$Target\$profileDir\NoneIME.dll"
if (-not (Test-Path $sourceDll)) {
    # Check default target directory fallback
    $sourceDll = Join-Path $PSScriptRoot "target\$profileDir\NoneIME.dll"
}

if (-not (Test-Path $sourceDll)) {
    throw "Build output DLL not found at expected location: $sourceDll"
}

$outputPath = Join-Path $PSScriptRoot "bin\$Platform\$Configuration"
if (-not (Test-Path $outputPath)) {
    New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
}

Copy-Item -Path $sourceDll -Destination (Join-Path $outputPath 'NoneIME.dll') -Force
$iniPath = Join-Path $PSScriptRoot 'NoneIME.ini'
if (Test-Path $iniPath) {
    Copy-Item -Path $iniPath -Destination $outputPath -Force
}
Copy-Item -Path (Join-Path $PSScriptRoot 'install.ps1') -Destination $outputPath -Force
Copy-Item -Path (Join-Path $PSScriptRoot 'uninstall.ps1') -Destination $outputPath -Force

Write-Host "Build complete! Artifacts copied to: $outputPath"
