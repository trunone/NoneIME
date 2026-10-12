// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

#include "private.h"
#include "define.h"
#include "NoneIMEBaseStructure.h"

void DllAddRef();
void DllRelease();

namespace Global {

inline void SafeRelease(_In_ IUnknown *punk)
{
    if (punk != nullptr)
    {
        punk->Release();
    }
}

inline void QuickVariantInit(_Inout_ VARIANT *pvar)
{
    pvar->vt = VT_EMPTY;
}

inline void QuickVariantClear(_Inout_ VARIANT *pvar)
{
    switch (pvar->vt) 
    {
    case VT_EMPTY:
    case VT_NULL:
    case VT_I2:
    case VT_I4:
    case VT_R4:
    case VT_R8:
    case VT_CY:
    case VT_DATE:
    case VT_I1:
    case VT_UI1:
    case VT_UI2:
    case VT_UI4:
    case VT_I8:
    case VT_UI8:
    case VT_INT:
    case VT_UINT:
    case VT_BOOL:
        break;

    case VT_UNKNOWN:
        SafeRelease(pvar->punkVal);
        break;

    default:
        VariantClear(pvar);
        break;
    }
    pvar->vt = VT_EMPTY;
}

inline BOOL IsTooSimilar(COLORREF cr1, COLORREF cr2)
{
    if ((cr1 | cr2) & 0xFF000000)
    {
        return FALSE;
    }

    LONG DeltaR = abs(GetRValue(cr1) - GetRValue(cr2));
    LONG DeltaG = abs(GetGValue(cr1) - GetGValue(cr2));
    LONG DeltaB = abs(GetBValue(cr1) - GetBValue(cr2));

    return DeltaR + DeltaG + DeltaB < 80;
}

extern HINSTANCE dllInstanceHandle;
extern ATOM AtomCandidateWindow;

BOOL RegisterWindowClass();

extern LONG dllRefCount;

extern CRITICAL_SECTION CS;
extern HFONT defaultlFontHandle;
extern UINT candidateFontSize;
extern COLORREF candidateHighlightColor;
BOOL GetConfiguredCinPath(_Out_writes_(cchPath) LPWSTR cinPath, size_t cchPath);
void LoadCandidateWindowSettings();

extern const CLSID NoneIMECLSID;
extern const CLSID NoneIMEGuidProfile;
extern const CLSID NoneIMEGuidImeModePreserveKey;

BOOL CheckModifiers(UINT uModCurrent, UINT uMod);
BOOL UpdateModifiers(WPARAM wParam, LPARAM lParam);

extern USHORT ModifiersValue;
extern BOOL IsShiftKeyDownOnly;
extern BOOL IsControlKeyDownOnly;
extern BOOL IsAltKeyDownOnly;

extern const GUID NoneIMEGuidLangBarIMEMode;

extern const GUID NoneIMEGuidDisplayAttributeInput;
extern const GUID NoneIMEGuidDisplayAttributeConverted;

extern const GUID NoneIMEGuidCandUIElement;

extern const WCHAR UnicodeByteOrderMark;
extern const WCHAR KeywordDelimiter;
extern const WCHAR StringDelimiter;

extern const WCHAR ImeModeDescription[];
extern const int ImeModeOnIcoIndex;
extern const int ImeModeOffIcoIndex;

extern const WCHAR LangbarImeModeDescription[];
}
