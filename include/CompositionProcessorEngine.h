// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

#include "sal.h"
#include "TableDictionaryEngine.h"
#include "KeyHandlerEditSession.h"
#include "NoneIMEBaseStructure.h"
#include "FileMapping.h"
#include "define.h"

class CCompositionProcessorEngine
{
public:
    CCompositionProcessorEngine(void);
    ~CCompositionProcessorEngine(void);

    BOOL SetupLanguageProfile(LANGID langid, REFGUID guidLanguageProfile, _In_ ITfThreadMgr *pThreadMgr, TfClientId tfClientId, BOOL isSecureMode, BOOL isComLessMode);

    GUID GetLanguageProfile(LANGID *plangid)
    {
        *plangid = _langid;
        return _guidProfile;
    }
    LCID GetLocale()
    {
        return MAKELCID(_langid, SORT_DEFAULT);
    }

    BOOL IsVirtualKeyNeed(UINT uCode, _In_reads_(1) WCHAR *pwch, BOOL fComposing, CANDIDATE_MODE candidateMode, BOOL hasCandidateWithWildcard, _Out_opt_ _KEYSTROKE_STATE *pKeyState);

    BOOL AddVirtualKey(WCHAR wch);
    void RemoveVirtualKey(DWORD_PTR dwIndex);
    void PurgeVirtualKey();

    DWORD_PTR GetVirtualKeyLength() { return _keystrokeBuffer.GetLength(); }
    WCHAR GetVirtualKey(DWORD_PTR dwIndex);

    void GetReadingStrings(_Inout_ CNoneImeArray<CStringRange> *pReadingStrings, _Out_ BOOL *pIsWildcardIncluded);
    void GetCandidateList(_Inout_ CNoneImeArray<CCandidateListItem> *pCandidateList, BOOL isIncrementalWordSearch, BOOL isWildcardSearch);
    void GetCandidateStringInConverted(CStringRange &searchString, _In_ CNoneImeArray<CCandidateListItem> *pCandidateList);

    BOOL IsDictionaryAvailable() { return (_pTableDictionaryEngine ? TRUE : FALSE); }

    inline CCandidateRange *GetCandidateListIndexRange() { return &_candidateListIndexRange; }

private:
    void InitKeyStrokeTable();

    struct _KEYSTROKE
    {
        UINT VirtualKey;
        UINT Modifiers;
        KEYSTROKE_FUNCTION Function;

        _KEYSTROKE()
        {
            VirtualKey = 0;
            Modifiers = 0;
            Function = FUNCTION_NONE;
        }
    };

    BOOL IsVirtualKeyKeystrokeComposition(UINT uCode, _Out_opt_ _KEYSTROKE_STATE *pKeyState, KEYSTROKE_FUNCTION function);
    BOOL IsVirtualKeyKeystrokeCandidate(UINT uCode, _In_ _KEYSTROKE_STATE *pKeyState, CANDIDATE_MODE candidateMode, _Out_ BOOL *pfRetCode, _In_ CNoneImeArray<_KEYSTROKE> *pKeystrokeMetric);
    BOOL IsKeystrokeRange(UINT uCode, _Out_ _KEYSTROKE_STATE *pKeyState, CANDIDATE_MODE candidateMode);

    void SetupKeystroke();
    void SetKeystrokeTable(_Inout_ CNoneImeArray<_KEYSTROKE> *pKeystroke);
    void SetInitialCandidateListRange();

    BOOL SetupDictionaryFile();
    CFile* GetDictionaryFile();

private:
    CTableDictionaryEngine* _pTableDictionaryEngine;
    CFileMapping* _pHomophoneDictionaryFile;
    CStringRange _keystrokeBuffer;

    LANGID _langid;
    GUID _guidProfile;
    TfClientId _tfClientId;

    CNoneImeArray<_KEYSTROKE> _KeystrokeComposition;
    CNoneImeArray<_KEYSTROKE> _KeystrokeCandidate;

    BOOL _isComLessMode;
    CCandidateRange _candidateListIndexRange;

    CFile* _pDictionaryFile;
};
