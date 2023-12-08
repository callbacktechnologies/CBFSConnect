/*
 * CBFS Connect 2024 C++ Edition - Sample Project
 *
 * This sample project demonstrates the usage of CBFS Connect in a 
 * simple, straightforward way. It is not intended to be a complete 
 * application. Error handling and other checks are simplified for clarity.
 *
 * www.callback.com/cbfsconnect
 *
 * This code is subject to the terms and conditions specified in the 
 * corresponding product license agreement which outlines the authorized 
 * usage and restrictions.
 */

#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h> 
#include <wchar.h>
#include <iostream>
#include <Shlwapi.h>
#include <filesystem>
#include <string>

#include "Sddl.h"

#ifdef _WIN32
// Windows Header Files:
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <aclapi.h>
// C RunTime Header Files
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <commctrl.h>
#include <assert.h>
#include <initguid.h>
#include <dskquota.h>
#include <winioctl.h>
#endif

#ifndef WIN32
#include <poll.h>
#include <dirent.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/types.h>
#endif

#ifdef _UNICODE
#include "../../include/unicode/cbfs.h"
#include "../../include/unicode/cbcache.h"
#else
#include "../../include/cbfs.h"
#include "../../include/cbcache.h"
#endif

using namespace std;

#ifdef UNICODE
#define sout wcout
#define scin wcin
typedef std::wstring cbt_string;
#else
#define sout cout
#define scin cin
typedef std::string cbt_string;
#define _T(q) q
#endif

#ifndef FSCTL_SET_REPARSE_POINT
#define FSCTL_SET_REPARSE_POINT         CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 41, METHOD_BUFFERED, FILE_SPECIAL_ACCESS) // REPARSE_DATA_BUFFER,
#endif

#ifndef FSCTL_GET_REPARSE_POINT
#define FSCTL_GET_REPARSE_POINT         CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 42, METHOD_BUFFERED, FILE_ANY_ACCESS) // REPARSE_DATA_BUFFER
#endif

#ifndef FSCTL_DELETE_REPARSE_POINT
#define FSCTL_DELETE_REPARSE_POINT      CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 43, METHOD_BUFFERED, FILE_SPECIAL_ACCESS) // REPARSE_DATA_BUFFER,
#endif

#define FILE_OPEN 1
#define FILE_OPEN_IF 3

// 1 Mb
#define BUFFER_SIZE 1048576

typedef enum _CONTEXT_NODE_TYPE_CODE {
    NTC_DIRECTORY_ENUM = 0x7534,
    NTC_NAMED_STREAMS_ENUM = 0x6209,
    NTC_HARD_LINKS_ENUM = 0x620A,
    NTC_FILE_CONTEXT = 0x1728,
    NTC_QUOTAS_CONTEXT = 0x2315

} CONTEXT_NODE_TYPE_CODE;

typedef enum _FSINFOCLASS {
    FileFsVolumeInformation = 1,
    FileFsLabelInformation,      // 2
    FileFsSizeInformation,       // 3
    FileFsDeviceInformation,     // 4
    FileFsAttributeInformation,  // 5
    FileFsControlInformation,    // 6
    FileFsFullSizeInformation,   // 7
    FileFsObjectIdInformation,   // 8
    FileFsDriverPathInformation, // 9
    FileFsMaximumInformation
} FS_INFORMATION_CLASS, * PFS_INFORMATION_CLASS;

typedef struct _FILE_FS_CONTROL_INFORMATION {
    LARGE_INTEGER FreeSpaceStartFiltering;
    LARGE_INTEGER FreeSpaceThreshold;
    LARGE_INTEGER FreeSpaceStopFiltering;
    LARGE_INTEGER DefaultQuotaThreshold;
    LARGE_INTEGER DefaultQuotaLimit;
    ULONG FileSystemControlFlags;
} FILE_FS_CONTROL_INFORMATION, * PFILE_FS_CONTROL_INFORMATION;

//
// CONTEXT_HEADER
//
// Header is used to mark the structures
//

typedef struct _CONTEXT_HEADER {
    DWORD     NodeTypeCode;
    DWORD     NodeByteSize;
} CONTEXT_HEADER, * PCONTEXT_HEADER;

typedef struct
{
    CONTEXT_HEADER Header;
    WIN32_FIND_DATAW finfo;
    HANDLE hFile;
}   ENUM_INFO, * PENUM_INFO;

typedef struct
{
    CONTEXT_HEADER Header;
    INT   OpenCount;
    HANDLE hFile;
    BOOL  ReadOnly;
}   FILE_CONTEXT, * PFILE_CONTEXT;

#define CBFSCONNECT_TAG 0x5107

typedef DWORD NODE_TYPE_CODE;
typedef NODE_TYPE_CODE* PNODE_TYPE_CODE;

#define NodeType(Ptr) (*((PNODE_TYPE_CODE)(Ptr)))

BOOL EnablePrivilege(LPCWSTR);

BOOL IsRecycleBinItem(LPCWSTR);

BOOL WIN32_FROM_HRESULT(IN HRESULT hr, OUT DWORD* Win32);

__int64 GetFileId(LPCWSTR);

WCHAR g_RootPath[_MAX_PATH];

//////////////////////////////////////////////////////////////////////////
// Some system definitions that are necessary to use NT API.
//////////////////////////////////////////////////////////////////////////

typedef LONG NTSTATUS, * PNTSTATUS;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
#ifdef MIDL_PASS
    [size_is(MaximumLength / 2), length_is((Length) / 2)] USHORT* Buffer;
#else // MIDL_PASS
    PWSTR  Buffer;
#endif // MIDL_PASS
} UNICODE_STRING;
typedef UNICODE_STRING* PUNICODE_STRING;
typedef const UNICODE_STRING* PCUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;        // Points to type SECURITY_DESCRIPTOR
    PVOID SecurityQualityOfService;  // Points to type SECURITY_QUALITY_OF_SERVICE
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES* POBJECT_ATTRIBUTES;
typedef CONST OBJECT_ATTRIBUTES* PCOBJECT_ATTRIBUTES;

typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    };

    ULONG* Information;
} IO_STATUS_BLOCK, * PIO_STATUS_BLOCK;

typedef struct _FILE_NAME_INFORMATION {
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_NAME_INFORMATION, * PFILE_NAME_INFORMATION;

#define InitializeObjectAttributes( p, n, a, r, s ) {   \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES );          \
    (p)->RootDirectory = r;                             \
    (p)->Attributes = a;                                \
    (p)->ObjectName = n;                                \
    (p)->SecurityDescriptor = s;                        \
    (p)->SecurityQualityOfService = NULL;               \
    }

typedef NTSTATUS(NTAPI* PfnNtOpenFile)(
    PHANDLE,
    ACCESS_MASK,
    POBJECT_ATTRIBUTES,
    PIO_STATUS_BLOCK,
    ULONG,
    ULONG
    );

typedef BOOL(WINAPI* PfnGetFileInformationByHandleEx)(
    HANDLE hFile,
    FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
    LPVOID lpFileInformation,
    DWORD dwBufferSize
    );

typedef NTSTATUS(NTAPI* PfnNtSetVolumeInformationFile)(
    HANDLE               FileHandle,
    PIO_STATUS_BLOCK     IoStatusBlock,
    PVOID                FsInformation,
    ULONG                Length,
    FS_INFORMATION_CLASS FsInformationClass
    );

typedef NTSTATUS(NTAPI* PfnNtQueryVolumeInformationFile)(
    HANDLE               FileHandle,
    PIO_STATUS_BLOCK     IoStatusBlock,
    PVOID                FsInformation,
    ULONG                Length,
    FS_INFORMATION_CLASS FsInformationClass
    );

typedef NTSTATUS(NTAPI* PfnNtQueryInformationFile)(
    HANDLE  FileHandle,
    PIO_STATUS_BLOCK  IoStatusBlock,
    PVOID  FileInformation,
    ULONG  Length,
    ULONG  FileInformationClass
    );

extern "C" ULONG NTAPI LsaNtStatusToWinError(NTSTATUS Status);


inline __int64 FileTimeToInt64(FILETIME ft)
{
    return static_cast<__int64>(ft.dwHighDateTime) << 32 | ft.dwLowDateTime;
}

inline void Int64ToFileTime(__int64 t, LPFILETIME pft)
{
    pft->dwLowDateTime = (DWORD)t;
    pft->dwHighDateTime = t >> 32;
}

//////////////////////////////////////////////////////////////////////////

class CacheFolderDriveCBCache : public CBCache
{

    INT FireReadData(CBCacheReadDataEventParams* e)
    {
        HANDLE FileHandle = e->FileContext;
        if (FileHandle == INVALID_HANDLE_VALUE)
        {
            e->ResultCode = ERROR_INVALID_HANDLE_STATE; // should never happen as we open a file earlier
            return e->ResultCode;
        }
        if (e->BytesToRead == 0)
            return e->ResultCode;

        if ((e->Flags & cbcConstants::RWEVENT_CANCELED) == 0)
        {
            DWORD Count;
            Count = (DWORD)(e->BytesToRead);

            LARGE_INTEGER pos;
            pos.QuadPart = e->Position;

            if (!::SetFilePointerEx(FileHandle, pos, NULL, FILE_BEGIN))
            {
                e->ResultCode = cbcConstants::RWRESULT_FILE_FAILURE;
                return e->ResultCode;
            }

            DWORD ToRead;
            DWORD BytesRead = 0;

            char* BufferPtr = (char*)e->Buffer;

            DWORD TotalBytesRead = 0;
            while (Count > 0)
            {
                ToRead = Count;
                if (!::ReadFile(FileHandle, BufferPtr, ToRead, &BytesRead, NULL))
                {
                    e->ResultCode = cbcConstants::RWRESULT_FILE_FAILURE;
                    return e->ResultCode;
                }
                if (BytesRead <= 0)
                    break;

                BufferPtr += BytesRead;
                Count -= BytesRead;
                TotalBytesRead += BytesRead;
            }
            e->BytesRead = (INT)TotalBytesRead;
        }
        else
            e->BytesRead = 0;

        e->ResultCode = 0;
        return 0;
    }

    INT FireWriteData(CBCacheWriteDataEventParams* e)
    {
        e->BytesWritten = 0;

        HANDLE FileHandle = e->FileContext;
        if (FileHandle == INVALID_HANDLE_VALUE)
        {
            e->ResultCode = cbcConstants::RWRESULT_FILE_FAILURE;
            return e->ResultCode;
        }
        if (e->BytesToWrite == 0)
            return e->ResultCode;

        if ((e->Flags & cbcConstants::RWEVENT_CANCELED) == 0)
        {

            DWORD Count = (DWORD)(e->BytesToWrite);

            LARGE_INTEGER pos;
            pos.QuadPart = e->Position;

            LARGE_INTEGER CurrentPosition;

            if (!::SetFilePointerEx(FileHandle, pos, &CurrentPosition, FILE_BEGIN))
            {
                e->ResultCode = cbcConstants::RWRESULT_FILE_FAILURE;
                return 0;
            }
            DWORD BytesToWrite;
            DWORD BytesWritten = 0;

            DWORD TotalBytesWritten = 0;

            char* BufferPtr = (char*)e->Buffer;

            while (Count > 0)
            {
                BytesToWrite = Count;

                if (!::WriteFile(FileHandle, BufferPtr, BytesToWrite, &BytesWritten, NULL))
                {
                    e->ResultCode = cbcConstants::RWRESULT_FILE_FAILURE;
                    return e->ResultCode;
                }
                if (BytesWritten <= 0)
                    break;

                Count -= BytesToWrite;
                TotalBytesWritten += BytesToWrite;
            }

            e->BytesWritten = (INT)(TotalBytesWritten);

            ::FlushFileBuffers(FileHandle);
        }

        e->ResultCode = 0;
        return 0;
    }
};

class CacheFolderDriveCBFS : public CBFS
{

private:
    CBCache* cache;

public: // Events
    CacheFolderDriveCBFS() : CBFS()
    {
        cache = NULL;
    }

    void SetCache(CBCache* value)
    {
        cache = value;
    }

    LPWSTR GetDiskName(LPCWSTR VFSName)
    {
        LPWSTR Result = (LPWSTR)malloc((wcslen(VFSName) + wcslen(g_RootPath) + 1) * sizeof(WCHAR));
        if (!Result)
            return NULL;
        wcscpy(Result, g_RootPath);
        wcscat(Result, VFSName);
        return Result;
    }

    int CopyStringToBuffer(LPWSTR Buffer, INT& BufferLengthVar, LPWSTR Source)
    {
        if (Buffer == NULL || Source == NULL)
            return ERROR_INVALID_PARAMETER;

        int SourceLength = (int)wcslen(Source);
        return CopyStringToBuffer(Buffer, BufferLengthVar, Source, SourceLength);
    }

    // Returns the value, which should be assigned to ResultCode and, if non-zero, reported as a result of the event handler
    int CopyStringToBuffer(LPWSTR Buffer, INT& BufferLengthVar, LPWSTR Source, int SourceLength)
    {
        if (Buffer == NULL || Source == NULL)
            return ERROR_INVALID_PARAMETER;

        if (SourceLength == 0)
        {
            Buffer[0] = 0;
            BufferLengthVar = 0;
            return 0;
        }
        else
            if (BufferLengthVar < SourceLength)
            {
                BufferLengthVar = 0;
                return ERROR_INSUFFICIENT_BUFFER;
            }
            else
            {
                wcsncpy(Buffer, Source, SourceLength);
                if (SourceLength < BufferLengthVar)
                    Buffer[SourceLength] = 0;
                BufferLengthVar = SourceLength;
                return 0;
            }
    }

    int GetSectorSize()
    {
        LPWSTR SS = this->Config(L"SectorSize");
        if (SS != NULL && wcslen(SS) > 0)
            return _wtoi(SS);
        else
            return 0;
    }

    INT FireMount(CBFSMountEventParams* e) override
    {
        return 0;
    }

    INT FireUnmount(CBFSUnmountEventParams* e) override
    {
        return 0;
    }

    INT IsEmptyDirectory(LPCWSTR DirName, BOOL& IsEmpty)
    {
#ifdef UNICODE
        WIN32_FIND_DATAW fd;
#else
        WIN32_FIND_DATAA fd;
#endif
        HANDLE hFind = INVALID_HANDLE_VALUE;
        BOOL find = FALSE;

        LPWSTR FullName = (LPWSTR)malloc((wcslen(DirName) + wcslen(g_RootPath) + 3) * sizeof(WCHAR));
        if (NULL == FullName)
            return ERROR_NOT_ENOUGH_MEMORY;

        wcscpy(FullName, g_RootPath);
        wcscat(FullName, DirName);
        wcscat(FullName, L"\\*");

        hFind = FindFirstFile(FullName, &fd);

        while (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
        {
            IsEmpty = !FindNextFile(hFind, &fd);
            if (IsEmpty) break;
        }

        free(FullName);
        FindClose(hFind);

        return 0;
    }

    INT FireCanFileBeDeleted(CBFSCanFileBeDeletedEventParams* e) override
    {
        BOOL IsEmpty;
        DWORD Attrs;

        LPCWSTR FileName = e->FileName;
        LPWSTR FName = static_cast<LPWSTR>(malloc((wcslen(FileName) + wcslen(g_RootPath) + 1) * sizeof(WCHAR)));

        if (NULL == FName)
        {
            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            return e->ResultCode;
        }

        // we have a stream here
        if (wcschr(FName, ':') != NULL)
            return 0;

        wcscpy(FName, g_RootPath);
        wcscat(FName, FileName);

        Attrs = GetFileAttributes(FName);
        free(FName);

        e->CanBeDeleted = TRUE;

        if ((Attrs != INVALID_FILE_ATTRIBUTES) && ((Attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) && ((Attrs & FILE_ATTRIBUTE_REPARSE_POINT) == 0))
        {
            e->ResultCode = IsEmptyDirectory(e->FileName, IsEmpty);
            if (e->ResultCode == 0 && !IsEmpty)
            {
                e->CanBeDeleted = FALSE;
                e->ResultCode = ERROR_DIR_NOT_EMPTY;
            }
        }

        return e->ResultCode;
    }

    INT FireCloseDirectoryEnumeration(CBFSCloseDirectoryEnumerationEventParams* e) override
    {
        if (e->EnumerationContext != 0)
        {
            PENUM_INFO pInfo = (PENUM_INFO)(e->EnumerationContext);
            assert(NTC_DIRECTORY_ENUM == NodeType(pInfo));

            FindClose(pInfo->hFile);
            free(pInfo);
        }

        return 0;
    }

    INT FireCloseFile(CBFSCloseFileEventParams* e) override
    {
        // printf("OnCloseFile fired for %ws\n\n", e->FileName);
        if (e->FileContext != 0)
        {
            PFILE_CONTEXT Ctx = (PFILE_CONTEXT)(e->FileContext);

            assert(NTC_FILE_CONTEXT == NodeType(Ctx));
            assert(Ctx->OpenCount > 0);

            if (Ctx->OpenCount == 1)
            {
                if (Ctx->hFile != INVALID_HANDLE_VALUE)
                {
                    cache->FileCloseEx(e->FileName, cbcConstants::FLUSH_IMMEDIATE, cbcConstants::PURGE_NONE);
                    // We close the handle in FireCloseFile because this particular call to FileCloseEx is fully synchronous - it flushes and purges the cached data immediately. 
                    // If flushing is postponed, then a handler of the CBCache's WriteData event would have to check for the completion or cancellation of writing, and close the handle there. 
                    // Also, the order of calls is important - first, the file is closed in the cache, then the backend handle is closed. 
                    if (!CloseHandle(Ctx->hFile))
                    {
                        e->ResultCode = ::GetLastError();
                        return e->ResultCode;
                    }
                }
                free(Ctx);
            }
            else
                Ctx->OpenCount--;
        }

        return 0;
    }

    INT FireCreateFile(CBFSCreateFileEventParams* e) override
    {
        LPCWSTR FileName = e->FileName;
        LPWSTR FName = static_cast<LPWSTR>(malloc((wcslen(FileName) + wcslen(g_RootPath) + 1) * sizeof(WCHAR)));

        INT LastError = 0;

        if (FName == NULL)
        {
            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            return e->ResultCode;
        }

        wcscpy(FName, g_RootPath);
        wcscat(FName, FileName);

        PFILE_CONTEXT Ctx = static_cast<PFILE_CONTEXT>(malloc(sizeof(FILE_CONTEXT)));

        if (Ctx == NULL) {
            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            free(FName);
            return e->ResultCode;
        }

        FillMemory(Ctx, sizeof(FILE_CONTEXT), 0);

        Ctx->Header.NodeTypeCode = NTC_FILE_CONTEXT;
        Ctx->ReadOnly = FALSE;

        if (e->Attributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!CreateDirectory(FName, NULL))
            {
                goto error;
            }

            Ctx->hFile = (PVOID)CreateFile(
                FName,
                GENERIC_READ/* | FILE_WRITE_ATTRIBUTES*/,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS,
                0);

            if (INVALID_HANDLE_VALUE == Ctx->hFile)
            {
                goto error;
            }
        }
        else
        {
            Ctx->hFile = (PVOID)CreateFile(
                FName,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                CREATE_NEW,
                e->Attributes,
                0);

            if (INVALID_HANDLE_VALUE == Ctx->hFile || !SetFileAttributes(FName, e->Attributes & 0xffff)) // Attributes contains creation flags as well, which we need to strip)
            {
                goto error;
            }

            if (cache->FileOpenEx(e->FileName, 0, false, 0, 0, cbcConstants::PREFETCH_NOTHING, Ctx->hFile) != 0)
            {
                goto error;
            }
        }

        Ctx->OpenCount++;
        e->FileContext = Ctx;

        if (FName) {
            free(FName);
        }

        return 0;

    error:

        LastError = ::GetLastError();
        if (Ctx != NULL && Ctx->hFile != INVALID_HANDLE_VALUE)
            CloseHandle(Ctx->hFile);
        e->FileContext = NULL;
        if (Ctx != NULL)
            free(Ctx);
        if (FName != NULL)
            free(FName);
        FName = NULL;
        e->ResultCode = LastError;

        return e->ResultCode;
    }

    INT FireDeleteFile(CBFSDeleteFileEventParams* e) override
    {
        LPCWSTR FileName = e->FileName;
        LPWSTR FName = static_cast<LPWSTR>(malloc((wcslen(FileName) + wcslen(g_RootPath) + 1) * sizeof(WCHAR)));

        if (NULL == FName)
        {
            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            return e->ResultCode;
        }

        wcscpy(FName, g_RootPath);
        wcscat(FName, FileName);

        BOOL result = FALSE;
        DWORD attr = GetFileAttributes(FName);

        cache->FileDelete(FileName);

        if (attr & FILE_ATTRIBUTE_DIRECTORY)
        {
            result = RemoveDirectory(FName);
        }
        else
        {
            result = DeleteFile(FName);
        }

        DWORD error = result ? 0 : ::GetLastError();

        if (FName) free(FName);

        if (!result)
        {
            e->ResultCode = error;
            return error;
        }
        return 0;
    }

    INT FireEnumerateDirectory(CBFSEnumerateDirectoryEventParams* e) override
    {
        PENUM_INFO pInfo = NULL;

        if (e->Restart && (e->EnumerationContext != NULL))
        {
            pInfo = (PENUM_INFO)(e->EnumerationContext);

            FindClose(pInfo->hFile);

            free(pInfo);

            e->EnumerationContext = NULL;
        }

        if (NULL == e->EnumerationContext)
        {
            pInfo = static_cast<PENUM_INFO>(malloc(sizeof(ENUM_INFO)));

            if (NULL == pInfo)
            {
                e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
                return e->ResultCode;
            }

            ZeroMemory(pInfo, sizeof(ENUM_INFO));

            pInfo->Header.NodeTypeCode = NTC_DIRECTORY_ENUM;

            LPCWSTR DirName = e->DirectoryName;
            LPCWSTR Mask = e->Mask;
            LPWSTR FName = static_cast<LPWSTR>(malloc((wcslen(DirName) + wcslen(g_RootPath) + 1 + wcslen(Mask) + 1) * sizeof(WCHAR)));

            if (FName == NULL)
            {
                e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
                return e->ResultCode;
            }

            wcscpy(FName, g_RootPath);
            wcscat(FName, DirName);
            if (DirName[wcslen(DirName) - 1] != '\\')
                wcscat(FName, L"\\");

            wcscat(FName, Mask);

            pInfo->hFile = FindFirstFile(FName, &pInfo->finfo);

            free(FName);
            FName = NULL;

            e->FileFound = pInfo->hFile != INVALID_HANDLE_VALUE;

            e->EnumerationContext = pInfo;
        }
        else
        {
            pInfo = (PENUM_INFO)(e->EnumerationContext);

            e->FileFound = FindNextFile(pInfo->hFile, &pInfo->finfo);
        }

        while (e->FileFound && (!wcscmp(pInfo->finfo.cFileName, L".") || !wcscmp(pInfo->finfo.cFileName, L"..")))
        {
            e->FileFound = FindNextFile(pInfo->hFile, &pInfo->finfo);
        }

        if (e->FileFound)
        {
            // Copy the file name 
            e->ResultCode = CopyStringToBuffer(e->FileName, e->lenFileName, pInfo->finfo.cFileName);

            // Copy the short name
            if (e->ResultCode == 0)
                e->ResultCode = CopyStringToBuffer(e->ShortFileName, e->lenShortFileName, pInfo->finfo.cAlternateFileName);

            // Copy the rest of attributes
            if (e->ResultCode == 0)
            {
                *(e->pCreationTime) = FileTimeToInt64(pInfo->finfo.ftCreationTime);

                *(e->pLastAccessTime) = FileTimeToInt64(pInfo->finfo.ftLastAccessTime);

                *(e->pLastWriteTime) = FileTimeToInt64(pInfo->finfo.ftLastWriteTime);

                *(e->pSize) = ((__int64)pInfo->finfo.nFileSizeHigh << 32 & 0xFFFFFFFF00000000) | pInfo->finfo.nFileSizeLow;

                // Try to obtain file size from the cache
                WCHAR NameBuf[MAX_PATH + 1];
                wcscpy(NameBuf, e->DirectoryName);
                wcscat(NameBuf, L"\\");
                wcsncat(NameBuf, e->FileName, e->lenFileName);
                if (cache->FileExists(NameBuf))
                    *(e->pSize) = cache->FileGetSize(NameBuf);

                *(e->pAllocationSize) = *(e->pSize);

                *(e->pAttributes) = pInfo->finfo.dwFileAttributes;

                *(e->pFileId) = 0;

                if ((*(e->pAttributes) & FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT)
                    *(e->pReparseTag) = pInfo->finfo.dwReserved0;
            }
            else
                if (e->EnumerationContext != NULL)
                {
                    FindClose(pInfo->hFile);

                    free(pInfo);

                    e->EnumerationContext = NULL;
                }

            return e->ResultCode;
        }

        return e->ResultCode;
    }

    INT FireGetFileInfo(CBFSGetFileInfoEventParams* e) override
    {
        BY_HANDLE_FILE_INFORMATION fi;
        HANDLE hFile, hFile2;
        DWORD error;

        static PfnNtQueryInformationFile NtQueryInformationFile = 0;

        if (0 == NtQueryInformationFile)
        {
            NtQueryInformationFile = (PfnNtQueryInformationFile)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryInformationFile");
        }

        assert(NtQueryInformationFile);

        LPCWSTR FileName = e->FileName;

        LPWSTR FName = (LPWSTR)malloc((wcslen(FileName) + wcslen(g_RootPath) + 1) * sizeof(WCHAR));
        if (NULL == FName)
        {
            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            return e->ResultCode;
        }

        wcscpy(FName, g_RootPath);
        wcscat(FName, FileName);

        if (L'\\' == FName[wcslen(FName) - 1]) {

            FName[wcslen(FName) - 1] = '\0';
        }

        // Open the original file to retrieve its metadata
        hFile = CreateFile(
            FName,
            READ_CONTROL,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
            NULL);

        error = ::GetLastError();

        if (hFile != INVALID_HANDLE_VALUE) {

            e->FileExists = TRUE;

            //
            // Try to get the short file name
            //

            NTSTATUS status;
            IO_STATUS_BLOCK iosb;
            WCHAR buffer[500];
            PFILE_NAME_INFORMATION fni = (PFILE_NAME_INFORMATION)buffer;

            if (e->lenShortFileName > 0)
            {
                e->ShortFileName[0] = 0;

                status = NtQueryInformationFile(hFile,
                    &iosb,
                    fni,
                    sizeof(buffer),
                    21/*FileAlternateNameInformation*/);

                if (status == 0)
                    e->ResultCode = CopyStringToBuffer(e->ShortFileName, e->lenShortFileName, fni->FileName, fni->FileNameLength / sizeof(WCHAR));
                else
                {
                    e->lenShortFileName = 0;
                    //ShortFileName is not supported.
                }
            } // if

            //
            // Try to get the real file name
            //
            if (e->ResultCode == 0) {

                status = NtQueryInformationFile(hFile,
                    &iosb,
                    fni,
                    sizeof(buffer),
                    9/*FileNameInformation*/);

                if (status == 0) {

                    if (e->lenRealFileName > 0)
                        e->RealFileName[0] = 0;

                    LPWSTR AName = static_cast<LPWSTR>(malloc((fni->FileNameLength + 1) * sizeof(WCHAR)));
                    size_t len = fni->FileNameLength / sizeof(WCHAR);
                    wcsncpy(AName, fni->FileName, len);
                    AName[len] = 0;

                    PWCHAR realFileName = wcsrchr(AName, L'\\');

                    if (realFileName != NULL)
                    {
                        realFileName += 1;
                        e->ResultCode = CopyStringToBuffer(e->RealFileName, e->lenRealFileName, realFileName);
                    }
                    else
                        e->lenRealFileName = 0;

                    if (AName) free(AName);
                    AName = NULL;
                }
                else
                {
                    e->lenRealFileName = 0;
                    e->ResultCode = status;
                }
            }

            wchar_t* sp, * ep = NULL;

            // Query file attributes
            if (!GetFileInformationByHandle(hFile, &fi))
            {
                e->ResultCode = ::GetLastError();
            }
            else
            {
                *(e->pAttributes) = fi.dwFileAttributes;
                *(e->pCreationTime) = FileTimeToInt64(fi.ftCreationTime);
                *(e->pLastAccessTime) = FileTimeToInt64(fi.ftLastAccessTime);
                *(e->pLastWriteTime) = FileTimeToInt64(fi.ftLastWriteTime);
                *(e->pSize) = ((__int64)fi.nFileSizeHigh << 32 & 0xFFFFFFFF00000000) | fi.nFileSizeLow;

                if (cache->FileExists(e->FileName))
                    *(e->pSize) = cache->FileGetSize(e->FileName);

                *(e->pAllocationSize) = *(e->pSize);

                e->HardLinkCount = fi.nNumberOfLinks;

                *(e->pFileId) = 0;
            }
            CloseHandle(hFile);
        }
        else
        {
            if (error == ERROR_FILE_NOT_FOUND)
                e->FileExists = FALSE;
            else
                e->ResultCode = error;
        }
        free(FName);
        return e->ResultCode;
    }

    INT FireGetFileNameByFileId(CBFSGetFileNameByFileIdEventParams* e) override
    {
        OBJECT_ATTRIBUTES ObjectAttributes;
        UNICODE_STRING Name;
        IO_STATUS_BLOCK Iosb;
        HANDLE RootHandle;
        HANDLE FileHandle;
        DWORD RootPathLength;
        DWORD LastError = ERROR_SUCCESS;
        PFILE_NAME_INFO NameInfo = NULL;
        BOOL b;

        static PfnGetFileInformationByHandleEx GetFileInformationByHandleEx = 0;
        static PfnNtOpenFile NtOpenFile = 0;

        if (GetFileInformationByHandleEx == 0)
        {
            GetFileInformationByHandleEx = (PfnGetFileInformationByHandleEx)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "GetFileInformationByHandleEx");
        }

        if (NtOpenFile == 0)
        {
            NtOpenFile = (PfnNtOpenFile)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtOpenFile");
            assert(NtOpenFile != 0);
        }

        RootPathLength = (DWORD)wcslen(g_RootPath);

        RootHandle = CreateFile(g_RootPath,
            0,
            FILE_SHARE_WRITE | FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL);

        assert(RootHandle != INVALID_HANDLE_VALUE);

        Name.Length = Name.MaximumLength = 8;
        Name.Buffer = (PWSTR)&e->FileId;


        InitializeObjectAttributes(&ObjectAttributes,
            &Name,
            0,
            RootHandle,
            NULL);

        LastError = LsaNtStatusToWinError(NtOpenFile(&FileHandle,
            GENERIC_READ,
            &ObjectAttributes,
            &Iosb,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0x00002000 | 0x00004000 //FILE_OPEN_BY_FILE_ID | FILE_OPEN_FOR_BACKUP_INTENT
        ));
        CloseHandle(RootHandle);

        if (LastError != ERROR_SUCCESS)
        {
            e->ResultCode = ERROR_FILE_NOT_FOUND;
            return e->ResultCode;
        }

        NameInfo = (PFILE_NAME_INFO)malloc(512);

        if (NULL == NameInfo)
        {
            CloseHandle(FileHandle);
            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            return e->ResultCode;
        }

        if (NULL == GetFileInformationByHandleEx)
        {
            CloseHandle(FileHandle);
            free(NameInfo);
            e->ResultCode = ERROR_PROC_NOT_FOUND;
            return e->ResultCode;
        }

        b = GetFileInformationByHandleEx(FileHandle, FileNameInfo, NameInfo, 512);

        LastError = ::GetLastError();

        CloseHandle(FileHandle);
        free(NameInfo);

        if (!b)
        {
            e->ResultCode = LastError;
            return LastError;
        }

        assert(NameInfo->FileNameLength / sizeof(WCHAR) > RootPathLength - (sizeof("X:") - 1));

        e->ResultCode = CopyStringToBuffer(
            e->FilePath,
            e->lenFilePath,
            &(NameInfo->FileName[RootPathLength]),
            NameInfo->FileNameLength / sizeof(WCHAR) - (RootPathLength - (sizeof("X:") - 1))
        );

        return e->ResultCode;
    }

    INT FireGetVolumeId(CBFSGetVolumeIdEventParams* e) override
    {
        e->VolumeId = 0x12345678;
        return 0;
    }

    INT FireGetVolumeLabel(CBFSGetVolumeLabelEventParams* e) override
    {
        const wchar_t* wideLabel = L"Folder Drive";
        size_t wideLabelLength = wcslen(wideLabel) + 1;
        LPWSTR Label = new WCHAR[wideLabelLength];
        wcscpy_s(Label, wideLabelLength, wideLabel);
        e->ResultCode = CopyStringToBuffer(e->Buffer, e->lenBuffer, Label);
        return e->ResultCode;
    }

    INT FireGetVolumeSize(CBFSGetVolumeSizeEventParams* e) override
    {
        DISKQUOTA_USER_INFORMATION dqui = { 0,-1,-1 };

        __int64 FreeBytesToCaller, TotalBytes, FreeBytes;

        if (!GetDiskFreeSpaceEx(g_RootPath,
            (PULARGE_INTEGER)&FreeBytesToCaller,
            (PULARGE_INTEGER)&TotalBytes,
            (PULARGE_INTEGER)&FreeBytes))
        {
            e->ResultCode = ::GetLastError();
            return e->ResultCode;
        }

        INT SectorSize = _wtoi(Config(L"SectorSize"));
        *(e->pAvailableSectors) = FreeBytes / SectorSize;
        *(e->pTotalSectors) = TotalBytes / SectorSize;

        return 0;
    }

    INT FireIsDirectoryEmpty(CBFSIsDirectoryEmptyEventParams* e) override
    {
        return (e->ResultCode = IsEmptyDirectory(e->DirectoryName, e->IsEmpty));
    }

    BOOL IsWriteRightForAttrRequested(DWORD acc)
    {
        DWORD writeRight = FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA;
        return ((acc & writeRight) == 0) && ((acc && FILE_WRITE_ATTRIBUTES) != 0);
    }

    BOOL IsReadRightForAttrOrDeleteRequested(DWORD acc)
    {
        DWORD readRight = FILE_READ_DATA | FILE_READ_EA;
        DWORD readAttrOrDeleteRight = FILE_READ_ATTRIBUTES | 0x10000 /* delete */;
        return ((acc & readRight) == 0) && ((acc && readAttrOrDeleteRight) != 0);
    }

    INT FireOpenFile(CBFSOpenFileEventParams* e) override
    {
        PFILE_CONTEXT Ctx = NULL;
        DWORD error = NO_ERROR;
        BOOL  bSelfAllocated = FALSE;

        if (e->FileContext != NULL)
        {
            Ctx = (PFILE_CONTEXT)(e->FileContext);
            assert(NTC_FILE_CONTEXT == NodeType(Ctx));
            if (Ctx->ReadOnly && (e->DesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)))
            {
                e->ResultCode = ERROR_ACCESS_DENIED;
                return e->ResultCode;
            }
            Ctx->OpenCount++;
            return 0;
        }
        else
        {
            Ctx = (PFILE_CONTEXT)malloc(sizeof(FILE_CONTEXT));
            if (Ctx == NULL)
            {
                e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
                return e->ResultCode;
            }

            bSelfAllocated = TRUE;
            FillMemory(Ctx, sizeof(FILE_CONTEXT), 0);
            Ctx->Header.NodeTypeCode = NTC_FILE_CONTEXT;
            Ctx->ReadOnly = FALSE;
            Ctx->hFile = INVALID_HANDLE_VALUE;
        }

        LPCWSTR FileName = e->FileName;

        LPWSTR FullName = GetDiskName(e->FileName);
        if (NULL == FullName)
        {
            if (bSelfAllocated)
                free(Ctx);
            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            return e->ResultCode;
        }

        //DWORD FileAttributes = GetFileAttributes(FName);

        {
            BOOL directoryFile = (e->Attributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
            BOOL reparsePoint = (e->Attributes & FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT;
            BOOL openAsReparsePoint = ((e->Attributes & FILE_FLAG_OPEN_REPARSE_POINT) == FILE_FLAG_OPEN_REPARSE_POINT);

            INT AddOpenFlag = (reparsePoint && openAsReparsePoint && this->GetUseReparsePoints()) ? FILE_FLAG_OPEN_REPARSE_POINT : 0;

            if (e->Attributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (wcscmp(FileName, _T("\\")) &&
                    (0xFFFFFFFF == GetFileAttributes(FullName)))
                {
                    if (bSelfAllocated)
                        free(Ctx);

                    free(FullName);
                    FullName = NULL;
                    e->ResultCode = ERROR_FILE_NOT_FOUND;
                    return e->ResultCode;
                }

            }
            else
            {
                Ctx->hFile = (PVOID)CreateFile(
                    FullName,
                    GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL,
                    OPEN_EXISTING,
                    AddOpenFlag,
                    0);

                if (INVALID_HANDLE_VALUE == Ctx->hFile)
                {
                    error = ::GetLastError();

                    if (ERROR_ACCESS_DENIED == error)
                    {
                        if (!IsReadRightForAttrOrDeleteRequested(e->DesiredAccess))
                        {
                            if (IsWriteRightForAttrRequested(e->DesiredAccess) || !(e->DesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)))
                            {
                                Ctx->hFile = (PVOID)CreateFile(
                                    FullName,
                                    GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    NULL,
                                    OPEN_EXISTING,
                                    AddOpenFlag,
                                    0);

                                Ctx->ReadOnly = TRUE;

                                if (INVALID_HANDLE_VALUE == Ctx->hFile)
                                {
                                    e->ResultCode = ::GetLastError();
                                    if (bSelfAllocated)
                                        free(Ctx);

                                    free(FullName);
                                    FullName = NULL;
                                    return e->ResultCode;
                                }
                            }
                        }
                    }
                    else
                    {
                        if (bSelfAllocated)
                            free(Ctx);

                        free(FullName);
                        FullName = NULL;

                        e->ResultCode = error;
                        return error;
                    }
                }
                // Open the file in the cache
                LARGE_INTEGER pos;
                pos.QuadPart = 0;

                LARGE_INTEGER CurrentPosition;

                // Determine file length
                ::SetFilePointerEx(Ctx->hFile, pos, &CurrentPosition, FILE_END);
                ::SetFilePointerEx(Ctx->hFile, pos, NULL, FILE_BEGIN);

                cache->FileOpen(e->FileName, CurrentPosition.QuadPart, cbcConstants::PREFETCH_NOTHING, Ctx->hFile);
            }
        }

        if ((e->NTCreateDisposition != FILE_OPEN) && (e->NTCreateDisposition != FILE_OPEN_IF))
            SetFileAttributes(FullName, e->Attributes & 0xffff); // Attributes contains creation flags as well, which we need to strip

        Ctx->OpenCount++;
        e->FileContext = Ctx;

        if (FullName) {
            free(FullName);
        }

        return 0;
    }

    INT FireReadFile(CBFSReadFileEventParams* e) override
    {
        if (cache->FileRead(e->FileName, e->Position, e->Buffer, e->BytesToRead, 0, e->BytesToRead))
            *(e->pBytesRead) = e->BytesToRead;
        else
        {
            e->ResultCode = ERROR_DEVICE_HARDWARE_ERROR;
            *(e->pBytesRead) = 0;
        }
        return e->ResultCode;
    }

    INT FireRenameOrMoveFile(CBFSRenameOrMoveFileEventParams* e) override
    {
        LPCWSTR FileName = e->FileName;
        LPWSTR OldName = (LPWSTR)malloc((wcslen(FileName) + wcslen(g_RootPath) + 1) * sizeof(WCHAR));
        if (NULL == OldName)
        {
            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            return e->ResultCode;
        }

        wcscpy(OldName, g_RootPath);
        wcscat(OldName, FileName);

        LPCWSTR NewFileName = e->NewFileName;
        LPWSTR NewName = (LPWSTR)malloc((wcslen(NewFileName) + wcslen(g_RootPath) + 1) * sizeof(WCHAR));
        if (NULL == NewName) {

            if (OldName) free(OldName);

            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            return e->ResultCode;
        }

        wcscpy(NewName, g_RootPath);
        wcscat(NewName, NewFileName);

        BOOL result = MoveFileEx(OldName, NewName, MOVEFILE_REPLACE_EXISTING);
        DWORD error = result ? 0 : ::GetLastError();

        if (error == 0)
        {
            if (cache->FileExists(e->NewFileName))
            {
                cache->FileDelete(e->NewFileName);
            }
            cache->FileChangeId(e->FileName, e->NewFileName);
        }

        if (OldName) free(OldName);
        if (NewName) free(NewName);

        if (!result)
        {
            e->ResultCode = error;
            return error;
        }

        return 0;
    }

    INT FireSetAllocationSize(CBFSSetAllocationSizeEventParams* e) override
    {
        return 0;
    }

    INT FireSetFileSize(CBFSSetFileSizeEventParams* e) override
    {
        DWORD error;
        LONG OffsetHigh;

        PFILE_CONTEXT Ctx = (PFILE_CONTEXT)(e->FileContext);
        assert(Ctx);

        cache->FileSetSize(e->FileName, e->Size, FALSE);

        OffsetHigh = (LONG)((e->Size >> 32) & 0xFFFFFFFF);

        if ((DWORD)-1 == SetFilePointer(Ctx->hFile, (LONG)(e->Size & 0xFFFFFFFF), &OffsetHigh, FILE_BEGIN))
        {
            error = ::GetLastError();

            if (error != ERROR_SUCCESS)
            {
                e->ResultCode = error;
                return error;
            }
        }

        if (!SetEndOfFile(Ctx->hFile))
        {
            e->ResultCode = ::GetLastError();
            return e->ResultCode;
        }

        return 0;
    }

    INT FireSetFileAttributes(CBFSSetFileAttributesEventParams* e) override
    {
        DWORD error = NO_ERROR;
        PFILE_CONTEXT Ctx;
        HANDLE hFile = INVALID_HANDLE_VALUE;

        Ctx = (PFILE_CONTEXT)(e->FileContext);
        assert(Ctx);

        LPCWSTR FileName = e->FileName;
        LPWSTR FName = (LPWSTR)malloc((wcslen(FileName) + wcslen(g_RootPath) + 1) * sizeof(WCHAR));
        if (NULL == FName)
        {
            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            return e->ResultCode;
        }

        wcscpy(FName, g_RootPath);
        wcscat(FName, FileName);

        // the case when Attributes == 0 indicates that file attributes
        // not changed during this callback
        DWORD Attr = GetFileAttributes(FName);
        if (e->Attributes != 0)
        {
            if (!SetFileAttributes(FName, e->Attributes))
            {
                error = ::GetLastError();
                goto end;
            }
        }

        if (Attr == INVALID_FILE_ATTRIBUTES)
        {
            error = ::GetLastError();
            goto end;
        }

        if ((Attr & FILE_ATTRIBUTE_READONLY) != 0)
        {
            if (!SetFileAttributes(FName, Attr & ~FILE_ATTRIBUTE_READONLY))
            {
                error = ::GetLastError();
                goto end;
            }
        }

        hFile = (PVOID)CreateFile(
            FName,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            ((Attr & FILE_ATTRIBUTE_DIRECTORY) != 0) ? FILE_FLAG_BACKUP_SEMANTICS : 0,
            0);

        if (INVALID_HANDLE_VALUE == hFile)
        {
            error = ::GetLastError();
            goto end;
        }

        FILETIME CreationTime, LastAccessTime, LastWriteTime;
        Int64ToFileTime(e->CreationTime, &CreationTime);
        Int64ToFileTime(e->LastAccessTime, &LastAccessTime);
        Int64ToFileTime(e->LastWriteTime, &LastWriteTime);
        if (!SetFileTime(hFile, &CreationTime, &LastAccessTime, &LastWriteTime))
        {
            error = ::GetLastError();
            goto end;
        }

        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;

        if ((Attr & FILE_ATTRIBUTE_READONLY) != 0)
        {
            if (!SetFileAttributes(FName, Attr))
            {
                error = ::GetLastError();
                goto end;
            }
        }
    end:
        if (FName) free(FName);
        if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);

        if (error != NO_ERROR)
        {
            e->ResultCode = error;
            return error;
        }

        return 0;
    }

    INT FireWriteFile(CBFSWriteFileEventParams* e) override
    {
        printf("OnWriteFile fired for %ws\n\n", e->FileName);

        if (cache->FileWrite(e->FileName, e->Position, (char*)e->Buffer, e->BytesToWrite, 0, e->BytesToWrite))
            *(e->pBytesWritten) = e->BytesToWrite;
        else
        {
            *(e->pBytesWritten) = 0;
            e->ResultCode = ERROR_DEVICE_HARDWARE_ERROR;
        }
        return 0;
    }
};

const LPCWSTR program_name = L"{713CC6CE-B3E2-4fd9-838D-E28F558F6866}";

const LPCWSTR strSuccess = L"Success";
const LPCSTR strInvalidOption = "Invalid option \"%s\"\n";

//-----------------------------------------------------------------------------------------------------------

int optcmp(char* arg, const char* opt)
{
    while (1)
    {
        if (*arg >= 'A' && *arg <= 'Z')
            *arg = *arg - 'A' + 'a';
        if (*arg != *opt)
            return 0;
        if (*arg == 0)
            return 1;
        arg++;
        opt++;
    }
}

void banner(void)
{
    printf("CBFS Connect Copyright (c) Callback Technologies, Inc.\n");
    printf("This demo shows how to mount a folder on your system as a drive and use CBCache for on-disk caching.\n\n");
}

void usage(void)
{
    printf("Usage: cachefolderdrive [-<switch 1> ... -<switch N>] <root path> <mounting point>\n\n");
    printf("<Switches>\n");
    printf("  -i icon_path - Set volume icon\n");
#ifdef WIN32
    printf("  -lc - Mount for current user only\n");
#endif
    printf("  -n - Mount as network volume\n");
#ifdef WIN32
    printf("  -drv cab_file - Install drivers from CAB file\n");
#endif
    printf("  -- Stop switches scanning\n\n");
    printf("Example: cachefolderdrive C:\\Root Y:\n\n");
}

// ----------------------------------------------------------------------------------

CacheFolderDriveCBFS cbfs;
CacheFolderDriveCBCache cache;

#ifdef WIN32
#define R_OK 0
#define access(filename, mode) (GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES ? 0 : 1)

void check_driver(INT module, LPCWSTR module_name)
{
    INT state;

    state = cbfs.GetDriverStatus(program_name, module);
    if (state == SERVICE_RUNNING)
    {
        LONG64 version;
        version = cbfs.GetModuleVersion(program_name, module);
        printf("%ws driver is installed, version: %d.%d.%d.%d\n", module_name,
            (INT)((version & 0x7FFF000000000000) >> 48),
            (INT)((version & 0xFFFF00000000) >> 32),
            (INT)((version & 0xFFFF0000) >> 16),
            (INT)(version & 0xFFFF));
    }
    else
    {
        printf("%ws driver is not installed\n", module_name);
        exit(0);
    }
}
#endif

LPWSTR a2w(char* source)
{
    LPWSTR result = NULL;
    if (source == NULL)
    {
        result = (LPWSTR)malloc(sizeof(WCHAR));
        result[0] = 0;
        return result;
    }
    else
    {
        int wstrLen = MultiByteToWideChar(CP_ACP, 0, source, -1, NULL, 0);
        if (wstrLen > 0)
        {
            result = (LPWSTR)malloc((wstrLen + 1) * sizeof(WCHAR));

            if (MultiByteToWideChar(CP_ACP, 0, source, -1, result, wstrLen) == 0)
                return NULL;
            else
                return result;
        }
        else
            return NULL;
    }
}

bool IsDriveLetter(const cbt_string& path) {
    if (path.empty())
        return false;

    wchar_t c = path[0];
    if (((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z')) && path.size() == 2 && path[1] == L':')
        return true;
    else
        return false;
}

bool IsAbsolutePath(const cbt_string& path) {
    if (path.empty()) {
        return false;
    }

#ifdef _WIN32
    // On Windows, check if the path starts with a drive letter followed by a colon and a separator
    if ((path.size() >= 3) && iswalpha(path[0]) && (path[1] == L':') && (path[2] == L'\\' || path[2] == L'/')) {
        return true;
    }

    // Check for UNC paths (e.g., \\server\share)
    if (path.size() >= 2 && (path[0] == L'\\') && (path[1] == L'\\')) {
        return true;
    }
#else
    // On Linux and Unix, check if the path starts with a '/'
    if (path[0] == _T('/')) {
        return true;
    }
#endif

    return false;
}

cbt_string ConvertRelativePathToAbsolute(const cbt_string& path, bool acceptMountingPoint = false) {
    cbt_string res;

    if (!path.empty()) {
        res = path;

#ifndef _WIN32
        // Linux/Unix-specific case of using a home directory
        if (path == "~" || path.find("~/") == 0) {
            const char* homeDir = getenv("HOME");

            if (path == "~") {
                return homeDir ? homeDir : "";
            }
            else {
                return homeDir ? cbt_string(homeDir) + path.substr(1) : "";
            }
        }
#else
        size_t semicolonCount = std::count(path.begin(), path.end(), ';');
        bool isNetworkMountingPoint = semicolonCount == 2;
        if (isNetworkMountingPoint) {
            if (!acceptMountingPoint) {
                sout << L"The path '" << path << L"' format cannot be equal to the Network Mounting Point" << std::endl;
                return path;
            }
            size_t pos = path.find(L";");
            if (pos != cbt_string::npos) {
                res = path.substr(0, pos);
                if (res.empty()) {
                    return path;
                }
            }
        }

#endif
        if (!IsAbsolutePath(res)) {
#ifdef _WIN32
            if (IsDriveLetter(res)) {
                if (!acceptMountingPoint) {
                    sout << L"The path '" << res << L"' format cannot be equal to the Drive Letter" << std::endl;
                }
                return path;
            }
            wchar_t currentDir[_MAX_PATH];
            const char pathSeparator = '\\';
            if (_wgetcwd(currentDir, _MAX_PATH) == nullptr) {
                sout << "Error getting current directory." << std::endl;
                return L"";
            }
#else
            char currentDir[PATH_MAX];
            const char pathSeparator = '/';
            if (getcwd(currentDir, sizeof(currentDir)) == nullptr) {
                sout << "Error getting current directory." << std::endl;
                return "";
            }
#endif
            cbt_string currentDirStr(currentDir);

            // Ensure that the current directory has a trailing backslash
            if (currentDirStr.back() != pathSeparator) {
                currentDirStr += pathSeparator;
            }

            return currentDirStr + path;
        }
    }
    else {
        sout << L"Error: The input path is empty." << std::endl;
    }
    return path;
}

int main(int argc, char* argv[]) {
#ifndef WIN32
    struct pollfd cinfd[1];
    INT count;
    int opt_debug = 0;
#else
    LPWSTR cab_file = NULL;
    INT drv_reboot = 0;
    INT opt_terminate = 0;
#endif
    INT opt_password_size = 0;
    LPCWSTR opt_icon_path = NULL, root_path = NULL, mount_point = NULL;
    INT argi, arg_len,
        stop_opt = 0,
        mounted = 0, opt_network = 0, opt_local = 0;
    INT flags = 0;

    banner();
    if (argc < 3) {
        usage();
#ifdef WIN32
        check_driver(cbcConstants::MODULE_DRIVER, L"CBFS");
#endif
        return 0;
    }
    int retVal;

    for (argi = 1; argi < argc; argi++) {
        arg_len = (INT)strlen(argv[argi]);
        if (arg_len > 0) {
            if ((argv[argi][0] == '-') && !stop_opt) {
                if (arg_len < 2)
                    fprintf(stderr, strInvalidOption, argv[argi]);
                else {
                    if (optcmp(argv[argi], "--"))
                        stop_opt = 1;
                    else if (optcmp(argv[argi], "-i")) {
                        argi++;
                        if (argi < argc) {
                            cbt_string opt_icon_path_wstr = ConvertRelativePathToAbsolute(a2w(argv[argi]));
                            opt_icon_path = wcsdup(opt_icon_path_wstr.c_str());
                        }
                    }
#ifdef WIN32
                    else if (optcmp(argv[argi], "-lc"))
                        opt_local = 1;
#endif
                    else if (optcmp(argv[argi], "-n"))
                        opt_network = 1;
#ifdef WIN32
                    else if (optcmp(argv[argi], "-drv")) {
                        argi++;
                        if (argi < argc) {
                            printf("Installing drivers from '%s'\n", argv[argi]);
                            cbt_string driver_path_wstr = ConvertRelativePathToAbsolute(a2w(argv[argi]));
                            LPCWSTR driver_path = wcsdup(driver_path_wstr.c_str());
                            drv_reboot = cbfs.Install(driver_path, program_name, NULL,
                                cbcConstants::MODULE_DRIVER | cbcConstants::MODULE_HELPER_DLL,
                                0);

                            retVal = cbfs.GetLastErrorCode();
                            if (0 != retVal) {
                                if (retVal == ERROR_PRIVILEGE_NOT_HELD)
                                    fprintf(stderr, "Drivers are not installed due to insufficient privileges. Please, run installation with administrator rights");
                                else
                                    fprintf(stderr, "Drivers are not installed, error %s", cbfs.GetLastError());

                                return retVal;
                            }

                            printf("Drivers installed successfully");
                            if (drv_reboot != 0) {
                                printf(", reboot is required\n");
                                exit(0);
                            }
                            else {
                                printf("\n");
                                exit(0);
                            }
                        }
                    }
#endif
                    else
                        fprintf(stderr, strInvalidOption, argv[argi]);
                }
            }
            else {

                int retVal = cbfs.Initialize(program_name);
                if (0 != retVal) {
                    fprintf(stderr, "Error: %s", cbfs.GetLastError());
                    return retVal;
                }

                if (opt_icon_path) {
                    retVal = cbfs.RegisterIcon(opt_icon_path, program_name, opt_icon_path);
                    if (0 != retVal) {
                        fprintf(stderr, "Error: %s", cbfs.GetLastError());
                        return retVal;
                    }
                }

                cbt_string root_path_wstr = ConvertRelativePathToAbsolute(a2w(argv[argi++]));
                root_path = wcsdup(root_path_wstr.c_str());
                if (argi < argc) {
                    cbt_string mount_point_wstr = ConvertRelativePathToAbsolute(a2w(argv[argi]), true);
                    mount_point = wcsdup(mount_point_wstr.c_str());
                }

                // Setup the cache
                TCHAR CacheDir[MAX_PATH + 10];

                if (!::GetCurrentDirectory(MAX_PATH + 9, &CacheDir[0])) {
                    fprintf(stderr, "Error %d when retrieving current directory", ::GetLastError());
                    return 1;
                }
                _tcscat(CacheDir, _T("\\cache"));

                DWORD attr = GetFileAttributes(CacheDir);

                if (attr == INVALID_FILE_ATTRIBUTES) {
                    if (!CreateDirectory(CacheDir, NULL)) {
                        fprintf(stderr, "Error %d when creating directory '%s'", ::GetLastError(), CacheDir);
                        return 1;
                    }
                }
                else
                    if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                        fprintf(stderr, "'%s' already exists and is not a directory", CacheDir);
                        return 1;
                    }

                cache.SetCacheDirectory(CacheDir);
                cache.SetCacheFile(_T("cache.svlt"));
                cache.SetReadingCapabilities(cbcConstants::RWCAP_POS_RANDOM | cbcConstants::RWCAP_SIZE_ANY);
                cache.SetResizingCapabilities(cbcConstants::RSZCAP_GROW_TO_ANY | cbcConstants::RSZCAP_SHRINK_TO_ANY | cbcConstants::RSZCAP_TRUNCATE_AT_ZERO);
                cache.SetWritingCapabilities(cbcConstants::RWCAP_POS_RANDOM | cbcConstants::RWCAP_SIZE_ANY | cbcConstants::RWCAP_WRITE_KEEPS_FILESIZE);
                int cacheOpenResult = cache.OpenCache(_T(""), FALSE);
                if (cacheOpenResult != 0) {
                    fprintf(stderr, "Error %d was reported during the attempt to open the cache", cacheOpenResult);
                    return cacheOpenResult;
                }

                cbfs.SetCache(&cache); // this is the sample's method, not an API one

                // Setup CBFS
                cbfs.SetFileCache(0);
                cbfs.Config(_T("FileCachePolicyWriteThrough=1"));
                cbfs.SetFileSystemName(L"NTFS");

                cbfs.SetUseWindowsSecurity(FALSE);
                cbfs.SetUseReparsePoints(FALSE);
                cbfs.SetUseHardLinks(FALSE);
                cbfs.SetUseFileIds(FALSE);

                // Do the job
                retVal = cbfs.CreateStorage();
                if (0 != retVal) {
                    fprintf(stderr, "Error: %s", cbfs.GetLastError());
                    return retVal;
                }

                HANDLE hDir = INVALID_HANDLE_VALUE;

                wcscpy(g_RootPath, root_path);
                int len = (int)wcslen(g_RootPath);

                if ((g_RootPath[len - 1] == '\\') || (g_RootPath[len - 1] == '/')) {
                    g_RootPath[len - 1] = '\0';
                }

                hDir = CreateFile(g_RootPath,
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS,
                    NULL
                );

                if (INVALID_HANDLE_VALUE == hDir) {
                    if (!CreateDirectory(g_RootPath, NULL)) {
                        fprintf(stderr, "Cannot create root directory, invalid path name: %d", GetLastError());
                        return 1;
                    }
                }
                else {
                    CloseHandle(hDir);
                }

                retVal = cbfs.MountMedia(0);
                if (0 == retVal)
                    printf("Media inserted in storage\n");
                else {
                    fprintf(stderr, "Error: %s", cbfs.GetLastError());
                    return retVal;
                }

#ifdef WIN32
                if (opt_local)
                    flags |= cbcConstants::STGMP_LOCAL;
				else
#endif
                if (opt_network)
                    flags = cbcConstants::STGMP_NETWORK;
#ifdef WIN32
                else
                    flags |= cbcConstants::STGMP_MOUNT_MANAGER;
#else
                else
                    flags |= cbcConstants::STGMP_SIMPLE;
#endif
                if (cbfs.AddMountingPoint(mount_point, flags, 0) == 0)
                    mounted = 1;

                break;
            } // if/else
        } // if
    }

    if ((root_path == NULL) || (wcslen(root_path) == 0))
        fprintf(stderr, "CacheFolderDrive: Invalid parameters, root path not specified\n");
    else
        if (!mounted) {
            if (mount_point == NULL)
                fprintf(stderr, "CacheFolderDrive: Invalid parameters, mounting point not specified\n");
            else
                fprintf(stderr, "CacheFolderDrive: Failed to mount the disk\n");
        }
        else {
            printf("Press Enter to unmount disk\n");
#ifndef WIN32
            cinfd[0].fd = fileno(stdin);
            cinfd[0].events = POLLIN;
            while (1) {
                if (poll(cinfd, 1, 1000)) {
#else
            getc(stdin);
#endif

            printf("Unmounting mounting point\n");
            retVal = cbfs.RemoveMountingPoint(-1, mount_point, flags, 0);
            if (0 != retVal)
                fprintf(stderr, "Error: %s", cbfs.GetLastError());

            retVal = cbfs.UnmountMedia(TRUE);
            if (0 != retVal)
                fprintf(stderr, "Error: %s", cbfs.GetLastError());

            printf("Unmount done\n");

            int retVal = cbfs.DeleteStorage(true);
            if (0 != retVal)
                fprintf(stderr, "Error: %s", cbfs.GetLastError());

            cache.CloseCache(cbcConstants::FLUSH_IMMEDIATE, cbcConstants::PURGE_IMMEDIATE);

#ifndef WIN32
            break;
                }
    else {
        count = cbfs.GetMountingPointCount();
        if (count == 0) {
            printf("Disk is unmounted\n");
            break;
        }
    }
            }
#endif
        }

        return 0;
}

//-----------------------------------------------------------------------------------------------------------

BOOL
WIN32_FROM_HRESULT(
    IN HRESULT hr,
    OUT DWORD* Win32
)
{
    if ((hr & (HRESULT)0xFFFF0000) == MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, 0)) {

        // Could have come from many values, but we choose this one
        *Win32 = HRESULT_CODE(hr);
        return TRUE;
    }

    if (hr == S_OK) {

        *Win32 = HRESULT_CODE(hr);
        return TRUE;
    }

    //
    // We got an impossible value
    //
    return FALSE;
}

HANDLE
OpenToken(
    DWORD DesiredAccess
)
{
    HANDLE TokenHandle;

    if (!OpenProcessToken(GetCurrentProcess(),
        DesiredAccess,
        &TokenHandle)) {

        return NULL;
    }

    return TokenHandle;
}


BOOL
EnablePrivilege(
    LPCWSTR Privilege
)
{
    HANDLE tokenHandle;
    TOKEN_PRIVILEGES tp;
    LUID Luid;
    BOOL success;
    DWORD lastError;

    tokenHandle = OpenToken(TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES);
    if (tokenHandle == NULL)
        return FALSE;

    if (!LookupPrivilegeValue(NULL,
        Privilege,
        &Luid)) {

        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = Luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    success = AdjustTokenPrivileges(tokenHandle,
        FALSE,
        &tp,
        0,
        NULL,
        NULL);

    lastError = GetLastError();
    CloseHandle(tokenHandle);
    SetLastError(lastError);
    return success;
}

BOOL
IsRecycleBinItem(
    LPCWSTR Path
)
{
    BOOL Result;
    LPWSTR UpcasePath;

    UpcasePath = _wcsdup(Path);
    if (UpcasePath == NULL)
        return FALSE;

    _wcsupr(UpcasePath);

    Result = wcsstr(UpcasePath, L"$RECYCLE.BIN") != NULL ||
        wcsstr(UpcasePath, L"RECYCLER") != NULL;

    free(UpcasePath);
    return Result;
}

//
//Input:
//  Path - source file path relative to CBFS root path
//Output:
// returns TRUE - if Path is a part of QUOTA_INDEX_FILE path
// FileId set to NTFS disk quotas index file if Path equal to QUOTA_INDEX_FILE,
// otherwise FileId = 0
//
BOOL
IsQuotasIndexFile(
    LPCWSTR Path,
    __int64* FileId
)
{
    static LPCWSTR QUOTA_INDEX_FILE = L"\\$Extend\\$Quota:$Q:$INDEX_ALLOCATION";

    if (Path[1] == '$')
    {
        if (wcsstr(QUOTA_INDEX_FILE, Path) == QUOTA_INDEX_FILE) {

            if (wcscmp(Path, QUOTA_INDEX_FILE) == 0) {

                if (FileId != NULL) {
                    WCHAR buf[64];
                    buf[0] = g_RootPath[0];
                    buf[1] = g_RootPath[1];
                    buf[2] = 0;
                    wcscat(buf, QUOTA_INDEX_FILE);
                    *FileId = GetFileId(buf);
                }
            }
            return TRUE;
        }
    }
    return FALSE;
}

__int64
GetFileId(
    LPCWSTR FileName
)
{
    BY_HANDLE_FILE_INFORMATION fi;

    HANDLE hFile = CreateFile(
        FileName,
        READ_CONTROL,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return 0;

    if (!GetFileInformationByHandle(hFile, &fi))
    {

        CloseHandle(hFile);
        return 0;
    }

    CloseHandle(hFile);

    return ((__int64)fi.nFileIndexHigh << 32 & 0xFFFFFFFF00000000) | fi.nFileIndexLow;
}




