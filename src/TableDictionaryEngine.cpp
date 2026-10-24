// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "TableDictionaryEngine.h"
#include "Globals.h"
#include "resource.h"

CTableDictionaryEngine::CTableDictionaryEngine(LCID locale) : CBaseDictionaryEngine(locale)
{
    _pHeader = nullptr;
    _pIndex = nullptr;
    _pPhrases = nullptr;
}

BOOL CTableDictionaryEngine::EnsureDictionaryLoaded()
{
    if (_pHeader != nullptr)
    {
        return TRUE;
    }

    HMODULE hModule = Global::dllInstanceHandle;
    HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(IDR_DICTIONARY_DATA), RT_RCDATA);
    if (!hRes)
    {
        return FALSE;
    }

    HGLOBAL hMem = LoadResource(hModule, hRes);
    if (!hMem)
    {
        return FALSE;
    }

    const BYTE* pData = reinterpret_cast<const BYTE*>(LockResource(hMem));
    if (!pData)
    {
        return FALSE;
    }

    DWORD dwSize = SizeofResource(hModule, hRes);
    if (dwSize < sizeof(DictHeader))
    {
        return FALSE;
    }

    const DictHeader* pHeader = reinterpret_cast<const DictHeader*>(pData);
    if (memcmp(pHeader->magic, "NIME", 4) != 0 || pHeader->version != 1)
    {
        return FALSE;
    }

    DWORD requiredSize = sizeof(DictHeader) +
        pHeader->indexCount * sizeof(DictIndexEntry) +
        pHeader->phraseCount * sizeof(wchar_t);
    if (dwSize < requiredSize)
    {
        return FALSE;
    }

    _pHeader = pHeader;
    _pIndex = reinterpret_cast<const DictIndexEntry*>(pData + sizeof(DictHeader));
    _pPhrases = reinterpret_cast<const wchar_t*>(pData + sizeof(DictHeader) + pHeader->indexCount * sizeof(DictIndexEntry));

    return TRUE;
}

VOID CTableDictionaryEngine::CollectWord(_In_ CStringRange *pKeyCode, _Inout_ CNoneImeArray<CStringRange> *pWordStrings)
{
    if (!pKeyCode || pKeyCode->GetLength() == 0 || pKeyCode->GetLength() >= 6)
    {
        return;
    }

    if (!EnsureDictionaryLoaded())
    {
        return;
    }

    char searchKey[6] = {0};
    for (DWORD_PTR i = 0; i < pKeyCode->GetLength(); ++i)
    {
        searchKey[i] = static_cast<char>(pKeyCode->Get()[i]);
    }
    searchKey[pKeyCode->GetLength()] = '\0';

    int low = 0;
    int high = static_cast<int>(_pHeader->indexCount) - 1;

    while (low <= high)
    {
        int mid = low + (high - low) / 2;
        int cmp = strcmp(searchKey, _pIndex[mid].key);
        if (cmp == 0)
        {
            uint16_t offset = _pIndex[mid].phraseOffset;
            uint16_t count = _pIndex[mid].phraseCount;
            for (uint16_t i = 0; i < count; ++i)
            {
                CStringRange* pPhrase = pWordStrings->Append();
                if (pPhrase)
                {
                    pPhrase->Set(&_pPhrases[offset + i], 1);
                }
            }
            break;
        }
        else if (cmp < 0)
        {
            high = mid - 1;
        }
        else
        {
            low = mid + 1;
        }
    }
}

VOID CTableDictionaryEngine::CollectWord(_In_ CStringRange *pKeyCode, _Inout_ CNoneImeArray<CCandidateListItem> *pItemList)
{
    if (!pKeyCode || pKeyCode->GetLength() == 0 || pKeyCode->GetLength() >= 6)
    {
        return;
    }

    if (!EnsureDictionaryLoaded())
    {
        return;
    }

    char searchKey[6] = {0};
    for (DWORD_PTR i = 0; i < pKeyCode->GetLength(); ++i)
    {
        searchKey[i] = static_cast<char>(pKeyCode->Get()[i]);
    }
    searchKey[pKeyCode->GetLength()] = '\0';

    int low = 0;
    int high = static_cast<int>(_pHeader->indexCount) - 1;

    while (low <= high)
    {
        int mid = low + (high - low) / 2;
        int cmp = strcmp(searchKey, _pIndex[mid].key);
        if (cmp == 0)
        {
            uint16_t offset = _pIndex[mid].phraseOffset;
            uint16_t count = _pIndex[mid].phraseCount;
            for (uint16_t i = 0; i < count; ++i)
            {
                CCandidateListItem* pLI = pItemList->Append();
                if (pLI)
                {
                    pLI->_ItemString.Set(&_pPhrases[offset + i], 1);
                    pLI->_FindKeyCode.Set(pKeyCode->Get(), pKeyCode->GetLength());
                }
            }
            break;
        }
        else if (cmp < 0)
        {
            high = mid - 1;
        }
        else
        {
            low = mid + 1;
        }
    }
}
