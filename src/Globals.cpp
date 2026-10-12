// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "resource.h"
#include "CandidateWindow.h"
#include "define.h"
#include "Globals.h"
#include "NoneIMEBaseStructure.h"

namespace Global {
HINSTANCE dllInstanceHandle;

LONG dllRefCount = -1;

CRITICAL_SECTION CS;
HFONT defaultlFontHandle;
UINT candidateFontSize = 14;
COLORREF candidateHighlightColor = CANDWND_SELECTED_BK_COLOR;

static BOOL GetIniPath(_Out_writes_(cchPath) LPWSTR iniPath, size_t cchPath)
{
    DWORD pathLength = GetModuleFileNameW(dllInstanceHandle, iniPath, static_cast<DWORD>(cchPath));
    if (pathLength == 0 || pathLength >= cchPath)
    {
        return FALSE;
    }

    WCHAR* pathSeparator = wcsrchr(iniPath, L'\\');
    if (pathSeparator == nullptr)
    {
        return FALSE;
    }
    pathSeparator[1] = L'\0';
    return SUCCEEDED(StringCchCatW(iniPath, cchPath, L"NoneIME.ini"));
}

BOOL GetConfiguredCinPath(_Out_writes_(cchPath) LPWSTR cinPath, size_t cchPath)
{
    if (cinPath == nullptr || cchPath == 0)
    {
        return FALSE;
    }

    WCHAR iniPath[MAX_PATH] = {};
    if (!GetIniPath(iniPath, ARRAYSIZE(iniPath)))
    {
        return FALSE;
    }

    WCHAR configuredPath[MAX_PATH] = {};
    GetPrivateProfileStringW(L"Dictionary", L"CinPath", TEXTSERVICE_CIN,
        configuredPath, ARRAYSIZE(configuredPath), iniPath);
    if (!configuredPath[0])
    {
        return FALSE;
    }

    BOOL isAbsolutePath = configuredPath[0] == L'\\' || configuredPath[0] == L'/' || configuredPath[1] == L':';
    if (isAbsolutePath)
    {
        return SUCCEEDED(StringCchCopyW(cinPath, cchPath, configuredPath));
    }

    WCHAR dictionaryDirectory[MAX_PATH] = {};
    if (FAILED(StringCchCopyW(dictionaryDirectory, ARRAYSIZE(dictionaryDirectory), iniPath)))
    {
        return FALSE;
    }
    WCHAR* pathSeparator = wcsrchr(dictionaryDirectory, L'\\');
    if (pathSeparator == nullptr)
    {
        return FALSE;
    }
    pathSeparator[1] = L'\0';

    return SUCCEEDED(StringCchCopyW(cinPath, cchPath, dictionaryDirectory)) &&
        SUCCEEDED(StringCchCatW(cinPath, cchPath, configuredPath));
}

void LoadCandidateWindowSettings()
{
    WCHAR iniPath[MAX_PATH] = {};
    if (!GetIniPath(iniPath, ARRAYSIZE(iniPath)))
    {
        return;
    }

    UINT fontSize = GetPrivateProfileIntW(L"CandidateWindow", L"CandidateFontSize", 14, iniPath);
    candidateFontSize = min(max(fontSize, 6), 72);

    WCHAR colorText[32] = {};
    GetPrivateProfileStringW(L"CandidateWindow", L"HighlightColor", L"0078D7", colorText, ARRAYSIZE(colorText), iniPath);
    const WCHAR* colorDigits = colorText;
    if (colorDigits[0] == L'#')
    {
        colorDigits++;
    }
    else if (colorDigits[0] == L'0' && (colorDigits[1] == L'x' || colorDigits[1] == L'X'))
    {
        colorDigits += 2;
    }

    WCHAR* colorEnd = nullptr;
    unsigned long colorValue = wcstoul(colorDigits, &colorEnd, 16);
    if (colorEnd != colorDigits && *colorEnd == L'\0' && colorValue <= 0xFFFFFF)
    {
        candidateHighlightColor = RGB((colorValue >> 16) & 0xFF, (colorValue >> 8) & 0xFF, colorValue & 0xFF);
    }
}

extern const CLSID NoneIMECLSID = { 
    0xd2291a80,
    0x84d8,
    0x4641,
    { 0x9a, 0xb2, 0xbd, 0xd1, 0x47, 0x2c, 0x84, 0x6b }
};

extern const GUID NoneIMEGuidProfile = { 
    0x83955c0e,
    0x2c09,
    0x47a5,
    { 0xbc, 0xf3, 0xf2, 0xb9, 0x8e, 0x11, 0xee, 0x8b }
};

extern const GUID NoneIMEGuidImeModePreserveKey = {
    0x4b62b54b, 
    0xf828, 
    0x43b5, 
    { 0x90, 0x95, 0xa9, 0x6d, 0xf9, 0xcb, 0xdf, 0x38 } 
};

extern const GUID NoneIMEGuidLangBarIMEMode = {
    0x89be500c,
    0x9462,
    0x4070,
    { 0x9d, 0xb0, 0xb4, 0x67, 0xbb, 0x5, 0x13, 0x27 }
};

extern const GUID NoneIMEGuidDisplayAttributeInput = {
    0x4c802e2c,
    0x8140,
    0x4436,
    { 0xa5, 0xe5, 0xf7, 0xc5, 0x44, 0xeb, 0xc9, 0xcd }
};

extern const GUID NoneIMEGuidDisplayAttributeConverted = {
    0x9a1cc683,
    0xf2a7,
    0x4701,
    { 0x9c, 0x6e, 0x2d, 0xa6, 0x9a, 0x5c, 0xd4, 0x74 }
};

extern const GUID NoneIMEGuidCandUIElement = {
    0x84b0749f,
    0x8de7,
    0x4732,
    { 0x90, 0x7a, 0x3b, 0xcb, 0x15, 0xa, 0x1, 0xa8 }
};

extern const WCHAR UnicodeByteOrderMark = 0xFEFF;

extern const WCHAR KeywordDelimiter = L'=';
extern const WCHAR StringDelimiter  = L'\"';

extern const WCHAR ImeModeDescription[] = L"Chinese/English input (Shift)";
extern const int ImeModeOnIcoIndex = IME_MODE_ON_ICON_INDEX;
extern const int ImeModeOffIcoIndex = IME_MODE_OFF_ICON_INDEX;

extern const WCHAR LangbarImeModeDescription[] = L"Conversion mode";

extern const WCHAR CandidateClassName[] = L"NoneIME.CandidateWindow";
ATOM AtomCandidateWindow;

BOOL RegisterWindowClass()
{
    if (!CCandidateWindow::_InitWindowClass(CandidateClassName, &AtomCandidateWindow))
    {
        return FALSE;
    }
    return TRUE;
}

#define TF_MOD_ALLALT     (TF_MOD_RALT | TF_MOD_LALT | TF_MOD_ALT)
#define TF_MOD_ALLCONTROL (TF_MOD_RCONTROL | TF_MOD_LCONTROL | TF_MOD_CONTROL)
#define TF_MOD_ALLSHIFT   (TF_MOD_RSHIFT | TF_MOD_LSHIFT | TF_MOD_SHIFT)
#define TF_MOD_RLALT      (TF_MOD_RALT | TF_MOD_LALT)
#define TF_MOD_RLCONTROL  (TF_MOD_RCONTROL | TF_MOD_LCONTROL)
#define TF_MOD_RLSHIFT    (TF_MOD_RSHIFT | TF_MOD_LSHIFT)

#define CheckMod(m0, m1, mod)        \
    if (m1 & TF_MOD_ ## mod ##)      \
{ \
    if (!(m0 & TF_MOD_ ## mod ##)) \
{      \
    return FALSE;   \
}      \
} \
    else       \
{ \
    if ((m1 ^ m0) & TF_MOD_RL ## mod ##)    \
{      \
    return FALSE;   \
}      \
} \

BOOL CheckModifiers(UINT modCurrent, UINT mod)
{
    mod &= ~TF_MOD_ON_KEYUP;

    if (mod & TF_MOD_IGNORE_ALL_MODIFIER)
    {
        return TRUE;
    }

    if (modCurrent == mod)
    {
        return TRUE;
    }

    if (modCurrent && !mod)
    {
        return FALSE;
    }

    CheckMod(modCurrent, mod, ALT);
    CheckMod(modCurrent, mod, SHIFT);
    CheckMod(modCurrent, mod, CONTROL);

    return TRUE;
}

USHORT ModifiersValue = 0;
BOOL   IsShiftKeyDownOnly = FALSE;
BOOL   IsControlKeyDownOnly = FALSE;
BOOL   IsAltKeyDownOnly = FALSE;

BOOL UpdateModifiers(WPARAM wParam, LPARAM lParam)
{
    SHORT sksMenu = GetKeyState(VK_MENU);
    SHORT sksCtrl = GetKeyState(VK_CONTROL);
    SHORT sksShft = GetKeyState(VK_SHIFT);

    switch (wParam & 0xff)
    {
    case VK_MENU:
        if (sksMenu & 0x8000)
        {
            if (lParam & 0x01000000)
            {
                ModifiersValue |= (TF_MOD_RALT | TF_MOD_ALT);
            }
            else
            {
                ModifiersValue |= (TF_MOD_LALT | TF_MOD_ALT);
            }

            if (!(lParam & 0x40000000))
            {
                if (!(sksCtrl & 0x8000) && !(sksShft & 0x8000))
                {
                    IsAltKeyDownOnly = TRUE;
                }
                else
                {
                    IsShiftKeyDownOnly = FALSE;
                    IsControlKeyDownOnly = FALSE;
                    IsAltKeyDownOnly = FALSE;
                }
            }
        }
        break;

    case VK_CONTROL:
        if (sksCtrl & 0x8000)    
        {
            if (lParam & 0x01000000)
            {
                ModifiersValue |= (TF_MOD_RCONTROL | TF_MOD_CONTROL);
            }
            else
            {
                ModifiersValue |= (TF_MOD_LCONTROL | TF_MOD_CONTROL);
            }

            if (!(lParam & 0x40000000))
            {
                if (!(sksShft & 0x8000) && !(sksMenu & 0x8000))
                {
                    IsControlKeyDownOnly = TRUE;
                }
                else
                {
                    IsShiftKeyDownOnly = FALSE;
                    IsControlKeyDownOnly = FALSE;
                    IsAltKeyDownOnly = FALSE;
                }
            }
        }
        break;

    case VK_SHIFT:
        if (sksShft & 0x8000)    
        {
            if (((lParam >> 16) & 0x00ff) == 0x36)
            {
                ModifiersValue |= (TF_MOD_RSHIFT | TF_MOD_SHIFT);
            }
            else
            {
                ModifiersValue |= (TF_MOD_LSHIFT | TF_MOD_SHIFT);
            }

            if (!(lParam & 0x40000000))
            {
                if (!(sksMenu & 0x8000) && !(sksCtrl & 0x8000))
                {
                    IsShiftKeyDownOnly = TRUE;
                }
                else
                {
                    IsShiftKeyDownOnly = FALSE;
                    IsControlKeyDownOnly = FALSE;
                    IsAltKeyDownOnly = FALSE;
                }
            }
        }
        break;

    default:
        IsShiftKeyDownOnly = FALSE;
        IsControlKeyDownOnly = FALSE;
        IsAltKeyDownOnly = FALSE;
        break;
    }

    if (!(sksMenu & 0x8000))
    {
        ModifiersValue &= ~TF_MOD_ALLALT;
    }
    if (!(sksCtrl & 0x8000))
    {
        ModifiersValue &= ~TF_MOD_ALLCONTROL;
    }
    if (!(sksShft & 0x8000))
    {
        ModifiersValue &= ~TF_MOD_ALLSHIFT;
    }

    return TRUE;
}

BOOL CompareElements(LCID locale, const CStringRange* pElement1, const CStringRange* pElement2)
{
    return (CStringRange::Compare(locale, (CStringRange*)pElement1, (CStringRange*)pElement2) == CSTR_EQUAL) ? TRUE : FALSE;
}
}
