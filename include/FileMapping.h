// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#pragma once

class CFileMapping
{
public:
    CFileMapping();
    ~CFileMapping();

    BOOL CreateFile(_In_ PCWSTR pFileName, DWORD desiredAccess, DWORD creationDisposition,
        DWORD sharedMode = 0, _Inout_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes = nullptr, DWORD flagsAndAttributes = 0, _Inout_opt_ HANDLE templateFileHandle = nullptr);
    const BYTE *GetRawData();
    DWORD_PTR GetFileSize() const { return _fileSize; }

private:
    HANDLE _fileHandle;
    HANDLE _fileMappingHandle;
    const VOID *_pMapBuffer;
    DWORD_PTR _fileSize;
};
