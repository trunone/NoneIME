// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "TableDictionaryEngine.h"
#include "DictionarySearch.h"

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
    if (originalCandidates.Count() > 0)
    {
        CCandidateListItem* pOriginal = originalCandidates.GetAt(0);
        CCandidateListItem* pLI = pItemList->Append();
        if (pLI)
        {
            pLI->_ItemString.Set(pOriginal->_ItemString);
            pLI->_FindKeyCode.Set(pOriginal->_FindKeyCode);
            pLI->_ShowFindKeyCode = TRUE;
        }
    }

    CDictionaryResult* pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pHomophoneDictionaryFile, pKeyCode);

    while (dshSearch.FindPhrase(&pdret))
    {
        for (UINT iIndex = 0; iIndex < pdret->_FindPhraseList.Count(); iIndex++)
        {
            CStringRange* pHomophones = pdret->_FindPhraseList.GetAt(iIndex);
            CDictionaryResult* pSharedResult = nullptr;
            if (pHomophones->GetLength() == 8 && pHomophones->Get()[0] == L'@' && pHomophones->Get()[1] == L'H')
            {
                CDictionarySearch sharedSearch(_locale, _pHomophoneDictionaryFile, pHomophones);
                if (sharedSearch.FindPhrase(&pSharedResult) && pSharedResult && pSharedResult->_FindPhraseList.Count() > 0)
                {
                    pHomophones = pSharedResult->_FindPhraseList.GetAt(0);
                }
            }

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

            delete pSharedResult;
        }

        delete pdret;
        pdret = nullptr;
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

