// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

#include "BaseDictionaryEngine.h"
#include <cstdint>

#pragma pack(push, 1)
struct DictHeader
{
    char magic[4];          // "NIME"
    uint32_t version;       // 1
    uint32_t indexCount;    // Number of DictIndexEntry items
    uint32_t phraseCount;   // Number of wchar_t phrases
};

struct DictIndexEntry
{
    char key[6];            // Null-terminated key string (max length 5)
    uint16_t phraseOffset;  // Start index in phrase array
    uint16_t phraseCount;   // Number of candidate phrases for this key
};
#pragma pack(pop)

class CTableDictionaryEngine : public CBaseDictionaryEngine
{
public:
    CTableDictionaryEngine(LCID locale);
    virtual ~CTableDictionaryEngine() { }

    VOID CollectWord(_In_ CStringRange *pKeyCode, _Inout_ CNoneImeArray<CStringRange> *pWordStrings);
    VOID CollectWord(_In_ CStringRange *pKeyCode, _Inout_ CNoneImeArray<CCandidateListItem> *pItemList);

private:
    BOOL EnsureDictionaryLoaded();

    const DictHeader* _pHeader;
    const DictIndexEntry* _pIndex;
    const wchar_t* _pPhrases;
};
