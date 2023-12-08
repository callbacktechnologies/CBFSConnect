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

//
// Uncomment the QUOTA_SUPPORTED define, if you want to support disk quotas.
// 
// #define QUOTA_SUPPORTED

// Enable in order to support file security.
#define SUPPORT_FILE_SECURITY 0
#define SUPPORT_FILE_SECURITY 1

// Enable in order to support file IDs (necessary for NFS sharing) or
// hard links (the OnEnumerateHardLinks and OnCreateHardLink events).
#define SUPPORT_FILE_IDS 0
#define SUPPORT_FILE_IDS 1

// Enable in order to support hard links (several names for the same file,
// it works only if the OnGetFileNameByFileId event handler is implemented). 
#define SUPPORT_HARD_LINKS 0
#define SUPPORT_HARD_LINKS 1

// Enable in order to support reparse points (symbolic links, mounting points, etc.) support. Also required for NFS sharing
#define SUPPORT_REPARSE_POINTS 0
#define SUPPORT_REPARSE_POINTS 1

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

#ifdef QUOTA_SUPPORTED
#include <DskQuota.h>
#endif

#include "cbfsconnectcommon.h"

#ifdef _UNICODE
#include "../../include/unicode/cbfs.h"
#else
#include "../../include/cbfs.h"
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
    HANDLE hFile;
    LPVOID EnumCtx;

} NAMED_STREAM_CONTEXT, * PNAMED_STREAM_CONTEXT;

typedef struct
{
    CONTEXT_HEADER Header;
    HANDLE Handle;

} HARD_LINK_CONTEXT, * PHARD_LINK_CONTEXT;

typedef struct
{
    CONTEXT_HEADER Header;
    PDISKQUOTA_CONTROL   pDiskQuotaControl;
    BOOL OleInitialized;

} DISK_QUOTAS_CONTEXT, * PDISK_QUOTAS_CONTEXT;

typedef struct
{
    CONTEXT_HEADER Header;
    INT   OpenCount;
    HANDLE hFile;
    BOOL  ReadOnly;
    BOOL QuotasIndexFile;
}   FILE_CONTEXT, * PFILE_CONTEXT;

#define CBFSCONNECT_TAG 0x5107

typedef DWORD NODE_TYPE_CODE;
typedef NODE_TYPE_CODE* PNODE_TYPE_CODE;

#define NodeType(Ptr) (*((PNODE_TYPE_CODE)(Ptr)))

BOOL EnablePrivilege(LPCWSTR);

BOOL IsRecycleBinItem(LPCWSTR);

#define QUOTA_FILE L"\\??\\C:\\$Extend\\$Quota:$Q:$INDEX_ALLOCATION"

BOOL IsQuotasIndexFile(LPCWSTR, __int64*);

VOID FreeQuotaContext(PDISK_QUOTAS_CONTEXT Context);

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

#ifdef QUOTA_SUPPORTED
INT AllocateQuotaContext(PSID Sid, PDISK_QUOTAS_CONTEXT* Context);
#endif

//////////////////////////////////////////////////////////////////////////

class FolderDriveCBFS : public CBFS
{

public: // Events
    FolderDriveCBFS() : CBFS()
    {
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
        printf("OnCloseFile fired for %ws\n\n", e->FileName);
        if (e->FileContext != 0)
        {
            PFILE_CONTEXT Ctx = (PFILE_CONTEXT)(e->FileContext);

            assert(NTC_FILE_CONTEXT == NodeType(Ctx));
            assert(Ctx->OpenCount > 0);

            if (Ctx->OpenCount == 1)
            {
                if (!Ctx->QuotasIndexFile && (Ctx->hFile != INVALID_HANDLE_VALUE))
                {
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

    INT FireCloseHardLinksEnumeration(CBFSCloseHardLinksEnumerationEventParams* e) override
    {
        if (!SUPPORT_HARD_LINKS)
            return 0;

        PHARD_LINK_CONTEXT Context;
        Context = (PHARD_LINK_CONTEXT)(e->EnumerationContext);
        FindClose(Context->Handle);
        free(Context);

        return 0;
    }

    INT FireCloseNamedStreamsEnumeration(CBFSCloseNamedStreamsEnumerationEventParams* e) override
    {
        if (e->EnumerationContext != 0)
        {
            PNAMED_STREAM_CONTEXT pInfo = (PNAMED_STREAM_CONTEXT)(e->EnumerationContext);
            assert(NTC_NAMED_STREAMS_ENUM == NodeType(pInfo));
            if (pInfo->hFile != INVALID_HANDLE_VALUE)
                FindClose(pInfo->hFile);
            free(pInfo);
        }

        return 0;
    }

#ifdef QUOTA_SUPPORTED
    INT FireCloseQuotasEnumeration(CBFSCloseQuotasEnumerationEventParams* e) override
    {
        if (!GetUseDiskQuotas())
            return 0;

        PDISK_QUOTAS_CONTEXT Context = (PDISK_QUOTAS_CONTEXT)(e->EnumerationContext);

        FreeQuotaContext(Context);

        return 0;
    }
#endif

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

        PFILE_CONTEXT Ctx = (PFILE_CONTEXT)malloc(sizeof(FILE_CONTEXT));

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
                LastError = ::GetLastError();
                free(FName);
                free(Ctx);
                FName = NULL;
                e->ResultCode = LastError;
                return e->ResultCode;
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
                LastError = ::GetLastError();
                e->FileContext = NULL;
                free(Ctx);
                free(FName);
                FName = NULL;
                e->ResultCode = LastError;
                return e->ResultCode;
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
                LastError = ::GetLastError();
                e->FileContext = NULL;
                free(Ctx);
                free(FName);
                FName = NULL;
                e->ResultCode = LastError;
                return e->ResultCode;
            }
        }

        Ctx->OpenCount++;
        e->FileContext = Ctx;

        if (FName) {
            free(FName);
        }

        return 0;
    }

    INT FireCreateHardLink(CBFSCreateHardLinkEventParams* e) override
    {
        if (!SUPPORT_HARD_LINKS)
            return 0;

        LPCWSTR FileName = e->FileName;
        LPWSTR CurrentName = static_cast<LPWSTR>(malloc((wcslen(FileName) + wcslen(g_RootPath) + 1) * sizeof(WCHAR)));

        if (NULL == CurrentName)
        {
            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            return e->ResultCode;
        }

        wcscpy(CurrentName, g_RootPath);
        wcscat(CurrentName, FileName);

        LPCWSTR LinkName = e->LinkName;
        LPWSTR NewName = static_cast<LPWSTR>(malloc((wcslen(LinkName) + wcslen(g_RootPath) + 1) * sizeof(WCHAR)));

        if (NULL == NewName)
        {
            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            free(CurrentName);
            return e->ResultCode;
        }

        wcscpy(NewName, g_RootPath);
        wcscat(NewName, LinkName);

        BOOL b = CreateHardLink(NewName, CurrentName, NULL);

        DWORD LastError = ::GetLastError();

        free(NewName);
        free(CurrentName);

        if (!b)
        {
            e->ResultCode = LastError;
            return LastError;
        }

        return 0;
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

    INT FireDeleteReparsePoint(CBFSDeleteReparsePointEventParams* e) override
    {
        if (!SUPPORT_REPARSE_POINTS)
            return 0;

        PFILE_CONTEXT Ctx = (PFILE_CONTEXT)(e->FileContext);

        assert(NTC_FILE_CONTEXT == NodeType(Ctx));

        DWORD BytesReturned = 0;
        //PREPARSE_DATA_BUFFER Data = (PREPARSE_DATA_BUFFER)Buffer;
        OVERLAPPED ovl = { 0 };

        if (0 == DeviceIoControl(Ctx->hFile,                 // handle to file or directory
            FSCTL_DELETE_REPARSE_POINT, // dwIoControlCode
            (LPVOID)e->ReparseBuffer, // input buffer 
            e->ReparseBufferLength,     // size of input buffer
            NULL,                       // lpOutBuffer
            0,                          // nOutBufferSize
            &BytesReturned,             // lpBytesReturned
            &ovl))                     // OVERLAPPED structure
        {
            e->ResultCode = ::GetLastError();
            return e->ResultCode;
        }

        return 0;
    }

    INT FireEjected(CBFSEjectedEventParams* e) override
    {
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

                *(e->pAllocationSize) = *(e->pSize);

                *(e->pAttributes) = pInfo->finfo.dwFileAttributes;

                *(e->pFileId) = 0;

                if ((*(e->pAttributes) & FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT)
                    *(e->pReparseTag) = pInfo->finfo.dwReserved0;

                //
                // If FileId is supported then get FileId for the file.
                //
                if (SUPPORT_FILE_IDS)
                {
                    WCHAR FullFileName[MAX_PATH + 1];
                    LPCWSTR DirName = e->DirectoryName;

                    DWORD RootPathLength = (DWORD)wcslen(g_RootPath);
                    DWORD DirNameLength = (DWORD)wcslen(DirName);
                    DWORD FileNameLength = (DWORD)wcslen(pInfo->finfo.cFileName);

                    if (RootPathLength + DirNameLength + FileNameLength + 1 <= sizeof(FullFileName) / sizeof(WCHAR))
                    {
                        memcpy(FullFileName, g_RootPath, RootPathLength * sizeof(WCHAR));

                        DWORD Offset = RootPathLength;

                        memcpy(FullFileName + Offset, DirName, DirNameLength * sizeof(WCHAR));

                        Offset += DirNameLength;

                        memcpy(FullFileName + Offset, L"\\", sizeof(WCHAR));
                        Offset += 1;

                        memcpy(FullFileName + Offset, pInfo->finfo.cFileName, (FileNameLength + 1) * sizeof(WCHAR));

                        *(e->pFileId) = GetFileId(FullFileName);
                    }
                }
            }
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

    INT FireEnumerateHardLinks(CBFSEnumerateHardLinksEventParams* e) override
    {
        if (!SUPPORT_HARD_LINKS)
            return 0;

        PHARD_LINK_CONTEXT Context;
        DWORD LastError;
        DWORD BufferLength;
        WCHAR Buffer[_MAX_PATH];

        while (1) {

            if (e->EnumerationContext == NULL) {

                Context = (PHARD_LINK_CONTEXT)malloc(sizeof(HARD_LINK_CONTEXT));
                if (NULL == Context)
                {
                    e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
                    return e->ResultCode;
                }

                ZeroMemory(Context, sizeof(HARD_LINK_CONTEXT));
                Context->Header.NodeByteSize = sizeof(HARD_LINK_CONTEXT);
                Context->Header.NodeTypeCode = NTC_HARD_LINKS_ENUM;

                LPWSTR FullName = GetDiskName(e->FileName);
                if (NULL == FullName)
                {
                    e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
                    return e->ResultCode;
                }

                BufferLength = sizeof(Buffer) / sizeof(WCHAR);
                Context->Handle = FindFirstFileNameW(FullName, 0, &BufferLength, Buffer);
                if (Context->Handle == INVALID_HANDLE_VALUE) {

                    LastError = ::GetLastError();
                    free(FullName);
                    free(Context);

                    e->ResultCode = LastError;
                    return e->ResultCode;
                }

                free(FullName);

                e->EnumerationContext = Context;
            }
            else {

                Context = (PHARD_LINK_CONTEXT)(e->EnumerationContext);

                BufferLength = sizeof(Buffer) / sizeof(WCHAR);
                if (!FindNextFileNameW(Context->Handle, &BufferLength, Buffer)) {

                    LastError = ::GetLastError();
                    if (LastError == ERROR_HANDLE_EOF) {

                        e->LinkFound = FALSE;
                        return 0;
                    }
                    else {
                        e->ResultCode = LastError;
                        return e->ResultCode;
                    }
                } // if
            } // else

            //
            // The link name returned from FindFirstFileNameW and FindNextFileNameW can 
            // contain path without drive letter. 
            //

            if (Buffer[0] == L'\\') {

                DWORD MountPointLength = 2;

                for (int i = 2; i < (int)wcslen(g_RootPath); i++, MountPointLength++) {

                    if (g_RootPath[i] == L'\\')
                        break;
                }

                memmove(
                    Buffer + MountPointLength,
                    Buffer,
                    (wcslen(Buffer) + 1) * sizeof(WCHAR));

                memcpy(Buffer, g_RootPath, MountPointLength * sizeof(WCHAR));
            }

            //
            // Report only file names (hard link names) that are only
            // inside the mapped directory. 
            //
            if (_wcsnicmp(g_RootPath, Buffer, wcslen(g_RootPath)) == 0)
                break;
        }

        //
        // The link name has been obtained. Trim it to the link name without path.
        //

        e->LinkFound = TRUE;
        if (e->lenLinkName >= 0)
            e->LinkName[0] = 0;

        PWCHAR LastSlash = wcsrchr(Buffer, L'\\');
        //LastSlash[0] = L'\0';
        LastSlash++;
        e->ResultCode = CopyStringToBuffer(e->LinkName, e->lenLinkName, LastSlash);

        if (e->ResultCode == 0)
        {
            //
            // And get file ID for its parent directory. 
            // For the root directory return 0x7FFFFFFFFFFFFFFF, because it's
            // predefined value for the root directory.
            //

            if (wcscmp(Buffer, g_RootPath) == 0) {

                *(e->pParentId) = 0x7FFFFFFFFFFFFFFFLL;
            }
            else {

                HANDLE DirHandle = CreateFileW(
                    Buffer,
                    READ_CONTROL,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS,
                    NULL);

                //
                // In the case we won't obtain file ID for the directory
                // set it to some fake value.
                //

                if (DirHandle != INVALID_HANDLE_VALUE) {

                    BY_HANDLE_FILE_INFORMATION Info;

                    if (GetFileInformationByHandle(DirHandle, &Info)) {

                        *(e->pParentId) = ((__int64)Info.nFileIndexHigh << 32 & 0xFFFFFFFF00000000) | Info.nFileIndexLow;
                    }

                    CloseHandle(DirHandle);
                } // if
            } // else
        } // if

        return e->ResultCode;
    }

    INT FireEnumerateNamedStreams(CBFSEnumerateNamedStreamsEventParams* e) override
    {
        WIN32_FIND_STREAM_DATA fsd;
        LPWSTR FullName = NULL;
        PNAMED_STREAM_CONTEXT nsc = NULL;
        BOOL SearchResult = FALSE;

        ZeroMemory(&fsd, sizeof(fsd));
        e->NamedStreamFound = FALSE;

        while (!e->NamedStreamFound)
        {
            if (e->EnumerationContext == NULL)
            {
                LPCWSTR FileName = e->FileName;
                FullName = GetDiskName(e->FileName);
                if (NULL == FullName)
                {
                    e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
                    return e->ResultCode;
                }

                nsc = (PNAMED_STREAM_CONTEXT)malloc(sizeof(NAMED_STREAM_CONTEXT));

                if (NULL == nsc)
                {
                    free(FullName);
                    e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
                    return e->ResultCode;
                }

                ZeroMemory(nsc, sizeof(NAMED_STREAM_CONTEXT));

                nsc->Header.NodeTypeCode = NTC_NAMED_STREAMS_ENUM;

                nsc->hFile = FindFirstStreamW(FullName, FindStreamInfoStandard, &fsd, 0);
                SearchResult = (nsc->hFile != INVALID_HANDLE_VALUE);

                if (SearchResult)
                    e->EnumerationContext = nsc;
            }
            else
            {
                nsc = (PNAMED_STREAM_CONTEXT)(e->EnumerationContext);
                SearchResult = (nsc != NULL && nsc->hFile != INVALID_HANDLE_VALUE) && FindNextStreamW(nsc->hFile, &fsd);
            }

            if (!SearchResult)
            {
                e->ResultCode = ::GetLastError();
                if (e->ResultCode == ERROR_HANDLE_EOF)
                    e->ResultCode = 0;
                else
                {
                    if (nsc->hFile != INVALID_HANDLE_VALUE)
                        FindClose(nsc->hFile);
                    free(nsc);
                }

                if (FullName)
                    free(FullName);
                return 0;
            }

            if (wcscmp(fsd.cStreamName, _T("::$DATA")) == 0)
                continue;

            e->NamedStreamFound = TRUE;

            *(e->pStreamSize) = fsd.StreamSize.QuadPart;
            *(e->pStreamAllocationSize) = fsd.StreamSize.QuadPart;

            if (e->lenStreamName > 0)
                e->StreamName[0] = 0;

            e->ResultCode = CopyStringToBuffer(e->StreamName, e->lenStreamName, fsd.cStreamName);
        }

        return 0;
    }

#ifdef QUOTA_SUPPORTED
    INT FireGetDefaultQuotaInfo(CBFSGetDefaultQuotaInfoEventParams* e) override
    {
        if (!GetUseDiskQuotas())
            return 0;

        NTSTATUS Status;
        IO_STATUS_BLOCK Iosb;
        WCHAR root[64];
        HANDLE hFile = INVALID_HANDLE_VALUE;
        FILE_FS_CONTROL_INFORMATION ffci;
        UNICODE_STRING name;
        OBJECT_ATTRIBUTES ObjectAttributes;
        DWORD LastError;
        static PfnNtQueryVolumeInformationFile  NtQueryVolumeInformationFile = 0;
        static PfnNtOpenFile                    NtOpenFile = 0;

        wcscpy(root, QUOTA_FILE);
        root[4] = g_RootPath[0];

        if (0 == NtOpenFile)
        {
            NtOpenFile = (PfnNtOpenFile)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtOpenFile");
        }

        assert(NtOpenFile != NULL);

        if (0 == NtQueryVolumeInformationFile)
        {
            NtQueryVolumeInformationFile = (PfnNtQueryVolumeInformationFile)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryVolumeInformationFile");
        }

        assert(NtQueryVolumeInformationFile);

        name.Buffer = root;
        name.Length = name.MaximumLength = sizeof(QUOTA_FILE) - sizeof(WCHAR);

        InitializeObjectAttributes(&ObjectAttributes,
            &name,
            0,
            NULL,
            NULL);

        LastError = LsaNtStatusToWinError(NtOpenFile(&hFile,
            FILE_READ_DATA | FILE_WRITE_DATA,
            &ObjectAttributes,
            &Iosb,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0x00000001 //FILE_OPEN
        ));

        if (LastError != ERROR_SUCCESS)
        {
            e->ResultCode = LastError;
            return LastError;
        }

        ZeroMemory(&ffci, sizeof(FILE_FS_CONTROL_INFORMATION));

        Status = NtQueryVolumeInformationFile(hFile,
            &Iosb,
            &ffci,
            sizeof(FILE_FS_CONTROL_INFORMATION),
            FileFsControlInformation);

        LastError = LsaNtStatusToWinError(Status);

        if (LastError == ERROR_SUCCESS) {

            *(e->pDefaultQuotaThreshold) = ffci.DefaultQuotaThreshold.QuadPart;
            *(e->pDefaultQuotaLimit) = ffci.DefaultQuotaLimit.QuadPart;
            *(e->pFileSystemControlFlags) = ffci.FileSystemControlFlags;
        }

        CloseHandle(hFile);

        if (LastError != ERROR_SUCCESS)
        {
            e->ResultCode = LastError;
            return LastError;
        }

        return 0;
    }
#endif


    INT FireGetFileInfo(CBFSGetFileInfoEventParams* e) override
    {
        BY_HANDLE_FILE_INFORMATION fi;
        HANDLE hFile, hFile2;
        DWORD error;

        BOOL IsADS = FALSE;

        static PfnNtQueryInformationFile NtQueryInformationFile = 0;

        if (0 == NtQueryInformationFile)
        {
            NtQueryInformationFile = (PfnNtQueryInformationFile)GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryInformationFile");
        }

        assert(NtQueryInformationFile);

        LPCWSTR FileName = e->FileName;

#ifdef QUOTA_SUPPORTED
        if (IsQuotasIndexFile(FileName, e->pFileId)) {

            //
            // at this point we emulate a
            // disk quotas index file path on
            // NTFS volumes (\\$Extend\\$Quota:$Q:$INDEX_ALLOCATION)
            //
            e->FileExists = TRUE;
            return 0;
        }
#endif 

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

        IsADS = (wcsrchr(FileName, _T(':')) != NULL);

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
                    PWCHAR realStreamName = wcsrchr(AName, L':');
                    if (realFileName != NULL)
                    {
                        realFileName += 1;

                        //
                        // Query the ADS information and specify the ADS name.
                        //
                        if (realStreamName != NULL)
                        {
                            realFileName = realStreamName;
                            IsADS = TRUE;
                        }
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

            // If we have an ADS, retrieve its size, 
            // then substitute the handle so that we could retrieve attributes and times of the parent file of the ADS
            if (IsADS)
            {
                if (GetFileInformationByHandle(hFile, &fi))
                {
                    *(e->pSize) = ((__int64)fi.nFileSizeHigh << 32 & 0xFFFFFFFF00000000) | fi.nFileSizeLow;
                }

                sp = wcsrchr(FName, _T('\\'));

                if (sp != NULL)
                {
                    ep = wcschr(sp, _T(':'));
                    if (ep != NULL)
                        *ep = _T('\0');
                }

                hFile2 = CreateFile(
                    FName,
                    READ_CONTROL,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                    NULL);

                if (hFile2 != INVALID_HANDLE_VALUE) {
                    CloseHandle(hFile);
                    hFile = hFile2;
                }
            }

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

                if (!IsADS)
                {
                    *(e->pSize) = ((__int64)fi.nFileSizeHigh << 32 & 0xFFFFFFFF00000000) | fi.nFileSizeLow;
                }
                *(e->pAllocationSize) = *(e->pSize);

                e->HardLinkCount = fi.nNumberOfLinks;

                //
                // If FileId is supported then set FileId for the file.
                //
                if (!SUPPORT_FILE_IDS)
                    *(e->pFileId) = 0;
                else
                    *(e->pFileId) = ((__int64)fi.nFileIndexHigh << 32 & 0xFFFFFFFF00000000) | fi.nFileIndexLow;
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

    INT FireGetFileSecurity(CBFSGetFileSecurityEventParams* e) override
    {
        if (!SUPPORT_FILE_SECURITY)
            return 0;

        DWORD error = ERROR_SUCCESS;

        //
        // Getting SACL_SECURITY_INFORMATION requires the program to 
        // execute elevated as well as the SE_SECURITY_NAME privilege
        // to be set. So just remove it from the request.
        //

        INT SecurityInformation = e->SecurityInformation;
        SecurityInformation &= ~SACL_SECURITY_INFORMATION;
        if (SecurityInformation == 0) {

            e->DescriptorLength = 0;
            return 0;
        }

        //
        // For recycle bin Windows expects some specific attributes 
        // (if they aren't set then it's reported that the recycle
        // bin is corrupted). So just get attributes from the recycle
        // bin files located on the volume "C:".
        //

        LPCWSTR FileName = e->FileName;
        if (IsRecycleBinItem(FileName)) {

            LPWSTR PathOnVolumeC = (LPWSTR)malloc((sizeof("C:") - 1 + wcslen(FileName) + 1) * sizeof(WCHAR));
            if (NULL == PathOnVolumeC)
            {
                e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
                return e->ResultCode;
            }

            wcscpy(PathOnVolumeC, L"C:");
            wcscat(PathOnVolumeC, FileName);

            DWORD LengthNeeded;
            if (!GetFileSecurityW(PathOnVolumeC,
                SecurityInformation,
                e->SecurityDescriptor,
                e->BufferLength,
                &LengthNeeded)) {

                //
                // If the SecurityDescriptor buffer is smaller than required then
                // GetFileSecurity itself sets in the LengthNeeded parameter the required 
                // size and the last error is set to ERROR_INSUFFICIENT_BUFFER.
                //
                error = ::GetLastError();
            }

            e->DescriptorLength = LengthNeeded;

            free(PathOnVolumeC);

            if (error != ERROR_SUCCESS)
            {
                e->ResultCode = error;
                return error;
            }
        }
        else
        {
            LPWSTR FullName = GetDiskName(e->FileName);
            if (NULL == FullName)
            {
                e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
                return e->ResultCode;
            }

            DWORD LengthNeeded;
            if (!GetFileSecurityW(FullName,
                SecurityInformation,
                e->SecurityDescriptor,
                e->BufferLength,
                &LengthNeeded))
            {
                error = ::GetLastError();
            }

            e->DescriptorLength = LengthNeeded;
            free(FullName);

            if (error != ERROR_SUCCESS)
            {
                e->ResultCode = error;
                return error;
            }
        }

        return 0;
    }

    INT FireGetReparsePoint(CBFSGetReparsePointEventParams* e) override
    {
        if (!SUPPORT_REPARSE_POINTS)
            return 0;

        PFILE_CONTEXT Ctx = NULL;
        DWORD error = NO_ERROR;
        DWORD BytesReturned = 0;
        //PREPARSE_DATA_BUFFER Data = (PREPARSE_DATA_BUFFER)Buffer;
        OVERLAPPED ovl = { 0 };
        HANDLE FileHandle = INVALID_HANDLE_VALUE;
        BOOL FileOpened = FALSE;
        BOOL Result;

        Ctx = (PFILE_CONTEXT)(e->FileContext);

        if (Ctx == NULL || Ctx->hFile == NULL)
        {
            LPCWSTR FileName = e->FileName;
            LPWSTR FullName = GetDiskName(e->FileName);
            if (NULL == FullName)
            {
                e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
                return e->ResultCode;
            }

            FileHandle = CreateFile(
                FullName,
                READ_CONTROL,
                0,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                NULL);

            error = ::GetLastError();
            free(FullName);

            if (FileHandle == INVALID_HANDLE_VALUE)
            {
                e->ResultCode = error;
                return error;
            }
            FileOpened = TRUE;

        }
        else
        {
            assert(NTC_FILE_CONTEXT == NodeType(Ctx));
            FileHandle = Ctx->hFile;
        }

        Result = DeviceIoControl(FileHandle,              // handle to file or directory
            FSCTL_GET_REPARSE_POINT, // dwIoControlCode
            NULL,                    // input buffer 
            0,                       // size of input buffer
            e->ReparseBuffer,        // lpOutBuffer
            e->ReparseBufferLength,  // nOutBufferSize
            &BytesReturned,          // lpBytesReturned
            &ovl);                  // OVERLAPPED structure

        error = ::GetLastError();
        e->ReparseBufferLength = BytesReturned;

        if (FileOpened)
            CloseHandle(FileHandle);

        if (!Result)
        {
            e->ResultCode = error;
            return error;
        }

        return 0;
    }

    INT FireGetVolumeId(CBFSGetVolumeIdEventParams* e) override
    {
        e->VolumeId = 0x12345678;
        return 0;
    }

    INT FireGetVolumeLabel(CBFSGetVolumeLabelEventParams* e) override
    {
        LPWSTR Label = const_cast<LPWSTR>(L"Folder Drive");
        e->ResultCode = CopyStringToBuffer(e->Buffer, e->lenBuffer, Label);
        return e->ResultCode;
    }

    INT FireGetVolumeSize(CBFSGetVolumeSizeEventParams* e) override
    {
        DISKQUOTA_USER_INFORMATION dqui = { 0,-1,-1 };
#ifdef QUOTA_SUPPORTED
        ULONGLONG RemainingQuota;
#endif

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
#ifdef QUOTA_SUPPORTED
        if (!GetUseDiskQuotas())
        {
            *(e->pAvailableSectors) = FreeBytes / GetSectorSize();
            *(e->pTotalSectors) = TotalBytes / GetSectorSize();
        }
        else { //fSupportDiskQuotas == TRUE
          //
          // We must take into account per user disk quotas
          //
            DWORD error, ReturnLength;
            PTOKEN_USER tu = NULL;
            PDISKQUOTA_CONTROL pDiskQuotaControl = NULL;
            PDISKQUOTA_USER pUser = NULL;
            HRESULT hr;
            BOOL res;
            WCHAR rootDir[4] = L"C:\\";

            rootDir[0] = g_RootPath[0];

            HANDLE user = (HANDLE)GetOriginatorToken();

            res = GetTokenInformation(user,
                TokenUser,
                NULL,
                0,
                &ReturnLength);

            assert(!res);
            assert(::GetLastError() == ERROR_INSUFFICIENT_BUFFER);

            tu = (PTOKEN_USER)malloc(ReturnLength);
            if (!tu)
            {
                e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
                return e->ResultCode;
            }


            res = GetTokenInformation(user,
                TokenUser,
                tu,
                ReturnLength,
                &ReturnLength);

            error = ::GetLastError();
            CloseHandle(user);

            if (!res)
            {
                e->ResultCode = error;
                return e->ResultCode;
            }

            //
            // get the user disk quota limits
            //
            OleInitialize(NULL);

            hr = CoCreateInstance(CLSID_DiskQuotaControl,
                NULL,
                CLSCTX_INPROC_SERVER,
                IID_IDiskQuotaControl,
                (PVOID*)&pDiskQuotaControl);

            if (SUCCEEDED(hr)) {

                hr = pDiskQuotaControl->Initialize(rootDir, TRUE);

                if (S_OK == hr) {

                    hr = pDiskQuotaControl->FindUserSid(tu->User.Sid,
                        DISKQUOTA_USERNAME_RESOLVE_SYNC,
                        &pUser);

                    if (SUCCEEDED(hr))
                        hr = pUser->GetQuotaInformation(&dqui, sizeof(DISKQUOTA_USER_INFORMATION));
                }
            }

            free(tu);

            if (pUser)
                pUser->Release();
            if (pDiskQuotaControl)
                pDiskQuotaControl->Release();

            OleUninitialize();

            if ((ULONGLONG)dqui.QuotaLimit <= (ULONGLONG)dqui.QuotaUsed)
                RemainingQuota = 0;
            else
                RemainingQuota = (ULONGLONG)dqui.QuotaLimit - (ULONGLONG)dqui.QuotaUsed;

            if ((ULONGLONG)dqui.QuotaLimit < (ULONGLONG)TotalBytes)
                *(e->pTotalSectors) = dqui.QuotaLimit / GetSectorSize();
            else
                *(e->pTotalSectors) = TotalBytes / GetSectorSize();

            if (RemainingQuota < (ULONGLONG)FreeBytes)
                *(e->pAvailableSectors) = RemainingQuota / GetSectorSize();
            else
                *(e->pAvailableSectors) = FreeBytes / GetSectorSize();
        }
#else
        * (e->pAvailableSectors) = FreeBytes / SectorSize;
        *(e->pTotalSectors) = TotalBytes / SectorSize;
#endif

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

#ifdef QUOTA_SUPPORTED
        if (IsQuotasIndexFile(FileName, NULL)) {

            //
            // at this point we emulate a
            // disk quotas index file path on
            // NTFS volumes (\\$Extend\\$Quota:$Q:$INDEX_ALLOCATION)
            //
            Ctx->hFile = INVALID_HANDLE_VALUE;
            Ctx->QuotasIndexFile = TRUE;
        }
        else
#endif
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

#ifdef QUOTA_SUPPORTED
    INT FireQueryQuotas(CBFSQueryQuotasEventParams* e) override
    {
        if (!GetUseDiskQuotas())
            return 0;

        PENUM_DISKQUOTA_USERS pEnumDiskQuotaUsers = NULL;
        PDISKQUOTA_USER pUser = NULL;
        HRESULT hr;
        DWORD error;
        PDISK_QUOTAS_CONTEXT Context;
        DISKQUOTA_USER_INFORMATION DiskQuotaInformation;

        if (e->EnumerationContext == NULL) {

            e->ResultCode = AllocateQuotaContext((LPVOID)e->SID, &Context);
            if (e->ResultCode != 0)
                return e->ResultCode;
            e->EnumerationContext = Context;
        }
        else
        { // Context is not null

            Context = (PDISK_QUOTAS_CONTEXT)(e->EnumerationContext);
        }

        assert(Context);

        e->QuotaFound = false;

        if (e->SID != NULL)
        {

            hr = Context->pDiskQuotaControl->FindUserSid((LPVOID)e->SID,
                DISKQUOTA_USERNAME_RESOLVE_SYNC,
                &pUser);

            if (SUCCEEDED(hr))
            {
                hr = pUser->GetQuotaInformation(&DiskQuotaInformation, sizeof(DISKQUOTA_USER_INFORMATION));
                if (e->pQuotaLimit != NULL)
                    *(e->pQuotaLimit) = DiskQuotaInformation.QuotaLimit;
                if (e->pQuotaThreshold != NULL)
                    *(e->pQuotaThreshold) = DiskQuotaInformation.QuotaThreshold;
                if (e->pQuotaUsed != NULL)
                    *(e->pQuotaUsed) = DiskQuotaInformation.QuotaUsed;
            }
        }
        else { // Sid is null

            hr = Context->pDiskQuotaControl->CreateEnumUsers(NULL,
                0,
                DISKQUOTA_USERNAME_RESOLVE_SYNC,
                &pEnumDiskQuotaUsers);

            if (SUCCEEDED(hr)) {

                hr = pEnumDiskQuotaUsers->Skip(e->Index);

                if (hr == S_OK) {

                    hr = pEnumDiskQuotaUsers->Next(1, &pUser, NULL);

                    if (hr == S_OK) {

                        pUser->GetQuotaInformation(&DiskQuotaInformation, sizeof(DISKQUOTA_USER_INFORMATION));

                        if (e->pQuotaLimit != NULL)
                            *(e->pQuotaLimit) = DiskQuotaInformation.QuotaLimit;
                        if (e->pQuotaThreshold != NULL)
                            *(e->pQuotaThreshold) = DiskQuotaInformation.QuotaThreshold;
                        if (e->pQuotaUsed != NULL)
                            *(e->pQuotaUsed) = DiskQuotaInformation.QuotaUsed;

                        hr = pUser->GetSid((LPBYTE)e->SIDOut, e->SIDOutLength);
                    }
                }
            }
        }

        if (pUser != NULL)
            pUser->Release();
        if (pEnumDiskQuotaUsers != NULL)
            pEnumDiskQuotaUsers->Release();

        if (hr == S_OK) {

            e->QuotaFound = TRUE;
        }
        else if (hr != S_FALSE) {

            WIN32_FROM_HRESULT(hr, &error);
            e->ResultCode = error;
            return error;
        }

        return 0;
    }
#endif


    INT FireReadFile(CBFSReadFileEventParams* e) override
    {
        OVERLAPPED Overlapped;
        PFILE_CONTEXT Ctx = (PFILE_CONTEXT)(e->FileContext);
        assert(Ctx);

        FillMemory(&Overlapped, sizeof(Overlapped), 0);

        Overlapped.Offset = (DWORD)(e->Position & 0xFFFFFFFF);

        Overlapped.OffsetHigh = (DWORD)(e->Position >> 32);

        DWORD BytesRead;
        if (!ReadFile(Ctx->hFile, e->Buffer, (DWORD)e->BytesToRead, &BytesRead, &Overlapped))
        {
            e->ResultCode = ::GetLastError();
            return e->ResultCode;
        }

        *(e->pBytesRead) = BytesRead;

        return 0;
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

#ifdef QUOTA_SUPPORTED
    INT FireSetDefaultQuotaInfo(CBFSSetDefaultQuotaInfoEventParams* e) override
    {
        if (!GetUseDiskQuotas())
            return 0;

        NTSTATUS Status;
        IO_STATUS_BLOCK Iosb;
        WCHAR root[64];
        HANDLE hFile = INVALID_HANDLE_VALUE;
        FILE_FS_CONTROL_INFORMATION ffci;
        UNICODE_STRING name;
        OBJECT_ATTRIBUTES ObjectAttributes;
        DWORD LastError;

        static PfnNtSetVolumeInformationFile NtSetVolumeInformationFile = 0;
        static PfnNtOpenFile                 NtOpenFile = 0;

        if (NtOpenFile == 0)
        {
            NtOpenFile = (PfnNtOpenFile)GetProcAddress(
                GetModuleHandle(L"ntdll.dll"), "NtOpenFile");
        }

        assert(NtOpenFile != NULL);

        if (NtSetVolumeInformationFile == 0)
        {
            NtSetVolumeInformationFile = (PfnNtSetVolumeInformationFile)GetProcAddress(
                GetModuleHandle(L"ntdll.dll"), "NtSetVolumeInformationFile");
        }

        assert(NtSetVolumeInformationFile);


        wcscpy(root, QUOTA_FILE);
        //
        // set drive symbol
        //
        root[4] = g_RootPath[0];

        name.Buffer = root;
        name.Length = name.MaximumLength = sizeof(QUOTA_FILE) - sizeof(WCHAR);

        InitializeObjectAttributes(
            &ObjectAttributes,
            &name,
            0,
            NULL,
            NULL);

        LastError = LsaNtStatusToWinError(NtOpenFile(&hFile,
            FILE_READ_DATA | FILE_WRITE_DATA,
            &ObjectAttributes,
            &Iosb,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            0x00000001 //FILE_OPEN
        ));

        if (LastError != ERROR_SUCCESS)
            return LastError;

        ZeroMemory(&ffci, sizeof(FILE_FS_CONTROL_INFORMATION));

        ffci.DefaultQuotaLimit.QuadPart = e->DefaultQuotaLimit;
        ffci.DefaultQuotaThreshold.QuadPart = e->DefaultQuotaThreshold;
        ffci.FileSystemControlFlags = e->FileSystemControlFlags;

        Status = NtSetVolumeInformationFile(
            hFile,
            &Iosb,
            &ffci,
            sizeof(FILE_FS_CONTROL_INFORMATION),
            FileFsControlInformation);

        LastError = LsaNtStatusToWinError(Status);

        CloseHandle(hFile);

        if (LastError != ERROR_SUCCESS) {
            e->ResultCode = LastError;
            return LastError;
        }

        return 0;
    }
#endif 


    INT FireSetFileSize(CBFSSetFileSizeEventParams* e) override
    {
        DWORD error;
        LONG OffsetHigh;

        PFILE_CONTEXT Ctx = (PFILE_CONTEXT)(e->FileContext);
        assert(Ctx);

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

        DWORD Attr = GetFileAttributes(FName);
        // the case when Attributes == 0 indicates that file attributes
        // not changed during this callback
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

    INT FireSetFileSecurity(CBFSSetFileSecurityEventParams* e) override
    {
        if (!SUPPORT_FILE_SECURITY)
            return 0;

        BOOL Success = FALSE;
        DWORD error;

        LPCWSTR FileName = e->FileName;
        LPWSTR FName = (LPWSTR)malloc((wcslen(FileName) + wcslen(g_RootPath) + 1) * sizeof(WCHAR));
        if (NULL == FName)
        {
            e->ResultCode = ERROR_NOT_ENOUGH_MEMORY;
            return e->ResultCode;
        }

        wcscpy(FName, g_RootPath);
        wcscat(FName, FileName);

        //
        // Disable setting of new security for the backend file because
        // these new security attributes can prohibit manipulation
        // with the file from the callbacks (for example if read/write
        // is not allowed for this process).
        //

        if (!SetFileSecurity(FName,
            e->SecurityInformation,
            (PSECURITY_DESCRIPTOR)e->SecurityDescriptor)) {

            goto end;
        }

        Success = TRUE;

    end:

        error = ::GetLastError();
        free(FName);

        if (!Success)
        {
            e->ResultCode = error;
            return error;
        }

        return 0;
    }

#ifdef QUOTA_SUPPORTED
    INT FireSetQuotas(CBFSSetQuotasEventParams* e) override
    {
        if (!GetUseDiskQuotas())
            return 0;

        PDISKQUOTA_USER pUser = NULL;
        PDISK_QUOTAS_CONTEXT Context;
        HRESULT hr;
        DWORD error = NO_ERROR;

        if (e->EnumerationContext == NULL) {

            e->ResultCode = AllocateQuotaContext((LPVOID)e->SID, &Context);
            if (e->ResultCode != 0)
                return e->ResultCode;
            e->EnumerationContext = Context;
        }
        else
        { // Context is not null

            Context = (PDISK_QUOTAS_CONTEXT)e->EnumerationContext;
        }

        assert(Context);

        hr = Context->pDiskQuotaControl->FindUserSid((LPVOID)e->SID,
            DISKQUOTA_USERNAME_RESOLVE_SYNC,
            &pUser);

        if (SUCCEEDED(hr)) {

            e->QuotaFound = true;

            if (e->RemoveQuota)
            {
                hr = Context->pDiskQuotaControl->DeleteUser(pUser);
            }
            else
            {
                pUser->SetQuotaThreshold(e->QuotaThreshold, TRUE);
                hr = pUser->SetQuotaLimit(e->QuotaLimit, TRUE);
            }
        }
        else
        {
            e->QuotaFound = FALSE;
        }

        if (pUser != NULL)
            pUser->Release();

        if (hr == S_OK)
        {
            // do nothing
        }
        else
            if (hr != S_FALSE) {

                WIN32_FROM_HRESULT(hr, &error);
                e->ResultCode = error;
                return error;
            }

        return 0;
    }
#endif


    INT FireSetReparsePoint(CBFSSetReparsePointEventParams* e) override
    {
        if (!SUPPORT_REPARSE_POINTS)
            return 0;

        LPWSTR FName = NULL;
        DWORD error = NO_ERROR;
        PFILE_CONTEXT Ctx = NULL;
        //PREPARSE_DATA_BUFFER Data = (PREPARSE_DATA_BUFFER)Buffer;
        OVERLAPPED ovl = { 0 };

        Ctx = (PFILE_CONTEXT)(e->FileContext);

        assert(NTC_FILE_CONTEXT == NodeType(Ctx));


        if (0 == DeviceIoControl(Ctx->hFile,              // handle to file or directory
            FSCTL_SET_REPARSE_POINT, // dwIoControlCode
            (LPVOID)e->ReparseBuffer,        // input buffer 
            e->ReparseBufferLength,  // size of input buffer
            NULL,                    // lpOutBuffer
            0,                       // nOutBufferSize
            NULL,                    // lpBytesReturned
            &ovl))                  // OVERLAPPED structure
        {
            error = ::GetLastError();
            e->ResultCode = error;
            return error;
        }

        return 0;
    }

    INT FireWriteFile(CBFSWriteFileEventParams* e) override
    {
        OVERLAPPED Overlapped;
        PFILE_CONTEXT Ctx = (PFILE_CONTEXT)(e->FileContext);
        assert(Ctx);

        printf("OnWriteFile fired for %ws\n\n", e->FileName);

        FillMemory(&Overlapped, sizeof(Overlapped), 0);

        Overlapped.Offset = (DWORD)(e->Position & 0xFFFFFFFF);

        Overlapped.OffsetHigh = (DWORD)(e->Position >> 32);

        DWORD BytesWritten;
        if (!WriteFile(Ctx->hFile, e->Buffer, (DWORD)e->BytesToWrite, &BytesWritten, &Overlapped))
        {
            e->ResultCode = ::GetLastError();
            return e->ResultCode;
        }

        *(e->pBytesWritten) = BytesWritten;
        return 0;
    }
};

const LPCWSTR program_name = L"{713CC6CE-B3E2-4fd9-838D-E28F558F6866}";

const LPCWSTR strSuccess = L"Success";
const LPCSTR strInvalidOption = "Invalid option \"%s\"\n";

//-----------------------------------------------------------------------------------------------------------

int optcmp(char* arg, char* opt)
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
    printf("This demo shows how to mount a folder on your system as a drive.\n\n");
}

void usage(void)
{
    printf("Usage: folderdrive [-<switch 1> ... -<switch N>] <root path> <mounting point>\n\n");
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
    printf("Example: folderdrive C:\\Root Y:\n\n");
}

// ----------------------------------------------------------------------------------

FolderDriveCBFS cbfs;

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
                    if (optcmp(argv[argi], (char*)"--"))
                        stop_opt = 1;
                    else if (optcmp(argv[argi], (char*)"-i")) {
                        argi++;
                        if (argi < argc) {
                            cbt_string icon_path_wstr = ConvertRelativePathToAbsolute(a2w(argv[argi]));
                            opt_icon_path = wcsdup(icon_path_wstr.c_str());
                        }
                    }
#ifdef WIN32
                    else if (optcmp(argv[argi], (char*)"-lc"))
                        opt_local = 1;
#endif
                    else if (optcmp(argv[argi], (char*)"-n"))
                        opt_network = 1;
#ifdef WIN32
                    else if (optcmp(argv[argi], (char*)"-drv")) {
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

                cbfs.SetFileCache(0);
                cbfs.Config(_T("FileCachePolicyWriteThrough=1"));
                cbfs.SetFileSystemName(L"NTFS");

                cbfs.SetUseWindowsSecurity(SUPPORT_FILE_SECURITY);
                cbfs.SetUseReparsePoints(SUPPORT_REPARSE_POINTS);
                cbfs.SetUseHardLinks(SUPPORT_HARD_LINKS);
                cbfs.SetUseFileIds(SUPPORT_HARD_LINKS);

                cbfs.SetUseAlternateDataStreams(TRUE);

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
        fprintf(stderr, "FolderDrive: Invalid parameters, root path not specified\n");
    else if (!mounted) {
        if (mount_point == NULL)
            fprintf(stderr, "FolderDrive: Invalid parameters, mounting point not specified\n");
        else
            fprintf(stderr, "FolderDrive: Failed to mount the disk\n");
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

#ifdef QUOTA_SUPPORTED 
INT
AllocateQuotaContext(
    PSID Sid,
    PDISK_QUOTAS_CONTEXT* Context
)
{
    PDISKQUOTA_CONTROL   pDiskQuotaControl = NULL;
    HRESULT hr;
    WCHAR rootDir[4] = L"C:\\";

    PDISK_QUOTAS_CONTEXT ctx = (PDISK_QUOTAS_CONTEXT)malloc(sizeof(DISK_QUOTAS_CONTEXT));

    if (ctx == NULL)
        return ERROR_OUTOFMEMORY;

    ZeroMemory(ctx, sizeof(DISK_QUOTAS_CONTEXT));
    *Context = NULL;

    hr = OleInitialize(NULL);

    if (SUCCEEDED(hr)) {

        ctx->OleInitialized = TRUE;

        hr = CoCreateInstance(CLSID_DiskQuotaControl,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_IDiskQuotaControl,
            (PVOID*)&pDiskQuotaControl);

        if (SUCCEEDED(hr)) {

            rootDir[0] = g_RootPath[0];
            hr = pDiskQuotaControl->Initialize(rootDir, TRUE);
        }
    }
    if (hr == S_OK) {

        ctx->pDiskQuotaControl = pDiskQuotaControl;
        *Context = ctx;
    }
    else {
        DWORD Error;

        if (pDiskQuotaControl != NULL)
            pDiskQuotaControl->Release();

        if (ctx->OleInitialized)
            OleUninitialize();
        free(ctx);

        WIN32_FROM_HRESULT(hr, &Error);
        return Error;
    }

    return 0;
}
#endif

VOID
FreeQuotaContext(
    PDISK_QUOTAS_CONTEXT Context
)
{
    if (Context == NULL) return;
    if (Context->pDiskQuotaControl != NULL)
        Context->pDiskQuotaControl->Release();

    if (Context->OleInitialized)
        OleUninitialize();
    free(Context);
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
 

