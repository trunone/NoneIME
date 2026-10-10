// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

#include "BaseDictionaryEngine.h"
#include "FileMapping.h"

class CTableDictionaryEngine : public CBaseDictionaryEngine
{
public:
    CTableDictionaryEngine(LCID locale, _In_ CFile *pDictionaryFile, _In_opt_ CFileMapping *pHomophoneDictionaryFile = nullptr) : CBaseDictionaryEngine(locale, pDictionaryFile), _pHomophoneDictionaryFile(pHomophoneDictionaryFile) { }
    virtual ~CTableDictionaryEngine() { }

    // Collect word from phrase string.
    // param
    //     [in] psrgKeyCode - Specified key code pointer
    //     [out] pasrgWordString - Specified returns pointer of word as CStringRange.
    // returns
    //     none.
    VOID CollectWord(_In_ CStringRange *pKeyCode, _Inout_ CNoneImeArray<CStringRange> *pWordStrings);
    VOID CollectWord(_In_ CStringRange *pKeyCode, _Inout_ CNoneImeArray<CCandidateListItem> *pItemList);
    VOID CollectHomophones(_In_ CStringRange *pKeyCode, _Inout_ CNoneImeArray<CCandidateListItem> *pItemList);

    VOID CollectWordForWildcard(_In_ CStringRange *psrgKeyCode, _Inout_ CNoneImeArray<CCandidateListItem> *pItemList);

    VOID CollectWordFromConvertedStringForWildcard(_In_ CStringRange *pString, _Inout_ CNoneImeArray<CCandidateListItem> *pItemList);

private:
    VOID CollectHomophonesFromList(_In_ CStringRange *pHomophones, _Inout_ CNoneImeArray<CCandidateListItem> *pItemList);

    CFileMapping* _pHomophoneDictionaryFile;
};
