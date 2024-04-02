/*
 * CBFS Connect 2022 C++ Edition - Sample Project
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
#include <vector>
#include <wchar.h>
#include <iostream>
#include <Shlwapi.h>
#include <filesystem>
#include <string>

#ifndef WIN32
#include <unistd.h>
#include <poll.h>
#include <dirent.h>

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/types.h>
#endif

#include "cbfsconnectcommon.h"

#ifdef _UNICODE
#include "../../include/unicode/cbfs.h"
#else
#include "../../include/cbfs.h"
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

class EncryptContext {
private:
    CBFS* mFS;
    DWORD mBufferSize;
    PBYTE mBuffer;
    PBYTE mBufferMask;
    std::vector<uint8_t> mInitialIV;
    __int64 mCurrentSize;
    int mRefCnt;
    BOOL mInitialized;
    BOOL  ReadOnly;

public:
    HANDLE mHandle;

    EncryptContext(CBFS* FS, LPCWSTR FileName, DWORD FileAttributes, INT DesiredAccess, INT NTCreateDisposition, BOOL New);
    ~EncryptContext();

    DWORD Read(__int64 Position, PVOID Buffer, DWORD BytesToRead);
    DWORD Write(__int64 Position, PVOID Buffer, DWORD BytesToWrite);

    void CloseFile(void)
    {
        if (mHandle != INVALID_HANDLE_VALUE)
            CloseHandle(mHandle);
        mHandle = INVALID_HANDLE_VALUE;
    }

    void EncryptBuffer(__int64 position, DWORD size);
    void DecryptBuffer(__int64 position, DWORD size);

    __int64 GetCurrentSize() { return mCurrentSize; };
    void SetCurrentSize(__int64 Value) { mCurrentSize = Value; };

    int IncrementRef();
    int DecrementRef();
    BOOL Initialized() {
        return mInitialized;
    }
    BOOL IsReadOnly() { return ReadOnly; }
};

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
    WIN32_FIND_DATA finfo;
    HANDLE hFile;
}   ENUM_INFO, * PENUM_INFO;

typedef struct
{
    CONTEXT_HEADER Header;
    INT   OpenCount;
    HANDLE hFile;
    BOOL QuotasIndexFile;
}   FILE_CONTEXT, * PFILE_CONTEXT;

#define CBFSCONNECT_TAG 0x5107

typedef DWORD NODE_TYPE_CODE;
typedef NODE_TYPE_CODE* PNODE_TYPE_CODE;

#define FILE_OPEN 1
#define FILE_OPEN_IF 3

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

//////////////////////////////////////////////////////////////////////////

class EncryptedDriveCBFS : public CBFS
{
private:

    // Enable in order to support file IDs (necessary for NFS sharing)
    bool fSupportFileIDs = false;

public: // Events
    EncryptedDriveCBFS() : CBFS()
    {
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
        EncryptContext* Context;
        Context = (EncryptContext*)(e->FileContext);
        if ((Context != NULL) && (Context->DecrementRef() == 0)) {
            delete Context;
        }

        return 0;
    }

    INT FireCreateFile(CBFSCreateFileEventParams* e) override
    {
        EncryptContext* Context = NULL;

        if (e->FileContext == NULL)
        {
            Context = new EncryptContext(this, e->FileName, e->Attributes, e->DesiredAccess, e->NTCreateDisposition, true);

            if (Context != NULL && Context->Initialized())
            {
                e->FileContext = Context;
            }
            else
            {
                DWORD error = ::GetLastError();
                if (Context != NULL)
                    delete Context;
                e->ResultCode = error;
                return e->ResultCode;
            }
        }
        else
        {
            Context = (EncryptContext*)(e->FileContext);
            if (Context->IsReadOnly() && (e->DesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)))
            {
                e->ResultCode = ERROR_ACCESS_DENIED;
                return e->ResultCode;
            }
            Context->IncrementRef();
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
                if (fSupportFileIDs)
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
            } // if
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

    INT FireGetFileInfo(CBFSGetFileInfoEventParams* e) override
    {
        BY_HANDLE_FILE_INFORMATION fi;
        HANDLE hFile;
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

        hFile = CreateFile(
            FName,
            READ_CONTROL,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
            NULL);

        error = ::GetLastError();
        free(FName);

        if (hFile != INVALID_HANDLE_VALUE) {

            e->FileExists = TRUE;

            //
            // Get the real file name.
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
                        // Queried the ADS information and specify the ADS name.
                        //
                        if (realStreamName != NULL)
                            realFileName = realStreamName;
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

            if (!GetFileInformationByHandle(hFile, &fi))
            {
                e->ResultCode = ::GetLastError();
                CloseHandle(hFile);
                return e->ResultCode;
            }

            *(e->pAttributes) = fi.dwFileAttributes;
            *(e->pCreationTime) = FileTimeToInt64(fi.ftCreationTime);
            *(e->pLastAccessTime) = FileTimeToInt64(fi.ftLastAccessTime);
            *(e->pLastWriteTime) = FileTimeToInt64(fi.ftLastWriteTime);
            *(e->pSize) = ((__int64)fi.nFileSizeHigh << 32 & 0xFFFFFFFF00000000) | fi.nFileSizeLow;
            *(e->pAllocationSize) = *(e->pSize);

            //
            // If FileId is supported then set FileId for the file.
            //
            if (!fSupportFileIDs)
                *(e->pFileId) = 0;
            else
                *(e->pFileId) = ((__int64)fi.nFileIndexHigh << 32 & 0xFFFFFFFF00000000) | fi.nFileIndexLow;

            CloseHandle(hFile);
        }
        else
        {
            e->FileExists = FALSE;
        }

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
        LPWSTR Label = const_cast<LPWSTR>(L"Encrypted Drive");
        e->ResultCode = CopyStringToBuffer(e->Buffer, e->lenBuffer, Label);
        return e->ResultCode;
    }

    INT FireGetVolumeSize(CBFSGetVolumeSizeEventParams* e) override
    {
        __int64 FreeBytesToCaller, TotalBytes, FreeBytes;

        if (!GetDiskFreeSpaceEx(g_RootPath,
            (PULARGE_INTEGER)&FreeBytesToCaller,
            (PULARGE_INTEGER)&TotalBytes,
            (PULARGE_INTEGER)&FreeBytes))
        {
            e->ResultCode = ::GetLastError();
            return e->ResultCode;
        }

        INT SectorSize = GetSectorSize();

        assert(SectorSize != 0);

        *(e->pAvailableSectors) = FreeBytes / SectorSize;
        *(e->pTotalSectors) = TotalBytes / SectorSize;

        return 0;
    }

    INT FireIsDirectoryEmpty(CBFSIsDirectoryEmptyEventParams* e) override
    {
        return (e->ResultCode = IsEmptyDirectory(e->DirectoryName, e->IsEmpty));
    }

    INT FireOpenFile(CBFSOpenFileEventParams* e) override
    {
        EncryptContext* Context = NULL;

        if (e->FileContext == NULL)
        {
            Context = new EncryptContext(this, e->FileName, e->Attributes, e->DesiredAccess, e->NTCreateDisposition, false);

            if (Context != NULL && Context->Initialized())
            {
                e->FileContext = Context;
            }
            else
            {
                DWORD error = ::GetLastError();
                if (Context != NULL)
                    delete Context;
                e->ResultCode = error;
                return e->ResultCode;
            }

        }
        else
        {
            Context = (EncryptContext*)(e->FileContext);
            if (Context->IsReadOnly() && (e->DesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)))
            {
                e->ResultCode = ERROR_ACCESS_DENIED;
                return e->ResultCode;
            }
            Context->IncrementRef();
        }

        return 0;
    }

    INT FireReadFile(CBFSReadFileEventParams* e) override
    {
        EncryptContext* Context;

        Context = (EncryptContext*)(e->FileContext);
        if (Context == NULL)
        {
            e->ResultCode = E_FAIL;
            return e->ResultCode;
        }

        DWORD BytesRead;
        BytesRead = Context->Read(e->Position, e->Buffer, (DWORD)e->BytesToRead);

        e->ResultCode = ::GetLastError();

        if (e->ResultCode == 0)
            *(e->pBytesRead) = BytesRead;

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
        if (e->FileContext != NULL) {
            EncryptContext* Context = (EncryptContext*)(e->FileContext);

            Context->SetCurrentSize(e->Size);
        }

        return 0;
    }

    INT FireSetFileAttributes(CBFSSetFileAttributesEventParams* e) override
    {
        DWORD error = NO_ERROR;
        EncryptContext* Context;

        Context = (EncryptContext*)(e->FileContext);
        if (Context == NULL)
        {
            e->ResultCode = E_FAIL;
            return e->ResultCode;
        }

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
        if (e->Attributes != 0)
        {
            if (!SetFileAttributes(FName, e->Attributes))
            {
                error = ::GetLastError();
                goto end;
            }
        }

        FILETIME CreationTime, LastAccessTime, LastWriteTime;
        Int64ToFileTime(e->CreationTime, &CreationTime);
        Int64ToFileTime(e->LastAccessTime, &LastAccessTime);
        Int64ToFileTime(e->LastWriteTime, &LastWriteTime);
        if (!SetFileTime(Context->mHandle, &CreationTime, &LastAccessTime, &LastWriteTime))
        {
            error = ::GetLastError();
        }
    end:
        if (FName) free(FName);

        if (error != NO_ERROR)
        {
            e->ResultCode = error;
            return error;
        }

        return 0;
    }

    INT FireWriteFile(CBFSWriteFileEventParams* e) override
    {
        EncryptContext* Context;

        Context = (EncryptContext*)(e->FileContext);

        if (Context == NULL)
        {
            e->ResultCode = E_FAIL;
            return e->ResultCode;
        }

        DWORD BytesWritten = Context->Write(e->Position, (LPVOID)e->Buffer, (DWORD)e->BytesToWrite);

        e->ResultCode = ::GetLastError();
        if (e->ResultCode == 0)
            *(e->pBytesWritten) = BytesWritten;

        return e->ResultCode;
    }
};

const LPCWSTR program_name = L"{713CC6CE-B3E2-4fd9-838D-E28F558F6866}";
const LPCWSTR icon_id = L"0_EncDrive";

std::vector<uint8_t> XORData;

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
    printf("This demo shows how to mount a folder on your system as an encrypted drive.\n\n");
}

void usage(void)
{
    printf("Usage: encrypteddrive -<switch 1> [... -<switch N>] <root path> <mounting point>\n\n");
    printf("<Switches>\n");
    printf("  -p password - Set encryption password (mandatory switch)\n");
    printf("  -i icon_path - Set volume icon\n");
#ifndef WIN32
    printf("  -lc - Mount for current user only\n");
#endif
    printf("  -n - Mount as network volume\n");
#ifdef WIN32
    printf("  -drv {cab_file} - Install drivers from CAB file\n");
#endif
    printf("  -- Stop switches scanning\n\n");
    printf("Example: encrypteddrive -p test C:\\Root Y:\n\n");
}

// ----------------------------------------------------------------------------------

EncryptedDriveCBFS cbfs;

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

int main(int argc, char* argv[])
{
#ifndef WIN32
    struct pollfd cinfd[1];
    int count;
    int opt_debug = 0, opt_local = 0;
#else
    int drv_reboot = 0;
#endif
    LPCWSTR  opt_icon_path = nullptr, root_path = nullptr, mount_point = nullptr, opt_password = nullptr;

    int argi, arg_len,
        stop_opt = 0,
        mounted = 0, opt_network = 0;
    int flags = 0;

    banner();
    if (argc < 3)
    {
        usage();
#ifdef WIN32
        check_driver(cbcConstants::MODULE_DRIVER, L"CBFS");
#endif
        return 0;
    }

    int retVal;

    for (argi = 1; argi < argc; argi++)
    {
        arg_len = (int)strlen(argv[argi]);
        if (arg_len > 0)
        {
            if ((argv[argi][0] == '-') && !stop_opt)
            {
                if (arg_len < 2)
                    fprintf(stderr, strInvalidOption, argv[argi]);
                else
                {
                    if (optcmp(argv[argi], (char*)"--"))
                        stop_opt = 1;
                    else
                        if (optcmp(argv[argi], (char*)"-i"))
                        {
                            argi++;
                            if (argi < argc) {
                                cbt_string opt_icon_path_wstr = ConvertRelativePathToAbsolute(a2w(argv[argi]));
                                opt_icon_path = wcsdup(opt_icon_path_wstr.c_str());
                            }
                        }
                        else
                            if (optcmp(argv[argi], (char*)"-p"))
                            {
                                argi++;
                                if (argi < argc)
                                {
                                    opt_password = a2w(argv[argi]);
                                }
                            }
#ifndef WIN32
                            else if (optcmp(argv[argi], (char*)"-lc"))
                                opt_local = 1;
#endif
                            else if (optcmp(argv[argi], (char*)"-n"))
                                opt_network = 1;
#ifdef WIN32
                            else if (optcmp(argv[argi], (char*)"-drv"))
                            {
                                argi++;
                                if (argi < argc)
                                {
                                    printf("Installing drivers from '%s'\n", argv[argi]);
                                    cbt_string drv_path_wstr = ConvertRelativePathToAbsolute(a2w(argv[argi]));
                                    drv_reboot = cbfs.Install(drv_path_wstr.c_str(), program_name, NULL,
                                        cbcConstants::MODULE_DRIVER | cbcConstants::MODULE_HELPER_DLL,
                                        cbcConstants::INSTALL_REMOVE_OLD_VERSIONS);

                                    retVal = cbfs.GetLastErrorCode();
                                    if (0 != retVal)
                                    {
                                        if (retVal == ERROR_PRIVILEGE_NOT_HELD)
                                            fprintf(stderr, "Drivers are not installed due to insufficient privileges. Please, run installation with administrator rights");
                                        else
                                            fprintf(stderr, "Drivers are not installed, error: %s", cbfs.GetLastError());
                                        return retVal;
                                    }

                                    printf("Drivers installed successfully");
                                    if (drv_reboot != 0)
                                    {
                                        printf(", reboot is required\n");
                                        exit(0);
                                    }
                                    else
                                        printf("\n");
                                }
                            }
#endif
                            else
                                fprintf(stderr, strInvalidOption, argv[argi]);
                }
            }
            else
            {
                retVal = cbfs.Initialize(program_name);
                if (0 != retVal)
                {
                    fprintf(stderr, "Error: %s", cbfs.GetLastError());
                    return retVal;
                }

                if (opt_icon_path)
                {
                    retVal = cbfs.RegisterIcon(opt_icon_path, program_name, icon_id);
                    if (0 != retVal)
                    {
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

                if ((opt_password == nullptr) || (wcslen(opt_password) == 0))
                {
                    fprintf(stderr, "Error: password must be specified");
                    return -1;
                }

                // Initialize XOR data from password
                {
#ifdef _UNICODE
                    char ch[260];
                    char DefChar = ' ';
                    WideCharToMultiByte(CP_ACP, 0, opt_password, -1, ch, 260, &DefChar, NULL);

                    XORData.resize(strlen(ch));
                    memmove(&XORData[0], ch, strlen(ch));
#else
                    XORData.resize(strlen(opt_password));
                    memmove(&XORData[0], opt_password, strlen(opt_password));
#endif
                }

                retVal = cbfs.CreateStorage();
                if (0 != retVal)
                {
                    fprintf(stderr, "Error: %s", cbfs.GetLastError());
                    return retVal;
                }

                if (cbfs.IsIconRegistered(icon_id))
                    cbfs.SetIcon(icon_id);

                HANDLE hDir = INVALID_HANDLE_VALUE;

                wcscpy(g_RootPath, root_path);
                int len = (int)wcslen(g_RootPath);

                if ((g_RootPath[len - 1] == '\\') || (g_RootPath[len - 1] == '/'))
                {
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

                if (INVALID_HANDLE_VALUE == hDir)
                {
                    if (!CreateDirectory(g_RootPath, NULL))
                    {
                        fprintf(stderr, "Cannot create root directory, invalid path name: %d", GetLastError());
                        return 1;
                    }
                }
                else
                {
                    CloseHandle(hDir);
                }

                retVal = cbfs.MountMedia(0);
                if (0 == retVal)
                    printf("Media inserted in storage\n");
                else
                {
                    fprintf(stderr, "Error: %s", cbfs.GetLastError());
                    return retVal;
                }

#ifndef WIN32
                if (opt_local)
                    flags |= cbsConstants::STGMP_LOCAL;
				else
#endif
                if (opt_network)
                    flags |= cbcConstants::STGMP_NETWORK;
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
            }
        }
    }

    if (wcslen(root_path) == 0)
        fprintf(stderr, "EncryptedDrive: Invalid parameters, root path not specified\n");
    else
        if (!mounted)
        {
            if (mount_point == nullptr)
                fprintf(stderr, "EncryptedDrive: Invalid parameters, mounting point not specified\n");
            else
                fprintf(stderr, "EncryptedDrive: Failed to mount the disk\n");
        }
        else
        {
            printf("Press Enter to unmount disk\n");
#ifndef WIN32
            cinfd[0].fd = fileno(stdin);
            cinfd[0].events = POLLIN;
            while (1)
            {
                if (poll(cinfd, 1, 1000))
                {
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
    else
    {
        count = cbfs.GetMountingPointCount();
        if (count == 0)
        {
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

#ifdef _UNICODE
    Result = wcsstr(UpcasePath, L"$RECYCLE.BIN") != NULL ||
        wcsstr(UpcasePath, L"RECYCLER") != NULL;
#else
    Result = strstr(UpcasePath, "$RECYCLE.BIN") != NULL ||
        strstr(UpcasePath, "RECYCLER") != NULL;
#endif

    free(UpcasePath);
    return Result;
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

/****************************************************************************************************
*
* EncryptContext class implementation
*
*****************************************************************************************************/

EncryptContext::EncryptContext(CBFS* FS, LPCWSTR FileName, DWORD FileAttributes, INT DesiredAccess, INT NTCreateDisposition, BOOL New)
    :mInitialized(FALSE)
    , mRefCnt(0)
    , mBuffer(NULL)
    , mBufferSize(0)
    , mCurrentSize(0)
    , mHandle(INVALID_HANDLE_VALUE)
    , mFS(FS)
    , ReadOnly(FALSE)
{
    LPWSTR RootPath = NULL;
    SYSTEM_INFO SystemInfo;
    DWORD error = NO_ERROR, FileSize;
    LPWSTR FName = NULL;

    if (New)
    {
        FName = static_cast<LPWSTR>(malloc((wcslen(FileName) + wcslen(g_RootPath)) * sizeof(TCHAR) + sizeof(TCHAR)));

        assert(FName);

        wcscpy(FName, g_RootPath);

        wcscat(FName, FileName);

        if (FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!CreateDirectory(FName, NULL))
            {
                free(FName);
                FName = NULL;
                return;
            }

            mHandle = (PVOID)CreateFile(
                FName,
                GENERIC_READ/* | FILE_WRITE_ATTRIBUTES*/,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS,
                0);
        }
        else
        {
            mHandle = (PVOID)CreateFile(
                FName,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                CREATE_NEW,
                FileAttributes,
                0);
        }
    }
    else
    {
        FName = static_cast<LPWSTR>(malloc((wcslen(FileName) + wcslen(g_RootPath)) * sizeof(TCHAR) + sizeof(TCHAR)));

        assert(FName);

        wcscpy(FName, g_RootPath);

        wcscat(FName, FileName);

        if (FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (wcscmp(FileName, L"\\") &&
                (0xFFFFFFFF == GetFileAttributes(FName)))
            {
                SetLastError(ERROR_FILE_NOT_FOUND);
                free(FName);
                FName = NULL;
                return;
            }
            else
            {
                mHandle = (PVOID)CreateFile(
                    FName,
                    GENERIC_READ | FILE_WRITE_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS,
                    0);
            }
        }
        else
        {
            mHandle = (PVOID)CreateFile(
                FName,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,//FileAttributes,
                0);

            if (INVALID_HANDLE_VALUE == mHandle)
            {
                error = GetLastError();

                if (ERROR_ACCESS_DENIED == error && !(DesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)))
                {
                    mHandle = (PVOID)CreateFile(
                        FName,
                        GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,//FileAttributes,
                        0);
                    ReadOnly = TRUE;
                }
            }
        }
    }

    if ((mHandle == INVALID_HANDLE_VALUE) || (New && !SetFileAttributes(FName, FileAttributes & 0xffff)) ||
        (!New && (NTCreateDisposition != FILE_OPEN) && (NTCreateDisposition != FILE_OPEN_IF) && !SetFileAttributes(FName, FileAttributes & 0xffff))) // Attributes contains creation flags as well, which we need to strip
    {
        error = GetLastError();
        if (FName) free(FName);
        return;
    }

    if (FName) free(FName);

    mInitialIV = XORData;

    IncrementRef();

    GetSystemInfo(&SystemInfo);
    mBufferSize = SystemInfo.dwPageSize;

    //optional - the more buffer size, the faster read/write callback processing  
    mBufferSize <<= 4;

    // allocating buffer for read/write operations
    mBuffer = (PBYTE)VirtualAlloc(NULL, mBufferSize, MEM_COMMIT, PAGE_READWRITE);
    mBufferMask = (PBYTE)VirtualAlloc(NULL, mBufferSize + 16, MEM_COMMIT, PAGE_READWRITE);

    DWORD high = 0;
    FileSize = GetFileSize(mHandle, &high);

    if (INVALID_FILE_SIZE == FileSize)
    {
        error = GetLastError();
        //WCHAR text[MAX_PATH * 2];
        //wsprintfW(text, L"ERROR(%d)!!!!! GetFileSize %s", error, FileName);
        return;
    }

    SetCurrentSize(FileSize);
    if (RootPath) {
        free(RootPath);
    }

    mInitialized = TRUE;
}

EncryptContext::~EncryptContext()
{
    if (mBuffer != NULL)
        VirtualFree(mBuffer, mBufferSize, MEM_DECOMMIT);
    if (mBufferMask != NULL)
        VirtualFree(mBufferMask, mBufferSize + 16, MEM_DECOMMIT);
    if (mHandle != INVALID_HANDLE_VALUE)
        CloseHandle(mHandle);
    mHandle = INVALID_HANDLE_VALUE;
    mRefCnt = 0;
}

DWORD EncryptContext::Read(__int64 Position, PVOID Buffer, DWORD BytesToRead)
{
    PBYTE SysBuf = (PBYTE)Buffer;
    __int64 CurrPos = 0, StartPosition = Position;
    OVERLAPPED Overlapped;
    DWORD TotalRead = 0, Completed = 0, BufPos = 0, SysBufPos = 0;
    DWORD error = NO_ERROR;

    CurrPos = Position;
    BufPos = (DWORD)(Position - CurrPos);
    while (BytesToRead > 0)
    {
        // reading internal buffer
        memset(&Overlapped, 0, sizeof(Overlapped));
        Overlapped.Offset = (DWORD)(CurrPos & 0xFFFFFFFF);
        Overlapped.OffsetHigh = (DWORD)((CurrPos >> 32) & 0xFFFFFFFF);
        if (!ReadFile(mHandle, mBuffer, mBufferSize, &Completed,
            &Overlapped) || (Completed == 0)) {
            error = GetLastError();
            break;
        }
        DecryptBuffer(CurrPos, Completed);
        // copying part of internal bufer into system buffer

        if (Completed > BufPos)
            Completed -= BufPos;
        if (Completed > BytesToRead)
            Completed = BytesToRead;
        TotalRead += Completed;
        if (SysBuf) {
            memcpy(SysBuf + SysBufPos, mBuffer + BufPos, Completed);
        }
        BufPos = 0;
        // preparing to next part of data
        SysBufPos += Completed;
        BytesToRead -= min(Completed, BytesToRead);
        CurrPos += Completed;
    }
    if (error == ERROR_HANDLE_EOF) {

        if (0 == TotalRead) {
            SetLastError(ERROR_HANDLE_EOF);
            return 0;
        }
        else
            error = NO_ERROR;
    }
    if ((StartPosition + TotalRead) > mCurrentSize) {

        SetCurrentSize(StartPosition + TotalRead);

    }
    SetLastError(error);
    return TotalRead;
}


DWORD EncryptContext::Write(__int64 Position, PVOID Buffer, DWORD BytesToWrite)
{
    PBYTE SysBuf = (PBYTE)Buffer;
    OVERLAPPED Overlapped;
    __int64 CurrPos, CurrSize, StartPosition = Position;
    DWORD error = NO_ERROR, Completed = 0, BufPos = 0, SysBufPos = 0, TotalWritten = 0;

    CurrPos = Position;
    CurrSize = GetCurrentSize();
    while (BytesToWrite > 0)
    {
        // copying system buffer into internal bufer
        BufPos = (DWORD)(Position - CurrPos);
        Completed = mBufferSize - BufPos;
        Completed = min(Completed, BytesToWrite);
        memcpy(mBuffer + BufPos, SysBuf + SysBufPos, Completed);
        Completed = 0;
        DWORD ToWrite = min(BytesToWrite, mBufferSize);
        EncryptBuffer(CurrPos, ToWrite);

        // writing internal buffer
        memset(&Overlapped, 0, sizeof(Overlapped));
        Overlapped.Offset = (DWORD)(CurrPos & 0xFFFFFFFF);
        Overlapped.OffsetHigh = (DWORD)((CurrPos >> 32) & 0xFFFFFFFF);

        if (!WriteFile(mHandle,
            mBuffer,
            ToWrite,
            &Completed,
            &Overlapped)) {

            error = GetLastError();
            TotalWritten += Completed;
            break;
        }
        // preparing to next part of data
        Position += Completed;
        SysBufPos += Completed;
        BytesToWrite -= min(BytesToWrite, Completed);
        CurrPos += Completed;
        TotalWritten += Completed;
        if (ToWrite > Completed) break;
    }

    if (TotalWritten + StartPosition > mCurrentSize) {

        SetCurrentSize(StartPosition + TotalWritten);
    }
    SetLastError(error);
    return TotalWritten;
}


void AddInt64(std::vector<uint8_t>& iv, __int64 num)
{
    std::vector<uint8_t> numArr(8);
    int i, a, b;

    for (i = 7; i >= 0; i--)
    {
        numArr[i] = (byte)(num & 0xff);
        num = num >> 8;
    }

    a = (int)iv.size() - 8;
    b = 0;

    for (i = (int)iv.size() - 1; i >= 0; i--)
    {
        if (i > a)
            b = b + numArr[i - a];
        b = b + iv[i];
        iv[i] = (byte)(b & 0xff);
        b = b >> 8;
    }
}

void AddOne(std::vector<uint8_t>& iv)
{
    int b = 1;

    for (int i = (int)iv.size() - 1; i >= 0; i--)
    {
        b = b + iv[i];
        iv[i] = (byte)(b & 0xff);
        b = b >> 8;
    }
}

void EncryptContext::EncryptBuffer(__int64 position, DWORD size)
{
    __int64 blockNum;
    int i, offset, blockCount, blockLen;
    std::vector<uint8_t> iv(mInitialIV.size());

    blockLen = (int)mInitialIV.size();
    assert(blockLen > 0);
    blockNum = position / blockLen;
    blockCount = (int)((position - (__int64)blockNum * (__int64)blockLen + (__int64)size + (__int64)blockLen - 1) / blockLen);

    std::copy(mInitialIV.begin(), mInitialIV.begin() + blockLen, iv.begin());
    AddInt64(iv, blockNum);

    for (i = 0; i < blockCount; i++)
    {
        memcpy(&mBufferMask[i * blockLen], &iv[0], blockLen);
        AddOne(iv);
    }

    offset = (int)(position % blockLen);

    for (i = 0; i < (int)size; i++)
        mBuffer[i] ^= mBufferMask[i + offset];
}

void EncryptContext::DecryptBuffer(__int64 position, DWORD size)
{
    EncryptBuffer(position, size);
}


int EncryptContext::IncrementRef()
{
    mRefCnt++;
    return mRefCnt;
}


int EncryptContext::DecrementRef()
{
    if (mRefCnt > 0)
        mRefCnt--;
    return mRefCnt;
}
 

