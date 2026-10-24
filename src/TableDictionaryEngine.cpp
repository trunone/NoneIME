// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "TableDictionaryEngine.h"
#include "EmbeddedDictionary.h"

VOID CTableDictionaryEngine::CollectWord(_In_ CStringRange *pKeyCode, _Inout_ CNoneImeArray<CStringRange> *pWordStrings)
{
    if (!pKeyCode || pKeyCode->GetLength() == 0 || pKeyCode->GetLength() >= 6)
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
    int high = static_cast<int>(g_dictIndexCount) - 1;

    while (low <= high)
    {
        int mid = low + (high - low) / 2;
        int cmp = strcmp(searchKey, g_dictIndex[mid].key);
        if (cmp == 0)
        {
            uint16_t offset = g_dictIndex[mid].phraseOffset;
            uint16_t count = g_dictIndex[mid].phraseCount;
            for (uint16_t i = 0; i < count; ++i)
            {
                CStringRange* pPhrase = pWordStrings->Append();
                if (pPhrase)
                {
                    pPhrase->Set(&g_dictPhrases[offset + i], 1);
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

    char searchKey[6] = {0};
    for (DWORD_PTR i = 0; i < pKeyCode->GetLength(); ++i)
    {
        searchKey[i] = static_cast<char>(pKeyCode->Get()[i]);
    }
    searchKey[pKeyCode->GetLength()] = '\0';

    int low = 0;
    int high = static_cast<int>(g_dictIndexCount) - 1;

    while (low <= high)
    {
        int mid = low + (high - low) / 2;
        int cmp = strcmp(searchKey, g_dictIndex[mid].key);
        if (cmp == 0)
        {
            uint16_t offset = g_dictIndex[mid].phraseOffset;
            uint16_t count = g_dictIndex[mid].phraseCount;
            for (uint16_t i = 0; i < count; ++i)
            {
                CCandidateListItem* pLI = pItemList->Append();
                if (pLI)
                {
                    pLI->_ItemString.Set(&g_dictPhrases[offset + i], 1);
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
