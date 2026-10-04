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

$dictionaryFiles = @('Boshiamy.txt', 'Boshiamy-Homophones.txt')
foreach ($dictionaryFile in $dictionaryFiles) {
    $dictionaryPath = Join-Path (Split-Path -Parent $DllPath) $dictionaryFile
    if (-not (Test-Path -LiteralPath $dictionaryPath -PathType Leaf)) {
        throw "Required dictionary not found beside the IME DLL: $dictionaryPath"
    }
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