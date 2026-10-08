param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [ValidateSet('Win32', 'x64')]
    [string]$Platform = 'x64',
    [string]$DllPath
)

$ErrorActionPreference = 'Stop'

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $powershellExecutable = if ($PSVersionTable.PSEdition -eq 'Core') { 'pwsh.exe' } else { 'powershell.exe' }
    $powershellPath = Join-Path $PSHOME $powershellExecutable
    $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Configuration $Configuration -Platform $Platform"
    if ($DllPath) {
        $arguments += " -DllPath `"$DllPath`""
    }
    $process = Start-Process -FilePath $powershellPath -ArgumentList $arguments -Verb RunAs -Wait -PassThru
    exit $process.ExitCode
}

if (-not $DllPath) {
    $localDllPath = Join-Path $PSScriptRoot 'NoneIME.dll'
    $DllPath = if (Test-Path -LiteralPath $localDllPath -PathType Leaf) {
        $localDllPath
    } else {
        Join-Path $PSScriptRoot "bin\$Platform\$Configuration\NoneIME.dll"
    }
}
$DllPath = [System.IO.Path]::GetFullPath($DllPath)

if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) {
    throw "IME DLL not found: $DllPath. Build it first or provide -DllPath."
}

$dictionaryDirectory = Split-Path -Parent $DllPath
$iniPath = Join-Path $dictionaryDirectory 'NoneIME.ini'
$cinFileName = 'Boshiamy.cin'
if (Test-Path -LiteralPath $iniPath -PathType Leaf) {
    $section = ''
    foreach ($line in [System.IO.File]::ReadAllLines($iniPath, [System.Text.Encoding]::UTF8)) {
        if ($line -match '^\s*\[([^\]]+)\]\s*$') {
            $section = $matches[1]
            continue
        }
        if ($section -ieq 'Dictionary' -and $line -match '^\s*CinPath\s*=\s*(.*?)\s*$') {
            if ($matches[1]) {
                $cinFileName = $matches[1]
            }
            break
        }
    }
}
$cinDictionaryPath = if ([System.IO.Path]::IsPathRooted($cinFileName)) {
    $cinFileName
} else {
    Join-Path $dictionaryDirectory $cinFileName
}
$cinDictionaryPath = [System.IO.Path]::GetFullPath($cinDictionaryPath)
if (-not (Test-Path -LiteralPath $cinDictionaryPath -PathType Leaf)) {
    throw "Configured CIN dictionary not found: $cinDictionaryPath"
}

if ($Platform -eq 'x64') {
    if (-not [Environment]::Is64BitOperatingSystem) {
        throw 'The x64 IME cannot be installed on a 32-bit version of Windows.'
    }
    if ([Environment]::Is64BitProcess) {
        $regsvr32Path = Join-Path $env:WINDIR 'System32\regsvr32.exe'
    }
    else {
        $regsvr32Path = Join-Path $env:WINDIR 'Sysnative\regsvr32.exe'
    }
}
elseif ([Environment]::Is64BitOperatingSystem) {
    $regsvr32Path = Join-Path $env:WINDIR 'SysWOW64\regsvr32.exe'
}
else {
    $regsvr32Path = Join-Path $env:WINDIR 'System32\regsvr32.exe'
}

& $regsvr32Path $DllPath
if ($LASTEXITCODE -ne 0) {
    throw "IME registration failed with exit code $LASTEXITCODE. Try running PowerShell as administrator."
}

Write-Output "Installed None IME from $DllPath"