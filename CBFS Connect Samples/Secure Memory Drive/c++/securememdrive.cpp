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

class SecureMemDriveCBFS : public CBFS
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

    bool SecureCheck(PSECURITY_DESCRIPTOR SecurityDescriptor, HANDLE ImpersonatedToken, DWORD AccessRights)
    {
        bool Allow = false;

        GENERIC_MAPPING mapping = { 0xFFFFFFFF };
        PRIVILEGE_SET privileges = { 0 };
        DWORD grantedAccess = 0, privilegesLength = sizeof(privileges);
        BOOL result = FALSE;

        mapping.GenericRead = FILE_GENERIC_READ;
        mapping.GenericWrite = FILE_GENERIC_WRITE;
        mapping.GenericExecute = FILE_GENERIC_EXECUTE;
        mapping.GenericAll = FILE_ALL_ACCESS;

        ::MapGenericMask(&AccessRights, &mapping);
        if (!::AccessCheck(SecurityDescriptor, ImpersonatedToken, AccessRights,
            &mapping, &privileges, &privilegesLength, &grantedAccess, &result)) {

            DWORD dwError = ::GetLastError();
        }
        else
            Allow = result != FALSE;

        return Allow;
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

    SecureMemDriveCBFS() : CBFS()
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

        //
        // Access Check
        //
        {
            bool Allow;

            DWORD AccessRights = (e->Attributes & FILE_ATTRIBUTE_DIRECTORY) ? FILE_ADD_SUBDIRECTORY : FILE_ADD_FILE;

            HANDLE hToken, hImpersonatedToken;
            hToken = (HANDLE)GetOriginatorToken();
            ::DuplicateToken(hToken, SecurityImpersonation, &hImpersonatedToken);

            Allow = SecureCheck(vdir->get_SecurityDescriptor(), hImpersonatedToken, AccessRights);
            ::CloseHandle(hImpersonatedToken);

            if (!Allow)
            {
                e->ResultCode = ERROR_ACCESS_DENIED;
                return 0;
            }
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

        if ((e->Restart || (e->EnumerationContext == NULL)) && !ExactMatch)
            ResetEnumeration = TRUE;

        if (e->Restart && (e->EnumerationContext != NULL))
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
        }
        else
        {
            pInfo = (PENUM_INFO)(e->EnumerationContext);
            vdir = pInfo->vfile;
        }

        if (ResetEnumeration)
            pInfo->Index = 0;

        if (pInfo->ExactMatch)
            e->FileFound = FALSE;
        else {

            e->FileFound = ExactMatch ?
                vdir->get_Context()->GetFile(e->Mask, vfile) : vdir->get_Context()->GetFile(pInfo->Index, vfile);
        }

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

                pInfo->vfile = vdir;
            }
        }

        ++pInfo->Index;
        pInfo->ExactMatch = ExactMatch;

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
        VirtualFile* vfile = NULL;

        if (e->FileContext)
            vfile = (VirtualFile*)e->FileContext;
        else
        {
            if (!FindVirtualFile(e->FileName, vfile))
            {
                e->ResultCode = ERROR_FILE_NOT_FOUND;
                return 0;
            }
        }


        //
        // Access Check
        //
        {
            bool Allow;

            DWORD AccessRights = e->DesiredAccess;

            HANDLE hToken, hImpersonatedToken;
            hToken = (HANDLE)GetOriginatorToken();
            ::DuplicateToken(hToken, SecurityImpersonation, &hImpersonatedToken);

            Allow = SecureCheck(vfile->get_SecurityDescriptor(), hImpersonatedToken, AccessRights);

            ::CloseHandle(hImpersonatedToken);

            if (!Allow)
            {
                e->ResultCode = ERROR_ACCESS_DENIED;
                return 0;
            }
        }


        if (e->FileContext == NULL)
            e->FileContext = vfile;

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

    INT FireSetFileSecurity(CBFSSetFileSecurityEventParams* e) override
    {
        VirtualFile* vfile = (VirtualFile*)(e->FileContext);

        e->ResultCode = vfile->set_SecurityDescriptor(e->SecurityInformation, (PSECURITY_DESCRIPTOR)e->SecurityDescriptor, e->Length);

        return 0;
    }

    INT FireGetFileSecurity(CBFSGetFileSecurityEventParams* e) override
    {
        VirtualFile* vfile = (VirtualFile*)(e->FileContext);

        e->ResultCode = vfile->get_SecurityDescriptor(e->SecurityInformation, e->SecurityDescriptor, e->BufferLength, &e->DescriptorLength);

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
    printf("CBFS Connect Copyright (c) Callback Technologies, Inc.\n\n");
    printf("This demo shows how to use Windows security to check access rights during file events.\n\n");
}

void usage(void)
{
    printf("Usage: securememdrive [-<switch 1> ... -<switch N>] <mounting point>\n\n");
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
    printf("Example: securememdrive Y:\n\n");
}

// ----------------------------------------------------------------------------------

SecureMemDriveCBFS cbfs;

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
    LPCWSTR  opt_icon_path = NULL, root_path = NULL, mount_point = NULL;
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
                            LPCWSTR driver_path = (driver_path_wstr.c_str());
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
                    retVal = cbfs.RegisterIcon(opt_icon_path, g_Guid, icon_id);
                    if (0 != retVal) {
                        fprintf(stderr, "Error: %s", cbfs.GetLastError());
                        return retVal;
                    }
                }

                cbfs.SetFileSystemName(L"NTFS");
                cbfs.SetUseWindowsSecurity(TRUE);
                cbfs.SetFireAllOpenCloseEvents(TRUE);

                cbt_string mount_point_wstr = ConvertRelativePathToAbsolute(a2w(argv[argi]), true);
                if (mount_point_wstr.empty()) {
                    printf("Error: Invalid Mounting Point Path\n");
                    exit(1);
                }
                mount_point = wcsdup(mount_point_wstr.c_str());

                retVal = cbfs.CreateStorage();
                if (0 != retVal) {
                    fprintf(stderr, "Error: %s", cbfs.GetLastError());
                    return retVal;
                }

                if (cbfs.IsIconRegistered(icon_id))
                    cbfs.SetIcon(icon_id);

                if (NULL == g_DiskContext) {
                    g_DiskContext = new VirtualFile(L"\\");
                    g_DiskContext->set_FileAttributes(FILE_ATTRIBUTE_DIRECTORY);
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
 
 
 

