param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidateSet('Win32', 'x64')]
    [string]$Platform = 'x64',
    [string]$PlatformToolset
)

$ErrorActionPreference = 'Stop'
$solutionPath = Join-Path $PSScriptRoot 'NoneIME.sln'
if (-not (Test-Path $solutionPath)) {
    throw "Solution not found: $solutionPath"
}

$msbuildPath = $null
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$toolsetMap = @{
    '14.5' = 'v145'
    '14.3' = 'v143'
    '14.2' = 'v142'
    '14.1' = 'v141'
    '14.0' = 'v140'
    '12.0' = 'v120'
    '11.0' = 'v110'
}

if (Test-Path $vswherePath) {
    $installPaths = & $vswherePath -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
    foreach ($installPath in $installPaths) {
        $candidate = Join-Path $installPath 'MSBuild\Current\Bin\MSBuild.exe'
        if (Test-Path $candidate) {
            $msbuildPath = $candidate
            $vcToolsRoot = Join-Path $installPath 'VC\Tools\MSVC'
            if (Test-Path $vcToolsRoot) {
                $toolsetDirs = Get-ChildItem -Path $vcToolsRoot -Directory |
                    Sort-Object { [version]($_.Name) } -Descending
                if (-not $PlatformToolset -and $toolsetDirs) {
                    foreach ($toolsetDir in $toolsetDirs) {
                        foreach ($versionPrefix in $toolsetMap.Keys | Sort-Object { [version]$_ } -Descending) {
                            if ($toolsetDir.Name.StartsWith($versionPrefix)) {
                                $PlatformToolset = $toolsetMap[$versionPrefix]
                                break
                            }
                        }
                        if ($PlatformToolset) {
                            break
                        }
                    }
                }
            }
            break
        }
    }
}

if (-not $msbuildPath) {
    $msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($msbuildCommand) {
        $msbuildPath = $msbuildCommand.Source
    }
}

if (-not $msbuildPath) {
    throw 'MSBuild was not found. Install Visual Studio Build Tools or add MSBuild to PATH.'
}

if (-not $PlatformToolset) {
    $vcToolsRoot = Join-Path (Split-Path (Split-Path $msbuildPath -Parent) -Parent) 'VC\Tools\MSVC'
    if (Test-Path $vcToolsRoot) {
        $toolsetDirs = Get-ChildItem -Path $vcToolsRoot -Directory |
            Sort-Object { [version]($_.Name) } -Descending
        foreach ($toolsetDir in $toolsetDirs) {
            foreach ($versionPrefix in $toolsetMap.Keys | Sort-Object { [version]$_ } -Descending) {
                if ($toolsetDir.Name.StartsWith($versionPrefix)) {
                    $PlatformToolset = $toolsetMap[$versionPrefix]
                    break
                }
            }
            if ($PlatformToolset) {
                break
            }
        }
    }
}

if (-not $PlatformToolset) {
    throw 'No compatible C++ toolset was found. Install the Desktop development with C++ workload in Visual Studio Build Tools.'
}

$vcVarsBat = if ($Platform -eq 'x64') {
    Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
} else {
    Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat'
}
if (-not (Test-Path $vcVarsBat)) {
    $vcVarsBat = Join-Path (Split-Path (Split-Path $msbuildPath -Parent) -Parent) 'VC\Auxiliary\Build\vcvars64.bat'
    if ($Platform -eq 'Win32') {
        $vcVarsBat = Join-Path (Split-Path (Split-Path $msbuildPath -Parent) -Parent) 'VC\Auxiliary\Build\vcvars32.bat'
    }
}
if (-not (Test-Path $vcVarsBat)) {
    throw "Developer environment batch file not found: $vcVarsBat"
}

cmd /c "call ""$vcVarsBat"" && ""$msbuildPath"" ""$solutionPath"" /t:Build /p:Configuration=$Configuration /p:Platform=$Platform /p:PlatformToolset=$PlatformToolset /nologo"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$outputPath = Join-Path $PSScriptRoot "bin\$Platform\$Configuration"
Copy-Item -Path (Join-Path $PSScriptRoot 'install.ps1') -Destination $outputPath -Force
Copy-Item -Path (Join-Path $PSScriptRoot 'uninstall.ps1') -Destination $outputPath -Force