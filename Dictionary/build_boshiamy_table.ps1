$ErrorActionPreference = 'Stop'

$sourcePath = Join-Path $PSScriptRoot 'Boshiamy-source.txt'
$outputPath = Join-Path $PSScriptRoot 'Boshiamy.txt'
$sourceLines = [System.IO.File]::ReadAllLines($sourcePath, [System.Text.Encoding]::UTF8)
$tableStart = [Array]::IndexOf($sourceLines, 'BEGIN_TABLE')

if ($tableStart -lt 0) {
    throw 'The source table does not contain BEGIN_TABLE.'
}

$records = New-Object 'System.Collections.Generic.List[string]'
for ($lineIndex = $tableStart + 1; $lineIndex -lt $sourceLines.Length; $lineIndex++) {
    $line = $sourceLines[$lineIndex].Trim()
    if (-not $line) {
        continue
    }
    if ($line.StartsWith('###')) {
        continue
    }

    $fields = $line -split '\s+'
    if ($fields.Count -ne 3 -or $fields[0] -notmatch "^[a-z,.'\[\];]{1,5}$" -or $fields[1].Contains('=')) {
        throw "Invalid Boshiamy table record at source line $($lineIndex + 1)."
    }

    $records.Add("$($fields[0])=$($fields[1])")
}

if ($records.Count -eq 0) {
    throw 'The source table contains no records.'
}

$contents = [System.String]::Join("`r`n", $records) + "`r`n"
[System.IO.File]::WriteAllText($outputPath, $contents, [System.Text.Encoding]::Unicode)
Write-Output "Wrote $($records.Count) UTF-16 dictionary records to $outputPath"