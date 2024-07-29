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

#include <assert.h> 
#include <iostream>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>

#ifdef WIN32
#include <conio.h>
#include <tchar.h>
#include <Shlwapi.h>
#endif

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
#include "../../include/unicode/nfs.h"
#else
#include "../../include/nfs.h"
#endif

#ifndef FALLOC_FL_KEEP_SIZE
#define FALLOC_FL_KEEP_SIZE 1 
#endif

using namespace std;
using namespace cbcConstants;

#ifdef _UNICODE
#define sout std::wcout
#define scin std::wcin
typedef std::wstring cbt_string;
#else
#define sout std::cout
#define scin std::cin
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
bool FindVirtualFile(const nfs_char* FileName, VirtualFile*& vfile);
bool FindVirtualDirectory(const nfs_char* FileName, VirtualFile*& vfile);
bool GetParentVirtualDirectory(const nfs_char* FileName, VirtualFile*& vfile);
const nfs_char* GetFileName(const nfs_char* fullpath);
void RemoveAllFiles(VirtualFile* root);

#ifndef UNIX
#ifndef S_IFMT 
#define S_IFMT 61440
#endif 
#define S_IFSOCK 49152
#define S_IFLNK 40960
#ifndef S_IFREG 
#define S_IFREG 32768
#endif
#define S_IFBLK 24576
#ifndef S_IFDIR 
#define S_IFDIR 16384
#endif
#ifndef S_IFCHR 
#define S_IFCHR 8192
#endif
#define S_IFIFO 4096
#define S_ISUID 2048
#define S_ISGID 1024
#define S_ISVTX 512
#define S_IRWXU 448
#define S_IRUSR 256
#define S_IWUSR 128
#define S_IXUSR 64
#define S_IRWXG 56
#define S_IRGRP 32
#define S_IWGRP 16
#define S_IXGRP 8
#define S_IRWXO 7
#define S_IROTH 4
#define S_IWOTH 2
#define S_IXOTH 1
#endif

#define baseCookie 0x12345678

// Type -> Directory, Permissions -> 755
#define DIR_MODE S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH

// Type -> File, Permissions 644
#define FILE_MODE S_IFREG | S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH

class MemDriveNFS : public NFS
{
public: // Events

    MemDriveNFS() : NFS()
    {
        //this->Config(_T("LogLevel=4"));
    }

    int FireAccess(NFSAccessEventParams* e) override { return 0; }
    int FireChmod(NFSChmodEventParams* e) override { return 0; }
    int FireChown(NFSChownEventParams* e) override { return 0; }

    int FireCreateLink(NFSCreateLinkEventParams* e) override
    {
        sout << _T("FireCreateLink: ") << e->Path << endl;
        e->Result = NFS4ERR_NOTSUPP;
        return 0;
    }

    int FireReadLink(NFSReadLinkEventParams* e) override
    {
        sout << _T("FireReadLink: ") << e->Path << endl;
        e->Result = NFS4ERR_NOTSUPP;
        return 0;
    }

    int FireConnected(NFSConnectedEventParams* e) override
    {
        sout << _T("Client connected: ") << e->StatusCode << _T(": ") << e->Description << endl;
        return 0;
    }

    int FireDisconnected(NFSDisconnectedEventParams* e) override
    {
        sout << _T("Client disconnected: ") << e->StatusCode << _T(": ") << e->Description << endl;
        return 0;
    }

    int FireError(NFSErrorEventParams* e) override
    {
        sout << _T("Error: ") << e->ErrorCode << endl;
        return 0;
    }

    int FireLog(NFSLogEventParams* e) override
    {
        sout << e->ConnectionId << _T(": ") << e->Message << endl;
        return 0;
    }

    int FireConnectionRequest(NFSConnectionRequestEventParams* e) override
    {
        sout << _T("Connection Request: ") << e->Address << _T(": ") << endl;
        e->Accept = TRUE;
        return 0;
    }

    int FireGetAttr(NFSGetAttrEventParams* e) override
    {
        sout << _T("FireGetAttr: ") << e->Path << endl;

        e->Result = NFS4ERR_NOENT;

        VirtualFile* vfile = NULL;

        if (FindVirtualFile(e->Path, vfile))
        {
            e->Result = 0;
            e->LinkCount = 1;
            e->Group = _T("0");
            e->User = _T("0");
            *(e->pSize) = vfile->get_Size();
            e->Mode = vfile->get_Mode();
            *(e->pCTime) = vfile->get_CreationTime();
            *(e->pMTime) = vfile->get_LastWriteTime();
            *(e->pATime) = vfile->get_LastAccessTime();
        }

        return 0;
    }

    int FireLookup(NFSLookupEventParams* e) override
    {
        sout << _T("FireLookup: ") << e->Path << endl;

        VirtualFile* vfile;

        if (FindVirtualDirectory(e->Path, vfile)) {
            return 0;
        }

        if (FindVirtualFile(e->Path, vfile)) {
            return 0;
        }

        e->Result = NFS4ERR_NOENT;
        return 0;
    }

    int FireMkDir(NFSMkDirEventParams* e) override
    {
        sout << _T("FireMkDir: ") << e->Path << endl;

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
            e->Result = NFS4ERR_EXIST;
            return 0;
        }

        if (!GetParentVirtualDirectory(e->Path, vdir))
        {
            e->Result = NFS4ERR_NOENT;
            return 0;
        }

        vfile = new VirtualFile(GetFileName(e->Path), DIR_MODE);

        vfile->set_CreationTime(now);
        vfile->set_LastAccessTime(now);
        vfile->set_LastWriteTime(now);

        vdir->AddFile(vfile);

        return 0;
    }

    int FireOpen(NFSOpenEventParams* e) override
    {
        sout << _T("FireOpen: ") << e->Path << _T(", open type: ") << e->OpenType << endl;

        int64 now;
#ifdef UNIX
        struct timeval tv;
        gettimeofday(&tv, NULL);
        now = UnixTimeToFileTime(tv.tv_sec, tv.tv_usec * 1000);
#endif // UNIX
#ifdef WIN32
        GetSystemTimeAsFileTime((LPFILETIME)&now);
#endif // WIND32

        if (e->OpenType == 1)
        {
            VirtualFile* vfile = NULL, * vdir = NULL;
            if (FindVirtualFile(e->Path, vfile))
            {
                e->Result = NFS4ERR_EXIST;
                return 0;
            }

            if (!GetParentVirtualDirectory(e->Path, vdir))
            {
                e->Result = NFS4ERR_NOENT;
                return 0;
            }

            vfile = new VirtualFile(GetFileName(e->Path), FILE_MODE);

            vfile->set_CreationTime(now);
            vfile->set_LastAccessTime(now);
            vfile->set_LastWriteTime(now);

            vdir->AddFile(vfile);
        }
        else
        {
            VirtualFile* vfile;

            if (!FindVirtualFile(e->Path, vfile))
                e->Result = NFS4ERR_NOENT;
            else
                vfile->set_LastAccessTime(now);
        }

        return 0;
    }

    int FireRead(NFSReadEventParams* e) override
    {
        sout << _T("FireRead: ") << e->Path << endl;

        if (e->Count == 0) return 0;

        int BytesRead;
        VirtualFile* vfile;

        if (FindVirtualFile(e->Path, vfile))
        {
            if (e->Offset >= vfile->get_Size())
            {
                e->Count = 0;
                e->Eof = true;
                return 0;
            }
            vfile->Read((void*)e->Buffer, e->Offset, (int)e->Count, &BytesRead);
            if (e->Offset + BytesRead == vfile->get_Size()) e->Eof = true;
            e->Count = BytesRead;
        }
        else
            e->Result = NFS4ERR_NOENT;

        return 0;
    }

    int FireReadDir(NFSReadDirEventParams* e) override
    {
        sout << _T("FireReadDir: ") << e->Path << endl;

        VirtualFile* vdir = NULL, * vfile = NULL;

        int readOffset = 0;
        long long cookie = baseCookie;  // just an offset to avoid a chance to have cookie set to 0, 1, 2

        // If Cookie != 0, continue listing entries from a specified cookie. Otherwise, start listing entries from the start.
        if (e->Cookie != 0) {
            readOffset = (int)e->Cookie - baseCookie + 1;
            cookie = e->Cookie + 1;
        }

        if (FindVirtualDirectory(e->Path, vdir))
        {
            int ret_code = 0;
            for (int i = readOffset; i < vdir->get_Context()->GetCount(); i++)
            {
                vdir->get_Context()->GetFile(i, vfile);
                ret_code = FillDir(e->ConnectionId, vfile->get_Name(), 0, cookie,
                    vfile->get_Mode(), _T("0"), _T("0"), 1,
                    vfile->get_Size(), vfile->get_LastAccessTime(),
                    vfile->get_LastWriteTime(), vfile->get_CreationTime());

                // Return now assuming FillDir returned non-zero value, indicating maximum entries have been provided.
                if (ret_code) 
                    return 0;
                else 
                    cookie++;
            }
        }
        else
            e->Result = NFS4ERR_NOENT;

        return 0;
    }

    int FireRename(NFSRenameEventParams* e) override
    {
        sout << _T("FireRename: ") << e->OldPath << _T(" -> ") << e->NewPath << endl;

        if (nfs_scmp(e->OldPath, e->NewPath) == 0) return 0;

        VirtualFile* voldfile = NULL, * vnewfile = NULL, * vnewparent = NULL;

        if (FindVirtualDirectory(e->OldPath, voldfile))
        {
            // Check for compatibility
            if (FindVirtualFile(e->NewPath, vnewfile) && (vnewfile->get_Mode() & S_IFREG) != 0)
            {
                e->Result = NFS4ERR_EXIST;
                return 0;
            }

            // Remove directory if it exists and is empty
            int fileCount = 0;
            if (FindVirtualDirectory(e->NewPath, vnewfile))
            {
                fileCount = vnewfile->get_Context()->GetCount();
                if (fileCount != 0)
                {
                    e->Result = NFS4ERR_EXIST;
                    return 0;
                }
                vnewfile->Remove();
                delete vnewfile;
            }

            // Move directory
            if (GetParentVirtualDirectory(e->NewPath, vnewparent))
            {
                voldfile->Remove();
                voldfile->Rename(GetFileName(e->NewPath));
                vnewparent->AddFile(voldfile);
            }
        }
        else if (FindVirtualFile(e->OldPath, voldfile))
        {
            // Check for compatibility
            if (FindVirtualDirectory(e->NewPath, vnewfile))
            {
                e->Result = NFS4ERR_EXIST;
                return 0;
            }

            // Remove existing file if it exists
            if (FindVirtualFile(e->NewPath, vnewfile))
            {
                vnewfile->Remove();
                delete vnewfile;
            }

            // Move file
            if (GetParentVirtualDirectory(e->NewPath, vnewparent))
            {
                voldfile->Remove();
                voldfile->Rename(GetFileName(e->NewPath));
                vnewparent->AddFile(voldfile);
            }
        }
        else
            e->Result = NFS4ERR_NOENT;

        return 0;
    }

    int FireRmDir(NFSRmDirEventParams* e) override
    {
        sout << _T("FireRmDir: ") << e->Path << endl;

        VirtualFile* vfile = NULL;

        if (FindVirtualFile(e->Path, vfile))
        {
            vfile->Remove();
            delete vfile;
        }
        else
            e->Result = NFS4ERR_NOENT;

        return 0;
    }

    int FireTruncate(NFSTruncateEventParams* e) override
    {
        sout << _T("FireTruncate: ") << e->Path << endl;

        VirtualFile* vfile = NULL;

        if (FindVirtualFile(e->Path, vfile))
            vfile->set_Size(e->Size);
        else
            e->Result = NFS4ERR_NOENT;

        return 0;
    }

    int FireUnlink(NFSUnlinkEventParams* e) override
    {
        sout << _T("FireUnlink: ") << e->Path << endl;

        VirtualFile* vfile = NULL;

        if (FindVirtualFile(e->Path, vfile))
        {
            vfile->Remove();
            delete vfile;
        }
        else
            e->Result = NFS4ERR_NOENT;

        return 0;
    }

    int FireUTime(NFSUTimeEventParams* e) override
    {
        sout << _T("FireUTime: ") << e->Path << endl;

        VirtualFile* vfile = NULL;

        if (FindVirtualFile(e->Path, vfile))
        {
            if (e->ATime != 0)
                vfile->set_LastAccessTime(e->ATime);
            if (e->MTime != 0)
                vfile->set_LastWriteTime(e->MTime);
        }
        else
            e->Result = NFS4ERR_NOENT;

        return 0;
    }

    int FireWrite(NFSWriteEventParams* e) override
    {
        sout << _T("FireWrite: ") << e->Path << endl;

        if (e->Count == 0) return 0;

        int BytesWritten;
        VirtualFile* vfile;

        if (FindVirtualFile(e->Path, vfile))
        {
            vfile->set_AllocationSize(e->Offset + e->Count);
            vfile->Write((void*)e->Buffer, e->Offset, (int)e->Count, &BytesWritten);

            e->Count = BytesWritten;
            e->Stable = FILE_SYNC4;
        }
        else
            e->Result = NFS4ERR_NOENT;

        return 0;
    }
};

//-----------------------------------------------------------------------------------------------------------

void banner(void)
{
    printf("CBFS Connect Copyright (c) Callback Technologies, Inc.\n\n");
    printf("This demo shows how to create a virtual drive that is stored in memory using the NFS class.\n\n");
}

void usage(void)
{
    printf("Usage: nfs [local port or - for default] <mounting point>\n\n");
    printf("Example 1 (any OS): nfs 2049\n");
    printf("Example 2 (Linux/macOS): sudo nfs - /mnt/mynfs\n\n");

    printf("'mount' command should be installed on Linux/macOS to use mounting points!\n");
    printf("Automatic mounting to mounting points is supported only on Linux and macOS.\n");
    printf("Also, mounting outside of your home directory may require admin rights.\n\n");

}

// ----------------------------------------------------------------------------------

MemDriveNFS cbfs_nfs;

// ----------------------------------------------------------------------------------

#ifdef WIN32
#define R_OK 0
#define access(filename, mode) (GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES ? 0 : 1)

nfs_char* a2w(char* source)
{
    nfs_char* result = NULL;
    if (source == NULL)
    {
        result = (nfs_char*)malloc(sizeof(WCHAR));
        result[0] = 0;
        return result;
    }
    else
    {
        int wstrLen = MultiByteToWideChar(CP_ACP, 0, source, -1, NULL, 0);
        if (wstrLen > 0)
        {
            result = (nfs_char*)malloc((wstrLen + 1) * sizeof(WCHAR));

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
#define a2w(str) (nfs_char*)(str)
#endif

void stopServer()
{
    sout << _T("Stopping server...") << endl;

    cbfs_nfs.StopListening();
    sout << _T("Server stopped") << endl;

    if (g_DiskContext)
        delete g_DiskContext;
}

int main(int argc, char* argv[]) {
    // default NFS port
    int port = 2049;
    cbt_string sPort;

#ifndef WIN32
    struct pollfd cinfd[1];
#endif

    banner();
    if (argc < 2) {
        usage();
        return 0;
    }

    sPort = a2w(argv[1]);
    if (sPort != _T("-"))
        port = atoi(argv[1]);

    if (argc == 3)
    {
#ifdef WIN32
        sout << "Mounting points are not supported on Windows, only on Linux and macOS" << endl;
        return 0;
#else
        cbt_string mountPointConfig = cbt_string("MountingPoint=") + a2w(argv[2]);
        cbfs_nfs.Config(mountPointConfig.c_str());
#endif
    }

    if (NULL == g_DiskContext)
    {
        int64 now;
#ifdef UNIX
        struct timeval tv;
        gettimeofday(&tv, NULL);
        now = UnixTimeToFileTime(tv.tv_sec, tv.tv_usec * 1000);
#endif // UNIX
#ifdef WIN32
        GetSystemTimeAsFileTime((LPFILETIME)&now);
#endif // WIND32
        
        g_DiskContext = new VirtualFile(TEXT("/"), DIR_MODE);

        g_DiskContext->set_CreationTime(now);
        g_DiskContext->set_LastAccessTime(now);
        g_DiskContext->set_LastWriteTime(now);

        g_DiskContext->set_Size(4096);


        VirtualFile* vfile = new VirtualFile(TEXT("test"), DIR_MODE);

        vfile->set_CreationTime(now);
        vfile->set_LastAccessTime(now);
        vfile->set_LastWriteTime(now);

        vfile->set_Size(4096);

        g_DiskContext->AddFile(vfile);
    }

    cbfs_nfs.SetLocalPort(port);
    int ret_code = cbfs_nfs.StartListening();

    if (ret_code) {
        sout << _T("Error starting NFS server: ") << cbfs_nfs.GetLastErrorCode() << _T(": ") << cbfs_nfs.GetLastError() << endl;
        return 0;
    }

    sout << _T("NFS server started on port ") << port << endl;
    sout << _T("Press <Enter> to stop the server") << endl;

#ifndef WIN32
    cinfd[0].fd = STDIN_FILENO;
    cinfd[0].events = POLLIN;
    while (1)
    {
        cbfs_nfs.DoEvents();
        if (poll(cinfd, 1, 100))
        {
            break;
        }
    }
#else
    while (true)
    {
        cbfs_nfs.DoEvents();
        if (_kbhit())
        {
            break;
        }
    }
#endif

    stopServer();
    return 0;
}

//-----------------------------------------------------------------------------------------------------------

bool FindVirtualFile(const nfs_char* FileName, VirtualFile*& vfile)
{
    assert(FileName);

    bool result = true;

    VirtualFile* root = g_DiskContext;

    nfs_char* buffer = (nfs_char*)malloc((nfs_slen(FileName) + 1) * sizeof(nfs_char));

    assert(buffer);

    nfs_scpy(buffer, FileName);

    nfs_char* token = nfs_stok(buffer, TEXT("/"));

    vfile = root;

    while (token != NULL)
    {
        if (nfs_scmp(vfile->get_Name(), token) == 0)
        {
            root = vfile;
            break;
        }
        else if (result = root->get_Context()->GetFile(token, vfile))
            root = vfile;
        token = nfs_stok(NULL, TEXT("/"));
    }
    free(buffer);

    return result;
}

//-----------------------------------------------------------------------------------------------------------

bool GetParentVirtualDirectory(const nfs_char* FileName, VirtualFile*& vfile)
{
    assert(FileName);

    const nfs_char* result = GetFileName(FileName);

    nfs_char* buffer = (nfs_char*)malloc((result - FileName + 1) * sizeof(nfs_char));

    memcpy(buffer, FileName, (result - FileName) * sizeof(nfs_char));

    buffer[result - FileName] = 0;

    bool find = FindVirtualFile(buffer, vfile);

    free(buffer);

    return find;
}

//-----------------------------------------------------------------------------------------------------------

bool FindVirtualDirectory(const nfs_char* FileName, VirtualFile*& vfile)
{
    assert(FileName);

    bool find = false;

    if (FindVirtualFile(FileName, vfile) && ((vfile->get_Mode() & S_IFDIR) != 0))
        return true;

    return find;
}


//-----------------------------------------------------------------------------------------------------------
const nfs_char* GetFileName(const nfs_char* fullpath)
{
    assert(fullpath);

    const nfs_char* result = &fullpath[nfs_slen(fullpath)];

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

 
 
 
 

