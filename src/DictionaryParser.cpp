// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "DictionaryParser.h"
#include "NoneIMEBaseStructure.h"

//---------------------------------------------------------------------
//
// ctor
//
//---------------------------------------------------------------------

CDictionaryParser::CDictionaryParser(LCID locale)
{
    _locale = locale;
    _isCinTable = FALSE;
    _insideCinTable = FALSE;
}

//---------------------------------------------------------------------
//
// dtor
//
//---------------------------------------------------------------------

CDictionaryParser::~CDictionaryParser()
{
}

//---------------------------------------------------------------------
//
// ParseLine
//
// dwBufLen - in character count
//
//---------------------------------------------------------------------

BOOL CDictionaryParser::ParseLine(_In_reads_(dwBufLen) LPCWSTR pwszBuffer, DWORD_PTR dwBufLen, _Out_ CParserStringRange *psrgKeyword, _Inout_opt_ CNoneImeArray<CParserStringRange> *pValue)
{
    DWORD_PTR lineStart = 0;
    while (lineStart < dwBufLen && (pwszBuffer[lineStart] == L' ' || pwszBuffer[lineStart] == L'\t'))
    {
        lineStart++;
    }

    if (dwBufLen - lineStart >= 3 && wcsncmp(pwszBuffer + lineStart, L"###", 3) == 0)
    {
        _isCinTable = TRUE;
        psrgKeyword->Set(pwszBuffer, 0);
        return TRUE;
    }

    BOOL isBeginDefinition = dwBufLen - lineStart >= 16 &&
        wcsncmp(pwszBuffer + lineStart, L"BEGIN_DEFINITION", 16) == 0;
    BOOL isBeginTable = dwBufLen - lineStart >= 11 &&
        wcsncmp(pwszBuffer + lineStart, L"BEGIN_TABLE", 11) == 0;
    if (isBeginDefinition || isBeginTable)
    {
        _isCinTable = TRUE;
        _insideCinTable = isBeginTable;
        psrgKeyword->Set(pwszBuffer, 0);
        return TRUE;
    }

    if (_isCinTable)
    {
        if (dwBufLen - lineStart >= 9 && wcsncmp(pwszBuffer + lineStart, L"END_TABLE", 9) == 0)
        {
            _insideCinTable = FALSE;
            psrgKeyword->Set(pwszBuffer, 0);
            return TRUE;
        }
        if (!_insideCinTable || lineStart == dwBufLen || pwszBuffer[lineStart] == L'#')
        {
            psrgKeyword->Set(pwszBuffer, 0);
            return TRUE;
        }

        DWORD_PTR keyEnd = lineStart;
        while (keyEnd < dwBufLen && pwszBuffer[keyEnd] != L' ' && pwszBuffer[keyEnd] != L'\t')
        {
            keyEnd++;
        }
        DWORD_PTR valueStart = keyEnd;
        while (valueStart < dwBufLen && (pwszBuffer[valueStart] == L' ' || pwszBuffer[valueStart] == L'\t'))
        {
            valueStart++;
        }
        DWORD_PTR valueEnd = valueStart;
        while (valueEnd < dwBufLen && pwszBuffer[valueEnd] != L' ' && pwszBuffer[valueEnd] != L'\t')
        {
            valueEnd++;
        }
        if (keyEnd == lineStart || valueEnd == valueStart)
        {
            psrgKeyword->Set(pwszBuffer, 0);
            return TRUE;
        }

        psrgKeyword->Set(pwszBuffer + lineStart, keyEnd - lineStart);
        if (pValue)
        {
            CParserStringRange* value = pValue->Append();
            if (!value)
            {
                return FALSE;
            }
            value->Set(pwszBuffer + valueStart, valueEnd - valueStart);
        }
        return TRUE;
    }

    LPCWSTR pwszKeyWordDelimiter = nullptr;
    pwszKeyWordDelimiter = GetToken(pwszBuffer, dwBufLen, Global::KeywordDelimiter, psrgKeyword);
    if (!(pwszKeyWordDelimiter))
    {
        return FALSE;    // End of file
    }

    dwBufLen -= (pwszKeyWordDelimiter - pwszBuffer);
    pwszBuffer = pwszKeyWordDelimiter + 1;
    dwBufLen--;

    // Get value.
    if (pValue)
    {
        if (dwBufLen)
        {
            CParserStringRange* psrgValue = pValue->Append();
            if (!psrgValue)
            {
                return FALSE;
            }
            psrgValue->Set(pwszBuffer, dwBufLen);
            RemoveWhiteSpaceFromBegin(psrgValue);
            RemoveWhiteSpaceFromEnd(psrgValue);
            RemoveStringDelimiter(psrgValue);
        }
    }

    return TRUE;
}

//---------------------------------------------------------------------
//
// GetToken
//
// dwBufLen - in character count
//
// return   - pointer of delimiter which specified chDelimiter
//
//---------------------------------------------------------------------
_Ret_maybenull_
LPCWSTR CDictionaryParser::GetToken(_In_reads_(dwBufLen) LPCWSTR pwszBuffer, DWORD_PTR dwBufLen, _In_ const WCHAR chDelimiter, _Out_ CParserStringRange *psrgValue)
{
    WCHAR ch = '\0';

    psrgValue->Set(pwszBuffer, dwBufLen);

    ch = *pwszBuffer;
    while ((ch) && (ch != chDelimiter) && dwBufLen)
    {
        dwBufLen--;
        pwszBuffer++;

        if (ch == Global::StringDelimiter)
        {
            while (*pwszBuffer && (*pwszBuffer != Global::StringDelimiter) && dwBufLen)
            {
                dwBufLen--;
                pwszBuffer++;
            }
            if (*pwszBuffer && dwBufLen)
            {
                dwBufLen--;
                pwszBuffer++;
            }
            else
            {
                return nullptr;
            }
        }
        ch = *pwszBuffer;
    }

    if (*pwszBuffer && dwBufLen)
    {
        LPCWSTR pwszStart = psrgValue->Get();

        psrgValue->Set(pwszStart, pwszBuffer - pwszStart);

        RemoveWhiteSpaceFromBegin(psrgValue);
        RemoveWhiteSpaceFromEnd(psrgValue);
        RemoveStringDelimiter(psrgValue);

        return pwszBuffer;
    }

    RemoveWhiteSpaceFromBegin(psrgValue);
    RemoveWhiteSpaceFromEnd(psrgValue);
    RemoveStringDelimiter(psrgValue);

    return nullptr;
}

//---------------------------------------------------------------------
//
// RemoveWhiteSpaceFromBegin
// RemoveWhiteSpaceFromEnd
// RemoveStringDelimiter
//
//---------------------------------------------------------------------

BOOL CDictionaryParser::RemoveWhiteSpaceFromBegin(_Inout_opt_ CStringRange *pString)
{
    DWORD_PTR dwIndexTrace = 0;  // in char

    if (pString == nullptr)
    {
        return FALSE;
    }

    if (SkipWhiteSpace(_locale, pString->Get(), pString->GetLength(), &dwIndexTrace) != S_OK)
    {
        return FALSE;
    }

    pString->Set(pString->Get() + dwIndexTrace, pString->GetLength() - dwIndexTrace);
    return TRUE;
}

BOOL CDictionaryParser::RemoveWhiteSpaceFromEnd(_Inout_opt_ CStringRange *pString)
{
    if (pString == nullptr)
    {
        return FALSE;
    }

    DWORD_PTR dwTotalBufLen = pString->GetLength();
    LPCWSTR pwszEnd = pString->Get() + dwTotalBufLen - 1;

    while (dwTotalBufLen && (IsSpace(_locale, *pwszEnd) || *pwszEnd == L'\r' || *pwszEnd == L'\n'))
    {
        pwszEnd--;
        dwTotalBufLen--;
    }

    pString->Set(pString->Get(), dwTotalBufLen);
    return TRUE;
}

BOOL CDictionaryParser::RemoveStringDelimiter(_Inout_opt_ CStringRange *pString)
{
    if (pString == nullptr)
    {
        return FALSE;
    }

    if (pString->GetLength() >= 2)
    {
        if ((*pString->Get() == Global::StringDelimiter) && (*(pString->Get()+pString->GetLength()-1) == Global::StringDelimiter))
        {
            pString->Set(pString->Get()+1, pString->GetLength()-2);
            return TRUE;
        }
    }

    return FALSE;
}

//---------------------------------------------------------------------
//
// GetOneLine
//
// dwBufLen - in character count
//
//---------------------------------------------------------------------

DWORD_PTR CDictionaryParser::GetOneLine(_In_z_ LPCWSTR pwszBuffer, DWORD_PTR dwBufLen)
{
    for (DWORD_PTR index = 0; index < dwBufLen; index++)
    {
        if (pwszBuffer[index] == L'\r' || pwszBuffer[index] == L'\n' || pwszBuffer[index] == L'\0')
        {
            return index;
        }
    }
    return dwBufLen;
}
