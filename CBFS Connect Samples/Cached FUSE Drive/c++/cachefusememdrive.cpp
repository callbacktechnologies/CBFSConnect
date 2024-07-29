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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h> 
#include <time.h> 
#include <tchar.h>
#include <iostream>
#include <Shlwapi.h>
#include <string>
#include <iostream>
#include <filesystem>

#ifdef UNIX
#include <unistd.h>
#include <poll.h>
#include <dirent.h>
#include <errno.h>

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

#include "cbfsconnectcommon.h"
#include "virtualfile.h"

#ifdef _UNICODE
#include "../../include/unicode/fuse.h"
#include "../../include/unicode/cbcache.h"
#else
#include "../../include/fuse.h"
#include "../../include/cbcache.h"
#endif


#ifndef FALLOC_FL_KEEP_SIZE
#define FALLOC_FL_KEEP_SIZE 1 
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
    int Index;
    bool ExactMatch;
}   ENUM_INFO, * PENUM_INFO;

VirtualFile* g_DiskContext = NULL;

//support routines
bool FindVirtualFile(const fuse_char* FileName, VirtualFile*& vfile);
bool FindVirtualDirectory(const fuse_char* FileName, VirtualFile*& vfile);
bool GetParentVirtualDirectory(const fuse_char* FileName, VirtualFile*& vfile);
const fuse_char* GetFileName(const fuse_char* fullpath);
void RemoveAllFiles(VirtualFile* root);
int64 CalculateFolderSize(VirtualFile* root);

class MemDriveCBCache : public CBCache
{

    INT FireReadData(CBCacheReadDataEventParams* e)
    {
        e->BytesRead = 0;
        if (e->BytesToRead == 0)
            return e->ResultCode;

        VirtualFile* vfile = (VirtualFile*)(e->FileContext);
        if (vfile == NULL)
        {
            e->ResultCode = cbcConstants::RWRESULT_FILE_FAILURE; // should never happen as we open a file earlier
            return e->ResultCode;
        }

        if ((e->Flags & cbcConstants::RWEVENT_CANCELED) == 0)
        {
            DWORD Count;
            Count = (DWORD)(e->BytesToRead);

            int64 CurrPos = e->Position;

            int BytesRead = 0;

            vfile->Read(e->Buffer, e->Position, e->BytesToRead, &BytesRead);
            if (BytesRead > 0)
            {
                e->BytesRead = (INT)BytesRead;
                e->ResultCode = 0;
            }
            else
            {
                e->ResultCode = cbcConstants::RWRESULT_FILE_FAILURE;
                return e->ResultCode;
            }
        }
        else
            e->ResultCode = 0;

        return 0;
    }

    INT FireWriteData(CBCacheWriteDataEventParams* e)
    {
        e->BytesWritten = 0;
        if (e->BytesToWrite == 0)
            return e->ResultCode;

        VirtualFile* vfile = (VirtualFile*)(e->FileContext);
        if (vfile == NULL)
        {
            e->ResultCode = cbcConstants::RWRESULT_FILE_FAILURE; // should never happen as we open a file earlier
            return e->ResultCode;
        }

        if ((e->Flags & cbcConstants::RWEVENT_CANCELED) == 0)
        {

            int BytesWritten = 0;

            vfile->Write((void*)e->Buffer, e->Position, e->BytesToWrite, &BytesWritten);

            if (BytesWritten > 0)
            {
                e->BytesWritten = (INT)BytesWritten;
                e->ResultCode = 0;
            }
            else
            {
                e->ResultCode = cbcConstants::RWRESULT_FILE_FAILURE;
                return e->ResultCode;
            }
        }
        else
            e->ResultCode = 0;

        return 0;
    }
};


class MemDriveFUSE : public FUSE
{
private:
    CBCache* cache;


public: // Events

    int GetSectorSize()
    {
        fuse_char* SS = this->Config(TEXT("SectorSize"));
        if (SS != NULL && fuse_slen(SS) > 0)
            return fuse_stoi(SS);
        else
            return 0;
    }

    MemDriveFUSE() : FUSE()
    {
        cache = NULL;
    }

    void SetCache(CBCache* value)
    {
        cache = value;
    }

    int FireAccess(FUSEAccessEventParams* e) override
    {
        return 0;
    }

    int FireChmod(FUSEChmodEventParams* e) override { return 0; }
    int FireChown(FUSEChownEventParams* e) override { return 0; }
    int FireCopyFileRange(FUSECopyFileRangeEventParams* e) override { return 0; }

    int FireCreate(FUSECreateEventParams* e) override
    {
        VirtualFile* vfile = NULL, * vdir = NULL;
        int64 now;
#ifdef UNIX
        struct timeval tv;
        gettimeofday(&tv, NULL);
        now = UnixTimeToFileTime(tv.tv_sec, tv.tv_usec * 1000);
#endif // UNIX
#ifdef WIN32
        GetSystemTimeAsFileTime((LPFILETIME)&now);
#endif // WIND32

        if (FindVirtualFile(e->Path, vfile))
        {
            e->Result = -EEXIST;
            return e->Result;
        }

        if (!GetParentVirtualDirectory(e->Path, vdir))
        {
            e->Result = -ENOENT;
            return e->Result;
        }

        vfile = new VirtualFile(GetFileName(e->Path), e->Mode);

        vfile->set_Gid(GetGid());
        vfile->set_Uid(GetUid());

        vfile->set_CreationTime(now);
        vfile->set_LastAccessTime(now);
        vfile->set_LastWriteTime(now);

        vdir->AddFile(vfile);
        cache->FileOpenEx(e->Path, 0, false, 0, 0, cbcConstants::PREFETCH_NOTHING, vfile);

        return 0;
    }

    int FireDestroy(FUSEDestroyEventParams* e) override
    {
        return 0;
    }

    int FireError(FUSEErrorEventParams* e) override
    {
        return 0;
    }

    int FireFAllocate(FUSEFAllocateEventParams* e) override
    {
        VirtualFile* vfile = NULL;

        if (FindVirtualFile(e->Path, vfile))
        {

            int flags = e->Mode & (~FALLOC_FL_KEEP_SIZE);
            if (flags != 0) // if we detect unsupported flags in Mode, we deny the request
            {
                e->Result = -EOPNOTSUPP;
                return e->Result;
            }

            int64 fsize = vfile->get_Size();

            if (e->Offset + e->Length >= fsize)
            {
                int64 newSize = e->Offset + e->Length;
                vfile->set_AllocationSize(newSize);

                // fallocate may be used on non-Windows systems to expand file size
                // Windows component always sets the FALLOC_FL_KEEP_SIZE flag
                if ((e->Mode & FALLOC_FL_KEEP_SIZE) != FALLOC_FL_KEEP_SIZE)
                {
                    if (fsize < newSize)
                        vfile->set_Size(newSize);
                }
            }
        }
        else
            e->Result = -ENOENT;

        return e->Result;
    }

    int FireFlush(FUSEFlushEventParams* e) override
    {
        return 0;
    }

    int FireFSync(FUSEFSyncEventParams* e) override
    {
        return 0;
    }

    int FireGetAttr(FUSEGetAttrEventParams* e) override
    {
        e->Result = -ENOENT;

        VirtualFile* vfile = NULL;

        if (FindVirtualFile(e->Path, vfile))
        {
            e->Result = 0;
            *(e->pIno) = (int64)vfile;
            e->Mode = vfile->get_Mode();
            e->Uid = vfile->get_Uid();
            e->Gid = vfile->get_Gid();
            e->LinkCount = 1;
            if ((e->Mode & S_IFDIR) != 0)
                *(e->pSize) = 512;
            else
            {
                *(e->pSize) = vfile->get_Size();
                if (cache->FileExists(e->Path))
                    *(e->pSize) = cache->FileGetSize(e->Path);
            }
            *(e->pCTime) = vfile->get_CreationTime();
            *(e->pMTime) = vfile->get_LastWriteTime();
            *(e->pATime) = vfile->get_LastAccessTime();
        }

        return e->Result;
    }

    int FireInit(FUSEInitEventParams* e) override
    {
        return 0;
    }

    int FireMkDir(FUSEMkDirEventParams* e) override
    {
        VirtualFile* vfile = NULL, * vdir = NULL;
        int64 now;
#ifdef UNIX
        struct timeval tv;
        gettimeofday(&tv, NULL);
        now = UnixTimeToFileTime(tv.tv_sec, tv.tv_usec * 1000);
#endif // UNIX
#ifdef WIN32
        GetSystemTimeAsFileTime((LPFILETIME)&now);
#endif // WIND32

        if (FindVirtualFile(e->Path, vfile))
        {
            e->Result = -EEXIST;
            return e->Result;
        }

        if (!GetParentVirtualDirectory(e->Path, vdir))
        {
            e->Result = -ENOENT;
            return e->Result;
        }

        vfile = new VirtualFile(GetFileName(e->Path), e->Mode);

        vfile->set_Gid(GetGid());
        vfile->set_Uid(GetUid());
        vfile->set_CreationTime(now);
        vfile->set_LastAccessTime(now);
        vfile->set_LastWriteTime(now);

        vdir->AddFile(vfile);

        return 0;
    }

    int FireOpen(FUSEOpenEventParams* e) override
    {
        VirtualFile* vfile;
        if (FindVirtualFile(e->Path, vfile))
        {
            if (e->FileContext == NULL)
            {
                e->FileContext = vfile;
                cache->FileOpen(e->Path, vfile->get_Size(), cbcConstants::PREFETCH_NOTHING, e->FileContext);
            }
            return 0;
        }
        else
            e->Result = -ENOENT;
        return e->Result;
    }

    int FireRead(FUSEReadEventParams* e) override
    {
        int BytesRead;
        VirtualFile* vfile;

        if (FindVirtualFile(e->Path, vfile))
        {
            //vfile->Read((void*)e->Buffer, e->Offset, (int)e->Size, &BytesRead);
            //e->Result = BytesRead;

            if (cache->FileRead(e->Path, e->Offset, e->Buffer, e->Size, 0, e->Size))
                e->Result = e->Size;
            else
                e->Result = -EIO;
        }
        else
            e->Result = -ENOENT;

        return e->Result;
    }

    int FireReadDir(FUSEReadDirEventParams* e) override
    {
        VirtualFile* vdir = NULL, * vfile = NULL;

        if (FindVirtualDirectory(e->Path, vdir))
        {
            TCHAR FileNameBuf[32768];
            for (int i = 0; i < vdir->get_Context()->GetCount(); i++)
            {
                vdir->get_Context()->GetFile(i, vfile);

                long childSize = vfile->get_Size();
                if ((vfile->get_Mode() & S_IFDIR) == 0)
                {
                    _tcscpy(FileNameBuf, e->Path);
#ifdef WIN32
                    _tcscat(FileNameBuf, _T("\\"));
#else
                    _tcscat(FileNameBuf, _T("/"));
#endif#                  
                    _tcscat(FileNameBuf, vfile->get_Name());

                    if (cache->FileExists(FileNameBuf))
                        childSize = cache->FileGetSize(FileNameBuf);
                }

                FillDir(e->FillerContext, vfile->get_Name(), 0,
                    vfile->get_Mode(), vfile->get_Uid(), vfile->get_Gid(), 1,
                    childSize, vfile->get_LastAccessTime(),
                    vfile->get_LastWriteTime(), vfile->get_CreationTime());
            }
        }
        else
            e->Result = -ENOTDIR;

        return 0;
    }

    int FireRelease(FUSEReleaseEventParams* e) override
    {
        cache->FileCloseEx(e->Path, cbcConstants::FLUSH_IMMEDIATE, cbcConstants::PURGE_NONE);
        return 0;
    }

    int FireRename(FUSERenameEventParams* e) override
    {
        VirtualFile* voldfile = NULL, * vnewfile = NULL, * vnewparent = NULL;

        if (FindVirtualFile(e->OldPath, voldfile))
        {
            if (FindVirtualFile(e->NewPath, vnewfile))
            {
                if (e->Flags == 0)
                {
                    vnewfile->Remove();
                    delete vnewfile;
                }
                else
                    e->Result = -EEXIST;
            }
            if (e->Result == 0)
            {
                if (GetParentVirtualDirectory(e->NewPath, vnewparent))
                {
                    voldfile->Remove();
                    voldfile->Rename(GetFileName(e->NewPath));
                    vnewparent->AddFile(voldfile);

                    if (cache->FileExists(e->NewPath))
                    {
                        cache->FileDelete(e->NewPath);
                    }
                    cache->FileChangeId(e->OldPath, e->NewPath);
                }
                else
                    e->Result = -ENOENT;
            }
        }
        else
            e->Result = -ENOENT;

        return e->Result;
    }

    int FireRmDir(FUSERmDirEventParams* e) override
    {
        VirtualFile* vfile = NULL;

        if (FindVirtualFile(e->Path, vfile))
        {
            vfile->Remove();
            delete vfile;
        }
        else
            e->Result = -ENOENT;

        return e->Result;
    }

    int FireStatFS(FUSEStatFSEventParams* e) override
    {
        int SectorSize;
        int64 TotalMemory;
#ifdef WIN32
        MEMORYSTATUS status;
        GlobalMemoryStatus(&status);

        TotalMemory = status.dwTotalVirtual;
        SectorSize = GetSectorSize();
#endif

#ifdef UNIX
        TotalMemory = 1024 * 1024 * 1024;
        SectorSize = 512;
#endif

        * (e->pBlockSize) = GetSectorSize();
        *(e->pTotalBlocks) = TotalMemory / SectorSize;
        *(e->pFreeBlocks) = *(e->pFreeBlocksAvail) = (TotalMemory - CalculateFolderSize(g_DiskContext) + SectorSize / 2) / SectorSize;

        return 0;
    }

    int FireTruncate(FUSETruncateEventParams* e) override
    {
        VirtualFile* vfile = NULL;

        if (FindVirtualFile(e->Path, vfile))
        {
            vfile->set_Size(e->Size);
            cache->FileSetSize(e->Path, e->Size, FALSE);
        }
        else
            e->Result = -ENOENT;

        return e->Result;
    }

    int FireUnlink(FUSEUnlinkEventParams* e) override
    {
        VirtualFile* vfile = NULL;

        if (FindVirtualFile(e->Path, vfile))
        {
            vfile->Remove();
            delete vfile;
            cache->FileDelete(e->Path);
        }
        else
            e->Result = -ENOENT;

        return e->Result;
    }

    int FireUtimens(FUSEUtimensEventParams* e) override
    {
        VirtualFile* vfile = NULL;

        if (FindVirtualFile(e->Path, vfile))
        {
            if (e->ATime != 0)
                vfile->set_LastAccessTime(e->ATime);
            if (e->MTime != 0)
                vfile->set_LastWriteTime(e->MTime);
        }
        else
            e->Result = -ENOENT;

        return e->Result;
    }

    int FireWrite(FUSEWriteEventParams* e) override
    {
        int BytesWritten;
        VirtualFile* vfile;

        if (FindVirtualFile(e->Path, vfile))
        {
            /*vfile->set_AllocationSize(e->Offset + e->Size);
            vfile->Write((void*)e->Buffer, e->Offset, (int)e->Size, &BytesWritten);
            */

            if (cache->FileWrite(e->Path, e->Offset, (char*)(e->Buffer), e->Size, 0, e->Size))
            {
                e->Result = e->Size;
                return 0;
            }
            else
                e->Result = -EIO;
        }
        else
            e->Result = -ENOENT;

        return e->Result;
    }
};


const fuse_char* g_Guid = TEXT("{713CC6CE-B3E2-4fd9-838D-E28F558F6866}");
const fuse_char* icon_id = TEXT("0_MemDrive");

const char* strSuccess = "Success";
const char* strInvalidOption = "Invalid option \"%s\"\n";

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
    printf("This demo shows how to create a virtual drive that is stored in memory using the FUSE component.\n\n");
}

void usage(void)
{
    printf("Usage: fusememdrive [-<switch 1> ... -<switch N>] <mounting point>\n\n");
    printf("<Switches>\n");
#ifdef WIN32
    printf("  -drv {cab_file} - Install drivers from CAB file\n");
#endif
    printf("  -- Stop switches scanning\n\n");
    printf("Example: fusememdrive Y:\n\n");
}

// ----------------------------------------------------------------------------------

MemDriveFUSE cbfs_fuse;
MemDriveCBCache cache;

#ifdef WIN32
#define R_OK 0
#define access(filename, mode) (GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES ? 0 : 1)

void check_driver()
{
    int state;

    state = cbfs_fuse.GetDriverStatus(g_Guid);
    if (state == SERVICE_RUNNING)
    {
        LONG64 version;
        version = cbfs_fuse.GetDriverVersion(g_Guid);
        printf("CBFSFUSE driver is installed, version: %d.%d.%d.%d\n",
            (int)((version & 0x7FFF000000000000) >> 48),
            (int)((version & 0xFFFF00000000) >> 32),
            (int)((version & 0xFFFF0000) >> 16),
            (int)(version & 0xFFFF));
    }
    else
    {
        printf("CBFSFUSE driver is not installed\n");
        exit(0);
    }
}

fuse_char* a2w(char* source)
{
    fuse_char* result = NULL;
    if (source == NULL)
    {
        result = (fuse_char*)malloc(sizeof(WCHAR));
        result[0] = 0;
        return result;
    }
    else
    {
        int wstrLen = MultiByteToWideChar(CP_ACP, 0, source, -1, NULL, 0);
        if (wstrLen > 0)
        {
            result = (fuse_char*)malloc((wstrLen + 1) * sizeof(WCHAR));

            if (MultiByteToWideChar(CP_ACP, 0, source, -1, result, wstrLen) == 0)
                return NULL;
            else
                return result;
        }
        else
            return NULL;
    }
}
#else
#define a2w(str) (fuse_char*)(str)
#endif

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

int main(int argc, char* argv[])
{
#ifndef WIN32
    struct pollfd cinfd[1];
    int count;
    int opt_debug = 0;
#else
    int drv_reboot = 0;
#endif
    const fuse_char* mount_point = nullptr;
    int argi, arg_len, stop_opt = 0, mounted = 0;

    banner();
    if (argc < 2)
    {
        usage();
#ifdef WIN32
        check_driver();
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
#ifdef WIN32
                    else if (optcmp(argv[argi], (char*)"-drv"))
                    {
                        argi++;
                        if (argi < argc)
                        {
                            printf("Installing drivers from '%s'\n", argv[argi]);
                            cbt_string drv_reboot_wstr = ConvertRelativePathToAbsolute(a2w(argv[argi]));
                            if (drv_reboot_wstr.empty()) {
                                printf("Error: Invalid Driver Path\n");
                                exit(1);
                            }
                            drv_reboot = cbfs_fuse.Install(drv_reboot_wstr.c_str(), g_Guid, NULL,
                                cbcConstants::INSTALL_REMOVE_OLD_VERSIONS);

                            retVal = cbfs_fuse.GetLastErrorCode();
                            if (0 != retVal)
                            {
                                if (retVal == ERROR_PRIVILEGE_NOT_HELD)
                                    fprintf(stderr, "Drivers are not installed due to insufficient privileges. Please, run installation with administrator rights");
                                else
                                    fprintf(stderr, "Drivers are not installed, error %s", cbfs_fuse.GetLastError());
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
                retVal = cbfs_fuse.Initialize(g_Guid);
                if (0 != retVal)
                {
                    fprintf(stderr, "Error: %s", cbfs_fuse.GetLastError());
                    return retVal;
                }

                // Setup the cache
                TCHAR CacheDir[MAX_PATH + 10];

                if (!::GetCurrentDirectory(MAX_PATH + 9, &CacheDir[0]))
                {
                    fprintf(stderr, "Error %d when retrieving current directory", ::GetLastError());
                    return 1;
                }
                _tcscat(CacheDir, _T("\\cache"));

                DWORD attr = GetFileAttributes(CacheDir);

                if (attr == INVALID_FILE_ATTRIBUTES)
                {
                    if (!CreateDirectory(CacheDir, NULL))
                    {
                        fprintf(stderr, "Error %d when creating directory '%s'", ::GetLastError(), CacheDir);
                        return 1;
                    }
                }
                else
                    if ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
                    {
                        fprintf(stderr, "'%s' already exists and is not a directory", CacheDir);
                        return 1;
                    }


                cache.SetCacheDirectory(&CacheDir[0]);
                cache.SetCacheFile(_T("cache.svlt"));
                cache.SetReadingCapabilities(cbcConstants::RWCAP_POS_RANDOM | cbcConstants::RWCAP_SIZE_ANY);
                cache.SetResizingCapabilities(cbcConstants::RSZCAP_GROW_TO_ANY | cbcConstants::RSZCAP_SHRINK_TO_ANY | cbcConstants::RSZCAP_TRUNCATE_AT_ZERO);
                cache.SetWritingCapabilities(cbcConstants::RWCAP_POS_RANDOM | cbcConstants::RWCAP_SIZE_ANY | cbcConstants::RWCAP_WRITE_KEEPS_FILESIZE);
                int cacheOpenResult = cache.OpenCache(_T(""), FALSE);
                if (cacheOpenResult != 0)
                {
                    fprintf(stderr, "Error %d was reported during the attempt to open the cache", cacheOpenResult);
                    return cacheOpenResult;
                }

                cbfs_fuse.SetCache(&cache); // this is the sample's method, not an API one


                //cbfs_fuse.Config("LinuxFUSEParams=-d");

                if (NULL == g_DiskContext)
                    g_DiskContext = new VirtualFile(TEXT("/"), S_IFDIR);

                cbt_string mount_point_wstr = ConvertRelativePathToAbsolute(a2w(argv[argi]), true);
                if (mount_point_wstr.empty()) {
                    printf("Error: Invalid Mounting Point Path\n");
                    exit(1);
                }
                mount_point = wcsdup(mount_point_wstr.c_str());

                retVal = cbfs_fuse.Mount(mount_point);
                if (0 != retVal)
                {
                    fprintf(stderr, "Error: %s", cbfs_fuse.GetLastError());
                    return retVal;
                }
                else
                    mounted = 1;
                break;
            }
        }
    }

    if (!mounted)
    {
        if (mount_point == nullptr)
            fprintf(stderr, "Virtual Disk: Invalid parameters, mounting point not specified\n");
        else
            fprintf(stderr, "Virtual Disk: Failed to mount the disk\n");
    }
    else
    {
        printf("Press Enter to unmount the disk\n");
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
        retVal = cbfs_fuse.Unmount();
        if (0 != retVal)
            fprintf(stderr, "Error: %s", cbfs_fuse.GetLastError());
        else
        {
            printf("Unmount done\n");
            cache.CloseCache(cbcConstants::FLUSH_IMMEDIATE, cbcConstants::PURGE_IMMEDIATE);
        }
#ifndef WIN32
        break;
            }
        }
#endif
    }

    return 0;
}

//-----------------------------------------------------------------------------------------------------------

bool FindVirtualFile(const fuse_char* FileName, VirtualFile*& vfile)
{
    assert(FileName);

    bool result = true;

    VirtualFile* root = g_DiskContext;

    fuse_char* buffer = (fuse_char*)malloc((fuse_slen(FileName) + 1) * sizeof(fuse_char));

    assert(buffer);

    fuse_scpy(buffer, FileName);

    fuse_char* token = fuse_stok(buffer, TEXT("/"));

    vfile = root;

    while (token != NULL)
    {
        if (fuse_scmp(vfile->get_Name(), token) == 0)
        {
            root = vfile;
            break;
        }
        else if (result = root->get_Context()->GetFile(token, vfile))
            root = vfile;
        token = fuse_stok(NULL, TEXT("/"));
    }
    free(buffer);

    return result;
}

//-----------------------------------------------------------------------------------------------------------

bool GetParentVirtualDirectory(const fuse_char* FileName, VirtualFile*& vfile)
{
    assert(FileName);

    const fuse_char* result = GetFileName(FileName);

    fuse_char* buffer = (fuse_char*)malloc((result - FileName + 1) * sizeof(fuse_char));

    memcpy(buffer, FileName, (result - FileName) * sizeof(fuse_char));

    buffer[result - FileName] = 0;

    bool find = FindVirtualFile(buffer, vfile);

    free(buffer);

    return find;
}

//-----------------------------------------------------------------------------------------------------------

bool FindVirtualDirectory(const fuse_char* FileName, VirtualFile*& vfile)
{
    assert(FileName);

    bool find = false;

    if (FindVirtualFile(FileName, vfile) && ((vfile->get_Mode() & S_IFDIR) != 0))
        return true;
    else
        find = GetParentVirtualDirectory(FileName, vfile);

    assert(find == true);
    return find;
}


//-----------------------------------------------------------------------------------------------------------
const fuse_char* GetFileName(const fuse_char* fullpath)
{
    assert(fullpath);

    const fuse_char* result = &fullpath[fuse_slen(fullpath)];

    while (--result != fullpath)
    {
        if (*result == '/')
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
        if ((vfile->get_Mode() & S_IFDIR) != 0)
            RemoveAllFiles(vfile);
        vfile->Remove();

        delete vfile;
    }
}

//-----------------------------------------------------------------------------------------------------------

int64 CalculateFolderSize(VirtualFile* root)
{
    if (root == NULL)
        return 0;
    VirtualFile* vfile;
    int64 DiskSize = 0;
    int Index = 0;

    while (root->get_Context()->GetFile(Index++, vfile))
    {
        if ((vfile->get_Mode() & S_IFDIR) != 0)
            DiskSize += CalculateFolderSize(vfile);
        DiskSize += (vfile->get_AllocationSize() + cbfs_fuse.GetSectorSize() - 1) & ~(cbfs_fuse.GetSectorSize() - 1);
    }
    return DiskSize;
}

 
 
 
 

