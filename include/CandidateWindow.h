// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

#include "Private.h"
#include "NoneIMEBaseStructure.h"

enum CANDWND_ACTION
{
    CAND_ITEM_SELECT
};

typedef HRESULT (*CANDWNDCALLBACK)(void *pv, enum CANDWND_ACTION action);

class CCandidateWindow
{
public:
    CCandidateWindow(_In_ CANDWNDCALLBACK pfnCallback, _In_ void *pv, _In_ CCandidateRange *pIndexRange, _In_ BOOL isStoreAppMode);
    virtual ~CCandidateWindow();

    BOOL _Create(ATOM atom, _In_opt_ HWND parentWndHandle);
    void _Destroy();

    void _Move(int x, int y);
    void _Show(BOOL isShowWnd);
    void _ResizeToFitCandidates();

    VOID _SetTextColor(_In_ COLORREF crColor, _In_ COLORREF crBkColor);
    VOID _SetSelectedTextColor(_In_ COLORREF crColor);
    VOID _SetFillColor(_In_ HBRUSH hBrush);

    void _AddString(_Inout_ CCandidateListItem *pCandidateItem, _In_ BOOL isAddFindKeyCode);
    void _ClearList();
    UINT _GetCount() { return _candidateList.Count(); }
    UINT _GetSelection() { return _currentSelection; }

    DWORD _GetCandidateString(_In_ int iIndex, _Outptr_result_maybenull_z_ const WCHAR **ppwchCandidateString);
    DWORD _GetSelectedCandidateString(_Outptr_result_maybenull_ const WCHAR **ppwchCandidateString);

    BOOL _MoveSelection(_In_ int offSet, _In_ BOOL isNotify);
    BOOL _SetSelection(_In_ int iPage, _In_ BOOL isNotify);
    void _SetSelection(_In_ int nIndex);
    BOOL _MovePage(_In_ int offSet, _In_ BOOL isNotify, _In_ BOOL isCycle = FALSE);
    BOOL _SetSelectionInPage(int nPos);

    HRESULT _GetPageIndex(UINT *pIndex, _In_ UINT uSize, _Inout_ UINT *puPageCnt);
    HRESULT _SetPageIndex(UINT *pIndex, _In_ UINT uPageCnt);
    HRESULT _GetCurrentPage(_Inout_ UINT *pCurrentPage);
    HRESULT _GetCurrentPage(_Inout_ int *pCurrentPage);

    HWND _GetWnd() { return _wndHandle; }

    static BOOL _InitWindowClass(_In_ LPCWSTR lpwszClassName, _Out_ ATOM *patom);
    static void _UninitWindowClass(ATOM atom);

private:
    static LRESULT CALLBACK _WindowProc(_In_ HWND wndHandle, UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam);
    LRESULT _OnPaint(HWND wndHandle);
    LRESULT _OnLButtonDown(LPARAM lParam);

    void _DrawList(HDC hdc);
    BOOL _AdjustPageIndexForSelection();

private:
    HWND _wndHandle;
    UINT _currentSelection;
    CNoneImeArray<CCandidateListItem> _candidateList;
    CNoneImeArray<UINT> _PageIndex;

    COLORREF _crTextColor;
    COLORREF _crSelectedTextColor;
    COLORREF _crBkColor;

    CCandidateRange* _pIndexRange;
    CANDWNDCALLBACK _pfnCallback;
    void* _pObj;
    BOOL _isStoreAppMode;
};
