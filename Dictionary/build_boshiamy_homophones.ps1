param(
    [Parameter(Mandatory = $true)]
    [string] $MoeDictionaryWorkbookPath
)

$ErrorActionPreference = 'Stop'

$dictionaryPath = Join-Path $PSScriptRoot 'Boshiamy.cin'
$binaryOutputPath = Join-Path $PSScriptRoot 'Boshiamy-Homophones.bin'
$dictionaryLines = [System.IO.File]::ReadAllLines($dictionaryPath, [System.Text.Encoding]::UTF8)
$tableStart = [Array]::IndexOf($dictionaryLines, 'BEGIN_TABLE')
if ($tableStart -lt 0) {
    throw 'The CIN dictionary does not contain BEGIN_TABLE.'
}

$tableCharacterSet = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::Ordinal)
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
    $value = $fields[1]

    if ($value.Length -eq 1 -and $tableCharacterSet.Add($value)) {
        $orderedTableCharacters.Add($value)
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

$poolStream = [System.IO.MemoryStream]::new()
$poolWriter = [System.IO.BinaryWriter]::new($poolStream, [System.Text.Encoding]::Unicode, $true)
$poolOffsets = New-Object 'System.Collections.Generic.Dictionary[string, uint32]' ([System.StringComparer]::Ordinal)
$addPoolString = {
    param([string] $value)
    if ($poolOffsets.ContainsKey($value)) {
        return $poolOffsets[$value]
    }
    if ($poolStream.Position -gt [uint32]::MaxValue) {
        throw 'The homophone string pool exceeds the 32-bit offset limit.'
    }
    $offset = [uint32] $poolStream.Position
    $poolWriter.Write([System.Text.Encoding]::Unicode.GetBytes($value))
    $poolWriter.Write([uint16] 0)
    $poolOffsets.Add($value, $offset)
    return $offset
}

$binaryReadingRecords = New-Object 'System.Collections.Generic.List[object]'
$sortedReadings = [string[]] @($charactersByReading.Keys)
[Array]::Sort($sortedReadings, [System.StringComparer]::Ordinal)
foreach ($reading in $sortedReadings) {
    $homophoneList = [string]::Concat($charactersByReading[$reading])
    if ($homophoneList.Length -gt [uint16]::MaxValue) {
        throw "Homophone list for '$reading' exceeds the 16-bit length limit."
    }
    $binaryReadingRecords.Add([PSCustomObject]@{
        Reading = $reading
        ReadingOffset = [uint32] (& $addPoolString $reading)
        ListOffset = [uint32] (& $addPoolString $homophoneList)
        ListLength = [uint16] $homophoneList.Length
    })
}

$binaryCharacterRecords = New-Object 'System.Collections.Generic.List[object]'
foreach ($character in $orderedTableCharacters) {
    if ($primaryReadingByCharacter.ContainsKey($character)) {
        $reading = $primaryReadingByCharacter[$character]
        $binaryCharacterRecords.Add([PSCustomObject]@{
            Character = [uint16] [char] $character
            ReadingOffset = [uint32] (& $addPoolString $reading)
        })
    }
}
$sortedCharacters = @($binaryCharacterRecords | Sort-Object Character)
if ($poolStream.Length -gt [uint32]::MaxValue) {
    throw 'The homophone string pool exceeds the 32-bit size limit.'
}

$binaryStream = [System.IO.MemoryStream]::new()
$binaryWriter = [System.IO.BinaryWriter]::new($binaryStream, [System.Text.Encoding]::Unicode, $true)
$binaryWriter.Write([byte[]] @(0x48, 0x4F, 0x4D, 0x00))
$binaryWriter.Write([uint16] 1)
$binaryWriter.Write([uint32] $sortedCharacters.Count)
$binaryWriter.Write([uint32] $binaryReadingRecords.Count)
$binaryWriter.Write([uint32] $poolStream.Length)
$binaryWriter.Write($poolStream.ToArray())
foreach ($record in $sortedCharacters) {
    $binaryWriter.Write([uint16] $record.Character)
    $binaryWriter.Write([uint32] $record.ReadingOffset)
}
foreach ($record in $binaryReadingRecords) {
    $binaryWriter.Write([uint32] $record.ReadingOffset)
    $binaryWriter.Write([uint32] $record.ListOffset)
    $binaryWriter.Write([uint16] $record.ListLength)
}
[System.IO.File]::WriteAllBytes($binaryOutputPath, $binaryStream.ToArray())
$poolWriter.Dispose()
$poolStream.Dispose()
$binaryWriter.Dispose()
$binaryStream.Dispose()

Write-Output "Wrote $($binaryCharacterRecords.Count) character readings and $($binaryReadingRecords.Count) Zhuyin homophone lists to $binaryOutputPath"