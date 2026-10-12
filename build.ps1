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
$vsInstallPath = $null
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
            $vsInstallPath = $installPath
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

if (-not $vsInstallPath -and $msbuildPath) {
    $dir = Split-Path $msbuildPath -Parent
    while ($dir) {
        if (Test-Path (Join-Path $dir 'VC\Auxiliary\Build')) {
            $vsInstallPath = $dir
            break
        }
        $parentDir = Split-Path $dir -Parent
        if ($parentDir -eq $dir) { break }
        $dir = $parentDir
    }
}

if (-not $PlatformToolset) {
    $vcToolsRoot = if ($vsInstallPath) { Join-Path $vsInstallPath 'VC\Tools\MSVC' } else { $null }
    if ($vcToolsRoot -and (Test-Path $vcToolsRoot)) {
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

$vcvarsName = if ($Platform -eq 'x64') { 'vcvars64.bat' } else { 'vcvars32.bat' }
$vcVarsBat = $null

if ($vsInstallPath) {
    $candidate = Join-Path $vsInstallPath "VC\Auxiliary\Build\$vcvarsName"
    if (Test-Path $candidate) {
        $vcVarsBat = $candidate
    }
}

if (-not $vcVarsBat -and $msbuildPath) {
    $dir = Split-Path $msbuildPath -Parent
    while ($dir) {
        $candidate = Join-Path $dir "VC\Auxiliary\Build\$vcvarsName"
        if (Test-Path $candidate) {
            $vcVarsBat = $candidate
            break
        }
        $parentDir = Split-Path $dir -Parent
        if ($parentDir -eq $dir) { break }
        $dir = $parentDir
    }
}

if (-not $vcVarsBat) {
    $programFilesDirs = @(
        ${env:ProgramFiles},
        ${env:ProgramFiles(x86)}
    ) | Where-Object { $_ -and (Test-Path $_) }

    foreach ($pf in $programFilesDirs) {
        $vsRoot = Join-Path $pf 'Microsoft Visual Studio'
        if (Test-Path $vsRoot) {
            $found = Get-ChildItem -Path $vsRoot -Filter $vcvarsName -Recurse -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if ($found) {
                $vcVarsBat = $found.FullName
                break
            }
        }
    }
}

if (-not $vcVarsBat -or -not (Test-Path $vcVarsBat)) {
    throw "Developer environment batch file ($vcvarsName) not found."
}

cmd /c "call ""$vcVarsBat"" && ""$msbuildPath"" ""$solutionPath"" /t:Build /p:Configuration=$Configuration /p:Platform=$Platform /p:PlatformToolset=$PlatformToolset /nologo"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$outputPath = Join-Path $PSScriptRoot "bin\$Platform\$Configuration"
Copy-Item -Path (Join-Path $PSScriptRoot 'install.ps1') -Destination $outputPath -Force
Copy-Item -Path (Join-Path $PSScriptRoot 'uninstall.ps1') -Destination $outputPath -Force
