// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "FileMapping.h"

//---------------------------------------------------------------------
//
// ctor
//
//---------------------------------------------------------------------

CFileMapping::CFileMapping()
{
    _fileHandle = nullptr;
    _fileMappingHandle = nullptr;
    _pMapBuffer = nullptr;
    _fileSize = 0;
}

//---------------------------------------------------------------------
//
// dtor
//
//---------------------------------------------------------------------

BOOL CFileMapping::CreateFile(_In_ PCWSTR pFileName, DWORD desiredAccess, DWORD creationDisposition,
    DWORD sharedMode, _Inout_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD flagsAndAttributes,
    _Inout_opt_ HANDLE templateFileHandle)
{
    _fileHandle = ::CreateFile(pFileName, desiredAccess, sharedMode, lpSecurityAttributes,
        creationDisposition, flagsAndAttributes, templateFileHandle);
    if (_fileHandle == INVALID_HANDLE_VALUE)
    {
        _fileHandle = nullptr;
        return FALSE;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(_fileHandle, &fileSize) || fileSize.QuadPart < 0)
    {
        CloseHandle(_fileHandle);
        _fileHandle = nullptr;
        return FALSE;
    }
    _fileSize = (DWORD_PTR)fileSize.QuadPart;
    return TRUE;
}

CFileMapping::~CFileMapping()
{
    if (_pMapBuffer)
    {
        UnmapViewOfFile(_pMapBuffer);
        _pMapBuffer = nullptr;
    }
    if (_fileMappingHandle)
    {
        CloseHandle(_fileMappingHandle);
        _fileMappingHandle = nullptr;
    }
    if (_fileHandle)
    {
        CloseHandle(_fileHandle);
        _fileHandle = nullptr;
    }
}

const BYTE *CFileMapping::GetRawData()
{
    if (!_pMapBuffer && _fileSize > 0)
    {
        _fileMappingHandle = CreateFileMapping(_fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!_fileMappingHandle)
        {
            return nullptr;
        }

        _pMapBuffer = MapViewOfFile(_fileMappingHandle, FILE_MAP_READ, 0, 0, 0);
        if (!_pMapBuffer)
        {
            CloseHandle(_fileMappingHandle);
            _fileMappingHandle = nullptr;
            return nullptr;
        }
    }

    return (const BYTE *)_pMapBuffer;
}
