param(
    [Parameter(Mandatory = $true)]
    [string] $MoeDictionaryWorkbookPath
)

$ErrorActionPreference = 'Stop'

$dictionaryPath = Join-Path $PSScriptRoot 'Boshiamy.cin'
$outputPath = Join-Path $PSScriptRoot 'Boshiamy-Homophones.txt'
$dictionaryLines = [System.IO.File]::ReadAllLines($dictionaryPath, [System.Text.Encoding]::UTF8)
$tableStart = [Array]::IndexOf($dictionaryLines, 'BEGIN_TABLE')
if ($tableStart -lt 0) {
    throw 'The CIN dictionary does not contain BEGIN_TABLE.'
}

$tableCharacterSet = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::Ordinal)
$codes = New-Object 'System.Collections.Generic.List[string]'
$firstCharacterByCode = @{}
$orderedTableCharacters = New-Object 'System.Collections.Generic.List[string]'
for ($lineIndex = $tableStart + 1; $lineIndex -lt $dictionaryLines.Length; $lineIndex++) {
    $line = $dictionaryLines[$lineIndex]
    $line = $line.Trim()
    if ($line -eq 'END_TABLE') {
        break
    }
    if (-not $line -or $line.StartsWith('#')) {
        continue
    }
    $fields = $line -split '\s+'
    if ($fields.Count -lt 2) {
        throw "Invalid CIN table record at source line $($lineIndex + 1)."
    }
    $code = $fields[0]
    $value = $fields[1]

    if (-not $firstCharacterByCode.ContainsKey($code)) {
        $codes.Add($code)
        $firstCharacterByCode[$code] = $value
    }

    if ($value.Length -eq 1 -and $tableCharacterSet.Add($value)) {
        $orderedTableCharacters.Add($value)
    }
}

$previousCharactersByCode = @{}
if (Test-Path $outputPath) {
    $previousLines = [System.IO.File]::ReadAllLines($outputPath, [System.Text.Encoding]::Unicode)
    $previousSharedLists = @{}
    foreach ($line in $previousLines) {
        if ($line -match '^(@H\d+)=(.*)$') {
            $previousSharedLists[$matches[1]] = $matches[2]
        }
    }
    foreach ($line in $previousLines) {
        if ($line -match '^@H') {
            continue
        }
        $separatorIndex = $line.IndexOf('=')
        if ($separatorIndex -le 0) {
            continue
        }
        $code = $line.Substring(0, $separatorIndex)
        $value = $line.Substring($separatorIndex + 1)
        if ($previousSharedLists.ContainsKey($value)) {
            $value = $previousSharedLists[$value]
        }
        $previousCharactersByCode[$code] = New-Object 'System.Collections.Generic.List[string]'
        foreach ($character in $value.ToCharArray()) {
            $candidate = [string] $character
            if (-not [char]::IsWhiteSpace($character) -and -not $previousCharactersByCode[$code].Contains($candidate)) {
                $previousCharactersByCode[$code].Add($candidate)
            }
        }
    }
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$workbookArchive = [System.IO.Compression.ZipFile]::OpenRead($MoeDictionaryWorkbookPath)
try {
    $sharedStrings = New-Object 'System.Collections.Generic.List[string]'
    $sharedStringsReader = [System.Xml.XmlReader]::Create($workbookArchive.GetEntry('xl/sharedStrings.xml').Open())
    while ($sharedStringsReader.Read()) {
        if ($sharedStringsReader.NodeType -eq [System.Xml.XmlNodeType]::Element -and $sharedStringsReader.LocalName -eq 'si') {
            $itemReader = $sharedStringsReader.ReadSubtree()
            $text = New-Object System.Text.StringBuilder
            while ($itemReader.Read()) {
                if ($itemReader.NodeType -eq [System.Xml.XmlNodeType]::Element -and $itemReader.LocalName -eq 't') {
                    [void] $text.Append($itemReader.ReadElementContentAsString())
                }
            }
            $sharedStrings.Add($text.ToString())
            $itemReader.Dispose()
        }
    }
    $sharedStringsReader.Dispose()

    $primaryReadingByCharacter = @{}
    $readingsByCharacter = @{}
    $sheetReader = [System.Xml.XmlReader]::Create($workbookArchive.GetEntry('xl/worksheets/sheet1.xml').Open())
    while ($sheetReader.ReadToFollowing('row')) {
        $rowReader = $sheetReader.ReadSubtree()
        $values = @{}
        while ($rowReader.Read()) {
            if ($rowReader.NodeType -ne [System.Xml.XmlNodeType]::Element -or $rowReader.LocalName -ne 'c') {
                continue
            }
            $reference = $rowReader.GetAttribute('r')
            $column = $reference -replace '\d', ''
            if ($column -notin @('A', 'C', 'I')) {
                continue
            }

            $cellType = $rowReader.GetAttribute('t')
            $cellReader = $rowReader.ReadSubtree()
            $rawValue = $null
            while ($cellReader.Read()) {
                if ($cellReader.NodeType -eq [System.Xml.XmlNodeType]::Element -and $cellReader.LocalName -eq 'v') {
                    $rawValue = $cellReader.ReadElementContentAsString()
                    break
                }
            }
            $cellReader.Dispose()
            if ($null -ne $rawValue) {
                if ($cellType -eq 's') {
                    $rawValue = $sharedStrings[[int] $rawValue]
                }
                $values[$column] = $rawValue
            }
        }
        $rowReader.Dispose()

        if ($values['C'] -ne '1' -or -not $values['A'] -or $values['A'].Length -ne 1 -or -not $values['I']) {
            continue
        }
        $character = $values['A']
        $reading = $values['I']
        if (-not $tableCharacterSet.Contains($character)) {
            continue
        }
        if (-not $primaryReadingByCharacter.ContainsKey($character)) {
            $primaryReadingByCharacter[$character] = $reading
            $readingsByCharacter[$character] = New-Object 'System.Collections.Generic.List[string]'
        }
        if (-not $readingsByCharacter[$character].Contains($reading)) {
            $readingsByCharacter[$character].Add($reading)
        }
    }
    $sheetReader.Dispose()
}
finally {
    $workbookArchive.Dispose()
}

$charactersByReading = @{}
foreach ($character in $orderedTableCharacters) {
    if (-not $readingsByCharacter.ContainsKey($character)) {
        continue
    }
    foreach ($reading in $readingsByCharacter[$character]) {
        if (-not $charactersByReading.ContainsKey($reading)) {
            $charactersByReading[$reading] = New-Object 'System.Collections.Generic.List[string]'
        }
        $charactersByReading[$reading].Add($character)
    }
}

$records = New-Object 'System.Collections.Generic.List[object]'
foreach ($code in $codes) {
    $firstCharacter = $firstCharacterByCode[$code]
    if ($firstCharacter.Length -ne 1 -or -not $primaryReadingByCharacter.ContainsKey($firstCharacter)) {
        continue
    }

    $reading = $primaryReadingByCharacter[$firstCharacter]
    if (-not $charactersByReading.ContainsKey($reading)) {
        continue
    }

    $homophones = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::Ordinal)
    foreach ($homophone in $charactersByReading[$reading]) {
        $homophones.Add($homophone) | Out-Null
    }
    $orderedHomophones = New-Object 'System.Collections.Generic.List[string]'
    if ($previousCharactersByCode.ContainsKey($code)) {
        foreach ($homophone in $previousCharactersByCode[$code]) {
            if ($homophones.Contains($homophone) -and -not $orderedHomophones.Contains($homophone)) {
                $orderedHomophones.Add($homophone)
            }
        }
    }
    foreach ($homophone in $charactersByReading[$reading]) {
        if (-not $orderedHomophones.Contains($homophone)) {
            $orderedHomophones.Add($homophone)
        }
    }

    $records.Add([PSCustomObject]@{
        Code = $code
        Homophones = [string]::Concat($orderedHomophones)
    })
}

$recordCounts = New-Object 'System.Collections.Generic.Dictionary[string, int]' ([System.StringComparer]::Ordinal)
foreach ($record in $records) {
    if (-not $recordCounts.ContainsKey($record.Homophones)) {
        $recordCounts[$record.Homophones] = 0
    }
    $recordCounts[$record.Homophones]++
}

$sharedIds = New-Object 'System.Collections.Generic.Dictionary[string, string]' ([System.StringComparer]::Ordinal)
$sharedRecords = New-Object 'System.Collections.Generic.List[string]'
$outputRecords = New-Object 'System.Collections.Generic.List[string]'
foreach ($record in $records) {
    if ($recordCounts[$record.Homophones] -gt 1) {
        if (-not $sharedIds.ContainsKey($record.Homophones)) {
            $reference = '@H{0:D6}' -f $sharedRecords.Count
            $sharedIds[$record.Homophones] = $reference
            $sharedRecords.Add("$reference=$($record.Homophones)")
        }
        $outputRecords.Add("$($record.Code)=$($sharedIds[$record.Homophones])")
    }
    else {
        $outputRecords.Add("$($record.Code)=$($record.Homophones)")
    }
}

$outputRecords.InsertRange(0, $sharedRecords)
$contents = [System.String]::Join("`r`n", $outputRecords) + "`r`n"
[System.IO.File]::WriteAllText($outputPath, $contents, [System.Text.Encoding]::Unicode)
Write-Output "Wrote $($records.Count) Mandarin homophone records and $($sharedRecords.Count) shared lists to $outputPath"