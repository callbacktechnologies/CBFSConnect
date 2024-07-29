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

// Enable in order to support reparse points (symbolic links, mounting points, etc.) support. Also required for NFS sharing
//#define SUPPORT_REPARSE_POINTS 0
#define SUPPORT_REPARSE_POINTS 1
#include <stdlib.h>
#include <stdio.h>
#include <strsafe.h>
#include <limits.h>
#include <assert.h>
#include <iostream>
#include <Shlwapi.h>
#include <filesystem>
#include <string>
#include <tchar.h>

#ifndef WIN32
#include <unistd.h>
#include <poll.h>
#include <dirent.h>

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/types.h>
#endif

#include "cbfsconnectcommon.h"
#include "virtualfile.h"

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

typedef struct
{
    VirtualFile* vfile;
    INT Index;
    BOOL ExactMatch;
}   ENUM_INFO, * PENUM_INFO;

VirtualFile* g_DiskContext = NULL;

//support routines
BOOL FindVirtualFile(LPCTSTR FileName, VirtualFile*& vfile);
BOOL FindVirtualDirectory(LPCTSTR FileName, VirtualFile*& vfile);
BOOL GetParentVirtualDirectory(LPCTSTR FileName, VirtualFile*& vfile);
LPCTSTR GetFileName(LPCTSTR fullpath);
void RemoveAllFiles(VirtualFile* root);
__int64 CalculateFolderSize(VirtualFile* root);
VOID CreateReparsePoint(LPWSTR SourcePath, LPWSTR ReparsePath);

class MemDriveCBFS : public CBFS
{

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

public: // Events

    int GetSectorSize()
    {
        LPWSTR SS = this->Config(L"SectorSize");
        if (SS != NULL && wcslen(SS) > 0)
            return _wtoi(SS);
        else
            return 0;
    }

    MemDriveCBFS() : CBFS()
    {
    }

    INT FireMount(CBFSMountEventParams* e) override
    {
        return 0;
    }

    INT FireUnmount(CBFSUnmountEventParams* e) override
    {
        return 0;
    }

    INT FireCanFileBeDeleted(CBFSCanFileBeDeletedEventParams* e) override
    {
        VirtualFile* vfile = NULL;

        e->CanBeDeleted = FindVirtualFile(e->FileName, vfile);
        if (e->CanBeDeleted)
        {
            if (((vfile->get_FileAttributes() & FILE_ATTRIBUTE_DIRECTORY) != 0) && ((vfile->get_FileAttributes() & FILE_ATTRIBUTE_REPARSE_POINT) == 0) && vfile->get_Context()->GetFile(0, vfile))
            {
                e->CanBeDeleted = FALSE;
                e->ResultCode = ERROR_DIR_NOT_EMPTY;
            }
        }
        else
            e->ResultCode = ERROR_FILE_NOT_FOUND;

        return e->ResultCode;
    }

    INT FireCloseDirectoryEnumeration(CBFSCloseDirectoryEnumerationEventParams* e) override
    {
        if (e->EnumerationContext != 0)
        {
            PENUM_INFO pInfo = (PENUM_INFO)(e->EnumerationContext);
            free(pInfo);
        }

        return 0;
    }

    INT FireCloseFile(CBFSCloseFileEventParams* e) override
    {
        return 0;
    }

    INT FireCreateFile(CBFSCreateFileEventParams* e) override
    {
        assert(e->FileInfo);

        VirtualFile* vfile = NULL, * vdir = NULL;
        FILETIME ft;

        LPCTSTR FileName = e->FileName;
        if (!GetParentVirtualDirectory(FileName, vdir))
        {
            e->ResultCode = ERROR_INVALID_PARAMETER;
            return e->ResultCode;
        }

        vfile = new VirtualFile(GetFileName(FileName));

        vfile->set_FileAttributes(e->Attributes & 0xFFFF);  // Attributes contains creation flags as well, which we need to strip

        GetSystemTimeAsFileTime(&ft);

        vfile->set_CreationTime(ft);

        vfile->set_LastAccessTime(ft);

        vfile->set_LastWriteTime(ft);

        vdir->AddFile(vfile);

        e->FileContext = vfile;

        return 0;
    }

    INT FireDeleteFile(CBFSDeleteFileEventParams* e) override
    {
        VirtualFile* vfile = NULL;

        LPCWSTR FileName = e->FileName;
        if (FindVirtualFile(FileName, vfile))
        {
            vfile->Remove();
            delete vfile;
        }

        return 0;
    }

    INT FireEnumerateDirectory(CBFSEnumerateDirectoryEventParams* e) override
    {
        VirtualFile* vdir = NULL, * vfile = NULL;
        PENUM_INFO pInfo = NULL;
        BOOL ResetEnumeration = FALSE;
        BOOL ExactMatch = FALSE;

        e->FileFound = FALSE;

        ExactMatch = (wcschr(e->Mask, '*') == NULL) && (wcschr(e->Mask, '?') == NULL);

        if (e->Restart || (e->EnumerationContext == NULL) || (ExactMatch != ((PENUM_INFO)(e->EnumerationContext))->ExactMatch))
            ResetEnumeration = TRUE;

        if (ResetEnumeration && (e->EnumerationContext != NULL))
        {
            free((PENUM_INFO)(e->EnumerationContext));

            e->EnumerationContext = NULL;
        }
        if (e->EnumerationContext == NULL)
        {
            pInfo = static_cast<PENUM_INFO>(malloc(sizeof(ENUM_INFO)));

            assert(pInfo);

            e->EnumerationContext = pInfo;

            ZeroMemory(pInfo, sizeof(ENUM_INFO));

            LPCWSTR DirName = e->DirectoryName;

            BOOL find = FindVirtualFile(DirName, vdir);

            assert(find);

            pInfo->vfile = vdir;
            pInfo->ExactMatch = ExactMatch;

        }
        else
        {
            pInfo = (PENUM_INFO)(e->EnumerationContext);
            vdir = pInfo->vfile;
        }

        if (pInfo->ExactMatch)
        {
            if (pInfo->Index > 0)
                e->FileFound = false;
            else
                e->FileFound = vdir->get_Context()->GetFile(e->Mask, vfile);
        }
        else
            e->FileFound = vdir->get_Context()->GetFile(pInfo->Index, vfile);

        if (e->FileFound)
        {
            e->ResultCode = CopyStringToBuffer(e->FileName, e->lenFileName, vfile->get_Name());

            if (e->ResultCode == 0)
            {
                *(e->pCreationTime) = FileTimeToInt64(vfile->get_CreationTime());

                *(e->pLastAccessTime) = FileTimeToInt64(vfile->get_LastAccessTime());

                *(e->pLastWriteTime) = FileTimeToInt64(vfile->get_LastWriteTime());

                *(e->pSize) = vfile->get_Size();

                *(e->pAllocationSize) = vfile->get_AllocationSize();

                *(e->pFileId) = 0;

                *(e->pAttributes) = vfile->get_FileAttributes();

                if ((*(e->pAttributes) & FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT)
                    *(e->pReparseTag) = vfile->get_ReparseTag();
                pInfo->vfile = vdir;
            }
        }

        pInfo->Index++;        

        return 0;
    }

    INT FireGetFileInfo(CBFSGetFileInfoEventParams* e) override
    {
        e->FileExists = FALSE;

        VirtualFile* vfile = NULL;

        LPCWSTR FileName = e->FileName;
        if (FindVirtualFile(FileName, vfile))
        {
            e->FileExists = TRUE;

            *(e->pCreationTime) = FileTimeToInt64(vfile->get_CreationTime());

            *(e->pLastAccessTime) = FileTimeToInt64(vfile->get_LastAccessTime());

            *(e->pLastWriteTime) = FileTimeToInt64(vfile->get_LastWriteTime());

            *(e->pSize) = vfile->get_Size();

            *(e->pAllocationSize) = vfile->get_AllocationSize();

            *(e->pFileId) = 0;

            *(e->pAttributes) = vfile->get_FileAttributes();

            *(e->pReparseTag) = vfile->get_ReparseTag();
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
        LPWSTR Label = const_cast<LPWSTR>(L"CBFS Connect Virtual Disk");
        e->ResultCode = CopyStringToBuffer(e->Buffer, e->lenBuffer, Label);
        return e->ResultCode;
    }

    INT FireGetVolumeSize(CBFSGetVolumeSizeEventParams* e) override
    {
        MEMORYSTATUS status;
        GlobalMemoryStatus(&status);
        INT SectorSize = GetSectorSize();

        *(e->pTotalSectors) = status.dwTotalVirtual / SectorSize;
        *(e->pAvailableSectors) = (status.dwTotalVirtual - CalculateFolderSize(g_DiskContext) + SectorSize / 2) / SectorSize;

        return 0;
    }

    INT FireIsDirectoryEmpty(CBFSIsDirectoryEmptyEventParams* e) override
    {
        VirtualFile* vdir = NULL, * vfile = NULL;

        e->IsEmpty = FALSE;

        LPCTSTR DirName = e->DirectoryName;
        if (FindVirtualFile(DirName, vdir)) {

            e->IsEmpty = vdir->get_Context()->IsEmpty();
        }

        return 0;
    }

    INT FireOpenFile(CBFSOpenFileEventParams* e) override
    {
        if (e->FileContext == NULL)
        {
            VirtualFile* vfile = NULL;

            LPCWSTR FileName = e->FileName;
            if (FindVirtualFile(FileName, vfile))
            {
                e->FileContext = vfile;
            }
        }

        return 0;
    }

    INT FireReadFile(CBFSReadFileEventParams* e) override
    {
        VirtualFile* vfile = (VirtualFile*)(e->FileContext);

        assert(vfile);
        DWORD BytesRead;
        vfile->Read(e->Buffer, (LONG)e->Position, (INT)e->BytesToRead, &BytesRead);
        *(e->pBytesRead) = BytesRead;

        return 0;
    }

    INT FireRenameOrMoveFile(CBFSRenameOrMoveFileEventParams* e) override
    {
        VirtualFile* vfile = NULL, * vdir = NULL;

        LPCWSTR FileName = e->FileName;
        if (FindVirtualFile(FileName, vfile))
        {
            LPCWSTR NewFileName = e->NewFileName;
            if (FindVirtualDirectory(NewFileName, vdir))
            {
                vfile->Remove();

                vfile->Rename(GetFileName(NewFileName));//obtain file name from the path;

                vdir->AddFile(vfile);
            }
        }

        return 0;
    }

    INT FireSetAllocationSize(CBFSSetAllocationSizeEventParams* e) override
    {
        VirtualFile* vfile = (VirtualFile*)(e->FileContext);

        assert(vfile);
        vfile->set_AllocationSize(e->AllocationSize);

        return 0;
    }

    INT FireSetFileSize(CBFSSetFileSizeEventParams* e) override
    {
        VirtualFile* vfile = (VirtualFile*)(e->FileContext);

        assert(vfile);

        vfile->set_Size(e->Size);

        return 0;
    }

    INT FireSetFileAttributes(CBFSSetFileAttributesEventParams* e) override
    {
        VirtualFile* vfile = (VirtualFile*)(e->FileContext);

        assert(vfile);

        // the case when Attributes == 0 indicates that file attributes
        // not changed during this callback

        if (e->Attributes != 0)
        {
            vfile->set_FileAttributes(e->Attributes);
        }
        if (e->CreationTime != 0)
        {
            FILETIME ft;
            Int64ToFileTime(e->CreationTime, &ft);
            vfile->set_CreationTime(ft);
        }
        if (e->LastAccessTime != 0)
        {
            FILETIME ft;
            Int64ToFileTime(e->LastAccessTime, &ft);
            vfile->set_LastAccessTime(ft);
        }
        if (e->LastWriteTime != 0)
        {
            FILETIME ft;
            Int64ToFileTime(e->LastWriteTime, &ft);
            vfile->set_LastWriteTime(ft);
        }

        return 0;
    }

    INT FireWriteFile(CBFSWriteFileEventParams* e) override
    {
        VirtualFile* vfile = (VirtualFile*)(e->FileContext);
        assert(vfile);

        *(e->pBytesWritten) = 0;

        DWORD BytesWritten;
        vfile->Write((LPVOID)e->Buffer, (LONG)e->Position, (INT)e->BytesToWrite, &BytesWritten);
        *(e->pBytesWritten) = BytesWritten;

        return 0;
    }

    INT FireGetReparsePoint(CBFSGetReparsePointEventParams* e) override
    {
        VirtualFile* vfile = NULL;
        WORD lengthReturned = 0;

        if (!SUPPORT_REPARSE_POINTS)
            return 0;

        if (e->FileContext != NULL)
        {
            vfile = (VirtualFile*)(e->FileContext);
        }
        else
        {
            if (!FindVirtualFile(e->FileName, vfile))
            {
                e->ResultCode = ERROR_FILE_NOT_FOUND;
                return 0;
            }
        }

        if ((vfile->get_FileAttributes() & FILE_ATTRIBUTE_REPARSE_POINT) != FILE_ATTRIBUTE_REPARSE_POINT)
        {
            e->ResultCode = ERROR_NOT_A_REPARSE_POINT;
            return 0;
        }
        lengthReturned = min(e->ReparseBufferLength, vfile->get_ReparseBufferLength());

        CopyMemory(e->ReparseBuffer, vfile->get_ReparseBuffer(), lengthReturned);

        if (vfile->get_ReparseBufferLength() > e->ReparseBufferLength)
        {
            e->ResultCode = ERROR_MORE_DATA;
            return 0;
        }
        e->ReparseBufferLength = lengthReturned;

        return 0;
    }


    INT FireSetReparsePoint(CBFSSetReparsePointEventParams* e) override
    {
        if (!SUPPORT_REPARSE_POINTS)
            return 0;

        VirtualFile* vfile = (VirtualFile*)(e->FileContext);
        assert(vfile);

        if (vfile->get_ReparseBuffer() != NULL)
        {
            if (vfile->get_ReparseTag() != e->ReparseTag)
            {
                e->ResultCode = ERROR_REPARSE_TAG_MISMATCH;
                return 0;
            }
        }

        PVOID buffer = malloc(e->ReparseBufferLength);
        CopyMemory(buffer, e->ReparseBuffer, e->ReparseBufferLength);
        vfile->set_ReparseBuffer(buffer);
        vfile->set_ReparseBufferLength(e->ReparseBufferLength);
        vfile->set_FileAttributes(vfile->get_FileAttributes() | FILE_ATTRIBUTE_REPARSE_POINT);
        vfile->set_ReparseTag(e->ReparseTag);

        return 0;
    }

    INT FireDeleteReparsePoint(CBFSDeleteReparsePointEventParams* e) override
    {
        if (!SUPPORT_REPARSE_POINTS)
            return 0;

        VirtualFile* vfile = (VirtualFile*)(e->FileContext);
        assert(vfile);

        if ((vfile->get_FileAttributes() & FILE_ATTRIBUTE_REPARSE_POINT) != FILE_ATTRIBUTE_REPARSE_POINT)
        {
            e->ResultCode = ERROR_NOT_A_REPARSE_POINT;
            return 0;
        }

        PVOID buffer = vfile->get_ReparseBuffer();
        vfile->set_ReparseBuffer(NULL);
        assert(buffer);
        free(buffer);
        vfile->set_FileAttributes(vfile->get_FileAttributes() & FILE_ATTRIBUTE_REPARSE_POINT);

        return 0;
    }
};

LPCWSTR g_Guid = L"{713CC6CE-B3E2-4fd9-838D-E28F558F6866}";
LPCWSTR icon_id = L"0_MemDrive";

const LPCSTR strSuccess = "Success";
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
    printf("This demo shows how to create a virtual drive that is stored in memory.\n\n");
}

void usage(void)
{
    printf("Usage: memdrive [-<switch 1> ... -<switch N>] <mounting point>\n\n");
    printf("<Switches>\n");
    printf("  -i icon_path - Set volume icon\n");
#ifdef WIN32
    printf("  -lc - Mount for current user only\n");
#endif
    printf("  -n - Mount as network volume\n");
#ifdef WIN32
    printf("  -drv {cab_file} - Install drivers from CAB file\n");
#endif
    printf("  -- Stop switches scanning\n\n");
    printf("Example: memdrive Y:\n\n");
}

// ----------------------------------------------------------------------------------

MemDriveCBFS cbfs;

#ifdef WIN32
#define R_OK 0
#define access(filename, mode) (GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES ? 0 : 1)

void check_driver(INT module, LPCWSTR module_name)
{
    INT state;

    state = cbfs.GetDriverStatus(g_Guid, module);
    if (state == SERVICE_RUNNING)
    {
        LONG64 version;
        version = cbfs.GetModuleVersion(g_Guid, module);
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
                return _T("");
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
                    sout << L"The path '" << res << L"' cannot be equal to the drive letter" << std::endl;
                    return _T("");
                }
                return path;
            }
            wchar_t currentDir[_MAX_PATH];
            const char pathSeparator = '\\';
            if (_wgetcwd(currentDir, _MAX_PATH) == nullptr) {
                sout << "Error getting current directory." << std::endl;
                return _T("");
            }
#else
            char currentDir[PATH_MAX];
            const char pathSeparator = '/';
            if (getcwd(currentDir, sizeof(currentDir)) == nullptr) {
                sout << "Error getting current directory." << std::endl;
                return _T("");
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
        return _T("");
    }
    return path;
}

int main(int argc, char* argv[]) {
#ifndef WIN32
    struct pollfd cinfd[1];
    INT count;
    int opt_debug = 0;
#else
    INT drv_reboot = 0;
#endif
    LPCWSTR opt_icon_path = NULL, root_path = NULL, mount_point = NULL;
    INT argi, arg_len, stop_opt = 0, mounted = 0, opt_network = 0, opt_local = 0;
    INT flags = 0;

    banner();
    if (argc < 2) {
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
                            cbt_string opt_icon_path_wstr = ConvertRelativePathToAbsolute(a2w(argv[argi]));
                            if (opt_icon_path_wstr.empty()) {
                                printf("Error: Invalid Icon Path\n");
                                exit(1);
                            }
                            opt_icon_path = wcsdup(opt_icon_path_wstr.c_str());
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
                            if (driver_path_wstr.empty()) {
                                printf("Error: Invalid Driver Path\n");
                                exit(1);
                            }
                            LPCWSTR driver_path = wcsdup(driver_path_wstr.c_str());
                            drv_reboot = cbfs.Install(driver_path, g_Guid, NULL,
                                cbcConstants::MODULE_DRIVER | cbcConstants::MODULE_HELPER_DLL,
                                cbcConstants::INSTALL_REMOVE_OLD_VERSIONS);

                            retVal = cbfs.GetLastErrorCode();
                            if (0 != retVal) {
                                if (retVal == ERROR_PRIVILEGE_NOT_HELD)
                                    fprintf(stderr, "Drivers are not installed due to insufficient privileges. Please, run installation with administrator rights");
                                else
                                    fprintf(stderr, "Drivers are not installed, error: %s", cbfs.GetLastError());
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
                retVal = cbfs.Initialize(g_Guid);
                if (0 != retVal) {
                    fprintf(stderr, "Error: %s", cbfs.GetLastError());
                    return retVal;
                }

                if (opt_icon_path) {
                    if (cbfs.RegisterIcon(opt_icon_path, g_Guid, icon_id))
                    {
                        printf("The icon installed successfully, reboot is required for the icon to be displayed\n");
                    }
                }

                cbt_string mount_point_wstr = ConvertRelativePathToAbsolute(a2w(argv[argi]), true);
                if (mount_point_wstr.empty()) {
                    printf("Error: Invalid Mounting Point Path\n");
                    exit(1);
                }
                mount_point = wcsdup(mount_point_wstr.c_str());

                cbfs.SetUseReparsePoints(SUPPORT_REPARSE_POINTS);
                retVal = cbfs.CreateStorage();
                if (0 != retVal) {
                    fprintf(stderr, "Error: %s", cbfs.GetLastError());
                    return retVal;
                }

                if (cbfs.IsIconRegistered(icon_id))
                    cbfs.SetIcon(icon_id);

                if (NULL == g_DiskContext) {
                    g_DiskContext = new VirtualFile(L"\\");
                }

                VirtualFile* vDir = new VirtualFile(L"Sample");
                vDir->set_FileAttributes(FILE_ATTRIBUTE_DIRECTORY);
                g_DiskContext->AddFile(vDir);

                VirtualFile* vFile = new VirtualFile(L"TestFile_1.txt");
                vFile->set_FileAttributes(FILE_ATTRIBUTE_NORMAL);
                LPSTR header = "This is the\nheader for the file.\n-------------------\n";
                DWORD written = 0;
                vFile->Write(header, 0, ((SHORT)strlen(header) + 1), &written);
                vDir->AddFile(vFile);

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
                if (cbfs.AddMountingPoint(mount_point, flags, 0) == 0) {

                    mounted = 1;
                    VirtualFile* root = NULL;
                    if (FindVirtualDirectory(L"\\", root)) {
                   
                        VirtualFile* vFullPathSymLink = new VirtualFile(L"full_path_reparse_to_TestFile_1.txt");
                        root->AddFile(vFullPathSymLink);
                        vFullPathSymLink->CreateReparsePoint(L".\\Sample\\TestFile_1.txt");
                    }
                    LPWSTR destPath = L".\\Sample\\TestFile_1.txt";
                    LPWSTR srcFile = L"\\relative_path_reparse_to_TestFile_1.txt";

                    LPWSTR srcPath = (LPWSTR)malloc((wcslen(mount_point) + wcslen(srcFile) + 1) * sizeof(WCHAR));
                    wcscpy(srcPath, mount_point);
                    wcscat(srcPath, srcFile);
                    CreateReparsePoint(srcPath, destPath);
                    free(srcPath);
                }
                break;
            }
        }
    }

    if (!mounted) {
        if (mount_point == NULL)
            fprintf(stderr, "Virtual Disk: Invalid parameters, mounting point not specified\n");
        else
            fprintf(stderr, "Virtual Disk: Failed to mount the disk\n");
    }
    else {
        printf("Press Enter to unmount the disk\n");
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

BOOL FindVirtualFile(LPCWSTR FileName, VirtualFile*& vfile)
{
    assert(FileName);

    BOOL result = TRUE;

    VirtualFile* root = g_DiskContext;

    LPWSTR buffer = (LPWSTR)malloc((wcslen(FileName) + 1) * sizeof(WCHAR));

    assert(buffer);

    wcscpy(buffer, FileName);

    PWCHAR token = _wcstok(buffer, L"\\");

    vfile = root;

    while (token != NULL)
    {
        if (result = root->get_Context()->GetFile(token, vfile))
        {
            root = vfile;
        }
        token = _wcstok(NULL, L"\\");
    }
    free(buffer);

    return result;
}

//-----------------------------------------------------------------------------------------------------------

BOOL GetParentVirtualDirectory(LPCWSTR FileName, VirtualFile*& vfile)
{
    assert(FileName);

    LPCWSTR result = GetFileName(FileName);

    LPWSTR buffer = (LPWSTR)malloc((result - FileName + 1) * sizeof(WCHAR));

    memcpy(buffer, FileName, (result - FileName) * sizeof(TCHAR));

    buffer[result - FileName] = 0;

    BOOL find = FindVirtualFile(buffer, vfile);

    free(buffer);

    return find;
}

//-----------------------------------------------------------------------------------------------------------

BOOL FindVirtualDirectory(LPCWSTR FileName, VirtualFile*& vfile)
{
    assert(FileName);

    BOOL find = FALSE;

    if (FindVirtualFile(FileName, vfile) &&
        (vfile->get_FileAttributes() & FILE_ATTRIBUTE_DIRECTORY)) {

        return TRUE;
    }
    else {

        find = GetParentVirtualDirectory(FileName, vfile);
    }
    assert(find == TRUE);
    return find;
}


//-----------------------------------------------------------------------------------------------------------
LPCWSTR GetFileName(LPCWSTR fullpath)
{
    assert(fullpath);

    LPCWSTR result = &fullpath[wcslen(fullpath)];

    while (--result != fullpath)
    {
        if (*result == '\\')
            break;
    }
    return ++result;
}
//-----------------------------------------------------------------------------------------------------------
void RemoveAllFiles(VirtualFile* root)
{
    assert(root);
    VirtualFile* vfile;

    root->get_Context()->ResetEnumeration();

    while (root->get_Context()->GetNextFile(vfile))
    {
        if (vfile->get_FileAttributes() & FILE_ATTRIBUTE_DIRECTORY)
        {
            RemoveAllFiles(vfile);
        }
        vfile->Remove();

        delete vfile;
    }
}

//-----------------------------------------------------------------------------------------------------------

__int64 CalculateFolderSize(VirtualFile* root)
{
    if (root == NULL)
        return 0;
    VirtualFile* vfile;
    __int64 DiskSize = 0;
    INT Index = 0;

    while (root->get_Context()->GetFile(Index++, vfile))
    {
        if (vfile->get_FileAttributes() & FILE_ATTRIBUTE_DIRECTORY)
        {
            DiskSize += CalculateFolderSize(vfile);
        }
        DiskSize += (vfile->get_AllocationSize() + cbfs.GetSectorSize() - 1) & ~(cbfs.GetSectorSize() - 1);
    }
    return DiskSize;
} 
 
//-----------------------------------------------------------------------------------------------------------

#define PATH_GLOBAL_PREFIX  L"\\??\\"
#define SYMLINK_FLAG_RELATIVE  0x00000001 //The substitute name is a path name relative to the directory containing the symbolic link.

typedef struct _REPARSE_DATA_BUFFER {
    ULONG  ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union {
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG  Flags;
            WCHAR  PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR  PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
            UCHAR DataBuffer[1];
        } GenericReparseBuffer;
    } DUMMYUNIONNAME;
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

VOID CreateReparsePoint(LPWSTR SourcePath, LPWSTR ReparsePath)
{
    PREPARSE_DATA_BUFFER reparseDataBuffer = NULL;
    SHORT linkLength = 0, reparseDataBufferLength = 0;
    BOOL relative = FALSE;
    LPWSTR reparseTarget = NULL;
    DWORD nBytesReturned = 0;

    if (ReparsePath[0] == '.')
    {
        reparseTarget = ReparsePath;
        relative = TRUE;
    }
    else
    {
        reparseTarget = (LPWSTR)malloc((sizeof(PATH_GLOBAL_PREFIX) + wcslen(ReparsePath) + 1) * sizeof(WCHAR));
        wcscpy(reparseTarget, PATH_GLOBAL_PREFIX);
        wcscat(reparseTarget, ReparsePath);
    }
    linkLength = (SHORT)wcslen(reparseTarget) * sizeof(WCHAR);
    reparseDataBufferLength = FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer) + linkLength;

    reparseDataBuffer = (PREPARSE_DATA_BUFFER)malloc(reparseDataBufferLength);

    reparseDataBuffer->ReparseTag = IO_REPARSE_TAG_SYMLINK;
    reparseDataBuffer->ReparseDataLength = reparseDataBufferLength - FIELD_OFFSET(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer);
    reparseDataBuffer->Reserved = 0;
    reparseDataBuffer->SymbolicLinkReparseBuffer.Flags = relative ? SYMLINK_FLAG_RELATIVE : 0;
    reparseDataBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength = linkLength;
    reparseDataBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
    reparseDataBuffer->SymbolicLinkReparseBuffer.PrintNameLength = 0;
    reparseDataBuffer->SymbolicLinkReparseBuffer.PrintNameOffset = linkLength;
    wcscpy(reparseDataBuffer->SymbolicLinkReparseBuffer.PathBuffer, reparseTarget);

    HANDLE hFile = CreateFile(SourcePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_NEW,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
        NULL);

    BOOL ioResult = DeviceIoControl(hFile,
        FSCTL_SET_REPARSE_POINT,
        reparseDataBuffer,
        reparseDataBufferLength,
        NULL,
        0,
        &nBytesReturned,
        NULL);
    if (hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);
}
 
 
 

