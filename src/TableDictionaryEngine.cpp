// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "TableDictionaryEngine.h"
#include "DictionarySearch.h"
#include "FileMapping.h"

static UINT16 ReadUInt16LE(_In_ const BYTE *pData)
{
    return (UINT16)(pData[0] | ((UINT16)pData[1] << 8));
}

static UINT32 ReadUInt32LE(_In_ const BYTE *pData)
{
    return (UINT32)pData[0] | ((UINT32)pData[1] << 8) |
        ((UINT32)pData[2] << 16) | ((UINT32)pData[3] << 24);
}

static BOOL ComparePoolStrings(_In_reads_bytes_(poolSize) const BYTE *pData, ULONGLONG poolSize,
    UINT32 leftOffset, UINT32 rightOffset, _Out_ int *pComparison)
{
    ULONGLONG position = 0;
    if ((leftOffset & 1) || (rightOffset & 1) || leftOffset >= poolSize || rightOffset >= poolSize)
    {
        return FALSE;
    }

    while ((ULONGLONG)leftOffset + position + sizeof(UINT16) <= poolSize &&
        (ULONGLONG)rightOffset + position + sizeof(UINT16) <= poolSize)
    {
        UINT16 leftCharacter = ReadUInt16LE(pData + leftOffset + position);
        UINT16 rightCharacter = ReadUInt16LE(pData + rightOffset + position);
        if (leftCharacter != rightCharacter)
        {
            *pComparison = leftCharacter < rightCharacter ? -1 : 1;
            return TRUE;
        }
        if (leftCharacter == 0)
        {
            *pComparison = 0;
            return TRUE;
        }
        position += sizeof(UINT16);
    }
    return FALSE;
}

static BOOL FindBinaryHomophoneList(_In_ CFileMapping *pFile, _In_ const CStringRange *pCharacter,
    _Out_ CStringRange *pHomophones)
{
    const BYTE *pData = pFile->GetRawData();
    const DWORD_PTR fileSize = pFile->GetFileSize();
    const ULONGLONG headerSize = 18;
    if (!pData || fileSize < headerSize || pData[0] != 'H' || pData[1] != 'O' ||
        pData[2] != 'M' || pData[3] != 0 || ReadUInt16LE(pData + 4) != 1 || pCharacter->GetLength() != 1)
    {
        return FALSE;
    }

    const UINT32 characterCount = ReadUInt32LE(pData + 6);
    const UINT32 zhuyinCount = ReadUInt32LE(pData + 10);
    const UINT32 poolSize = ReadUInt32LE(pData + 14);
    const ULONGLONG poolStart = headerSize;
    const ULONGLONG characterTable = poolStart + poolSize;
    const ULONGLONG zhuyinTable = characterTable + (ULONGLONG)characterCount * 6;
    const ULONGLONG fileEnd = zhuyinTable + (ULONGLONG)zhuyinCount * 10;
    if (fileEnd > fileSize)
    {
        return FALSE;
    }

    UINT32 low = 0;
    UINT32 high = characterCount;
    const UINT16 targetCharacter = (UINT16)pCharacter->Get()[0];
    while (low < high)
    {
        UINT32 middle = low + (high - low) / 2;
        UINT16 character = ReadUInt16LE(pData + characterTable + (ULONGLONG)middle * 6);
        if (character < targetCharacter)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    if (low >= characterCount)
    {
        return FALSE;
    }

    const BYTE *pCharacterRecord = pData + characterTable + (ULONGLONG)low * 6;
    if (ReadUInt16LE(pCharacterRecord) != targetCharacter)
    {
        return FALSE;
    }
    const UINT32 targetZhuyinOffset = ReadUInt32LE(pCharacterRecord + 2);
    low = 0;
    high = zhuyinCount;
    while (low < high)
    {
        UINT32 middle = low + (high - low) / 2;
        const BYTE *pRecord = pData + zhuyinTable + (ULONGLONG)middle * 10;
        int comparison = 0;
        if (!ComparePoolStrings(pData + poolStart, poolSize, ReadUInt32LE(pRecord), targetZhuyinOffset, &comparison))
        {
            return FALSE;
        }
        if (comparison < 0)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    if (low >= zhuyinCount)
    {
        return FALSE;
    }

    const BYTE *pZhuyinRecord = pData + zhuyinTable + (ULONGLONG)low * 10;
    int comparison = 0;
    if (!ComparePoolStrings(pData + poolStart, poolSize, ReadUInt32LE(pZhuyinRecord), targetZhuyinOffset, &comparison) || comparison != 0)
    {
        return FALSE;
    }

    const UINT32 listOffset = ReadUInt32LE(pZhuyinRecord + 4);
    const UINT16 listLength = ReadUInt16LE(pZhuyinRecord + 8);
    const ULONGLONG listEnd = (ULONGLONG)listOffset + (ULONGLONG)listLength * sizeof(WCHAR);
    if ((listOffset & 1) || listEnd + sizeof(WCHAR) > poolSize ||
        ReadUInt16LE(pData + poolStart + listEnd) != 0)
    {
        return FALSE;
    }

    pHomophones->Set((const WCHAR *)(pData + poolStart + listOffset), listLength);
    return TRUE;
}

//+---------------------------------------------------------------------------
//
// CollectWord
//
//----------------------------------------------------------------------------

VOID CTableDictionaryEngine::CollectWord(_In_ CStringRange *pKeyCode, _Inout_ CNoneImeArray<CStringRange> *pWordStrings)
{
    CDictionaryResult* pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pKeyCode);

    while (dshSearch.FindPhrase(&pdret))
    {
        for (UINT index = 0; index < pdret->_FindPhraseList.Count(); index++)
        {
            CStringRange* pPhrase = nullptr;
            pPhrase = pWordStrings->Append();
            if (pPhrase)
            {
                *pPhrase = *pdret->_FindPhraseList.GetAt(index);
            }
        }

        delete pdret;
        pdret = nullptr;
    }
}

VOID CTableDictionaryEngine::CollectWord(_In_ CStringRange *pKeyCode, _Inout_ CNoneImeArray<CCandidateListItem> *pItemList)
{
    CDictionaryResult* pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pKeyCode);

    while (dshSearch.FindPhrase(&pdret))
    {
        for (UINT iIndex = 0; iIndex < pdret->_FindPhraseList.Count(); iIndex++)
        {
            CCandidateListItem* pLI = nullptr;
            pLI = pItemList->Append();
            if (pLI)
            {
                pLI->_ItemString.Set(*pdret->_FindPhraseList.GetAt(iIndex));
                pLI->_FindKeyCode.Set(pdret->_FindKeyCode.Get(), pdret->_FindKeyCode.GetLength());
            }
        }

        delete pdret;
        pdret = nullptr;
    }
}

VOID CTableDictionaryEngine::CollectHomophones(_In_ CStringRange *pKeyCode, _Inout_ CNoneImeArray<CCandidateListItem> *pItemList)
{
    if (!_pHomophoneDictionaryFile)
    {
        return;
    }

    CNoneImeArray<CCandidateListItem> originalCandidates;
    CollectWord(pKeyCode, &originalCandidates);
    CCandidateListItem *pOriginal = nullptr;
    if (originalCandidates.Count() > 0)
    {
        pOriginal = originalCandidates.GetAt(0);
        CCandidateListItem* pLI = pItemList->Append();
        if (pLI)
        {
            pLI->_ItemString.Set(pOriginal->_ItemString);
            pLI->_FindKeyCode.Set(pOriginal->_FindKeyCode);
            pLI->_ShowFindKeyCode = TRUE;
        }
    }

    CFileMapping *pHomophoneFile = _pHomophoneDictionaryFile;
    const BYTE *pHomophoneData = pHomophoneFile->GetRawData();
    if (pHomophoneData && pHomophoneFile->GetFileSize() >= 4 && pHomophoneData[0] == 'H' &&
        pHomophoneData[1] == 'O' && pHomophoneData[2] == 'M' && pHomophoneData[3] == 0)
    {
        CStringRange homophones;
        if (pOriginal && FindBinaryHomophoneList(pHomophoneFile, &pOriginal->_ItemString, &homophones))
        {
            CollectHomophonesFromList(&homophones, pItemList);
        }
        return;
    }

    CStringRange homophones;
    if (pOriginal && FindBinaryHomophoneList(_pHomophoneDictionaryFile, &pOriginal->_ItemString, &homophones))
    {
        CollectHomophonesFromList(&homophones, pItemList);
    }
}

VOID CTableDictionaryEngine::CollectHomophonesFromList(_In_ CStringRange *pHomophones, _Inout_ CNoneImeArray<CCandidateListItem> *pItemList)
{
    for (DWORD_PTR characterIndex = 0; characterIndex < pHomophones->GetLength(); characterIndex++)
    {
        CStringRange homophone;
        homophone.Set(pHomophones->Get() + characterIndex, 1);
        BOOL isDuplicate = FALSE;
        for (UINT candidateIndex = 0; candidateIndex < pItemList->Count(); candidateIndex++)
        {
            CCandidateListItem* pCandidate = pItemList->GetAt(candidateIndex);
            if (CStringRange::Compare(_locale, &pCandidate->_ItemString, &homophone) == CSTR_EQUAL)
            {
                isDuplicate = TRUE;
                break;
            }
        }

        if (!isDuplicate)
        {
            CCandidateListItem* pLI = pItemList->Append();
            if (pLI)
            {
                pLI->_ItemString.Set(homophone);
                pLI->_ShowFindKeyCode = TRUE;

                WCHAR homophonePattern[] = { L'*', *homophone.Get(), L'*', L'\0' };
                CStringRange homophoneSearch;
                homophoneSearch.Set(homophonePattern, ARRAYSIZE(homophonePattern) - 1);
                CDictionarySearch codeSearch(_locale, _pDictionaryFile, &homophoneSearch);
                CDictionaryResult* pCodeResult = nullptr;
                if (codeSearch.FindConvertedStringForWildcard(&pCodeResult) && pCodeResult)
                {
                    pLI->_FindKeyCode.Set(pCodeResult->_FindKeyCode);
                }
                delete pCodeResult;
            }
        }
    }
}

//+---------------------------------------------------------------------------
//
// CollectWordForWildcard
//
//----------------------------------------------------------------------------

VOID CTableDictionaryEngine::CollectWordForWildcard(_In_ CStringRange *pKeyCode, _Inout_ CNoneImeArray<CCandidateListItem> *pItemList)
{
    CDictionaryResult* pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pKeyCode);

    while (dshSearch.FindPhraseForWildcard(&pdret))
    {
        for (UINT iIndex = 0; iIndex < pdret->_FindPhraseList.Count(); iIndex++)
        {
            CCandidateListItem* pLI = nullptr;
            pLI = pItemList->Append();
            if (pLI)
            {
                pLI->_ItemString.Set(*pdret->_FindPhraseList.GetAt(iIndex));
                pLI->_FindKeyCode.Set(pdret->_FindKeyCode.Get(), pdret->_FindKeyCode.GetLength());
            }
        }

        delete pdret;
        pdret = nullptr;
    }
}

//+---------------------------------------------------------------------------
//
// CollectWordFromConvertedStringForWildcard
//
//----------------------------------------------------------------------------

VOID CTableDictionaryEngine::CollectWordFromConvertedStringForWildcard(_In_ CStringRange *pString, _Inout_ CNoneImeArray<CCandidateListItem> *pItemList)
{
    CDictionaryResult* pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pString);

    while (dshSearch.FindConvertedStringForWildcard(&pdret)) // TAIL ALL CHAR MATCH
    {
        for (UINT index = 0; index < pdret->_FindPhraseList.Count(); index++)
        {
            CCandidateListItem* pLI = nullptr;
            pLI = pItemList->Append();
            if (pLI)
            {
                pLI->_ItemString.Set(*pdret->_FindPhraseList.GetAt(index));
                pLI->_FindKeyCode.Set(pdret->_FindKeyCode.Get(), pdret->_FindKeyCode.GetLength());
            }
        }

        delete pdret;
        pdret = nullptr;
    }
}

