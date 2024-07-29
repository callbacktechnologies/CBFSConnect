#if !defined _VIRTUAL_FILE_H
#define _VIRTUAL_FILE_H

//disable compiler warnings about functions that was marked with deprecated
#pragma warning(disable : 4996)

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <list>
#ifndef UNIX
#include <errno.h>
#endif

#ifdef _UNICODE
#include "../../include/unicode/fuse.h"
typedef __int64 int64;
typedef wchar_t fuse_char;
#define fuse_slen(str) wcslen(str)
#define fuse_scmp(str1, str2) wcscmp(str1, str2)
#define fuse_scpy(dst, src) wcscpy(dst, src)
#define fuse_stok(s, delim) _wcstok(s, delim)
#define fuse_stoi(str) _wtoi(str)
#else
#include "../../include/fuse.h"
typedef char fuse_char;
#define TEXT(quote) quote
#define fuse_slen(str) strlen(str)
#define fuse_scmp(str1, str2) strcmp(str1, str2)
#define fuse_scpy(dst, src) strcpy(dst, src)
#define fuse_stok(s, delim) strtok(s, delim)
#define fuse_stoi(str) atoi(str)
#endif

#include "cbfsconnectcommon.h"

class VirtualFile;//forward declaration

//class DirectoryEnumerationContext

class DirectoryEnumerationContext
{
public:
    DirectoryEnumerationContext();
    
    //DirectoryEnumerationContext(VirtualFile* vfile);

    int GetCount();
    
    bool GetNextFile(VirtualFile*& vfile);
    
    bool GetFile(const fuse_char *FileName, VirtualFile*& vfile);

    bool GetFile(int Index, VirtualFile*& vfile);

    void AddFile(VirtualFile* vfile);
    
    void Remove(VirtualFile* vfile);
    
    void ResetEnumeration(void);

    bool IsEmpty(void);
private:
    std::list <VirtualFile*> mFileList;
    std::list<VirtualFile*>::const_iterator mIndex;    
};

// class VirtualFile
// represent directories and files information

class VirtualFile
{
public:
    
    VirtualFile(const fuse_char * Name);
    
    VirtualFile(const fuse_char * Name, int Mode);

    VirtualFile(const fuse_char * Name, int Mode, int InitialSize);

    ~VirtualFile();
        
    void AddFile(VirtualFile* vfile);
    
    void Rename(const fuse_char * NewName);

    void Remove(void);

    void Write(void *WriteBuf, int64 Position, int BytesToWrite, int *BytesWritten);

    void Read(void *ReadBuf, int64 Position, int BytesToRead, int *BytesRead);

//property
    void set_AllocationSize(int64 Value);
    int64 get_AllocationSize(void);

    void set_Size(int64 Value);
    int64 get_Size(void);
    
    fuse_char *get_Name(void);

    int64 get_CreationTime(void);
    void set_CreationTime(int64 Value);

    int64 get_LastAccessTime(void);
    void set_LastAccessTime(int64 Value);

    int64 get_LastWriteTime(void);
    void set_LastWriteTime(int64 Value);

    int get_Mode(void);
    void set_Mode(int Value);

    int get_Uid(void);
    void set_Uid(int Value);

    int get_Gid(void);
    void set_Gid(int Value);

    DirectoryEnumerationContext* get_Context(void);
    
    VirtualFile* get_Parent(void);
    void set_Parent(VirtualFile* Value);

private:
    VirtualFile();    
    void Initializer(const fuse_char * Name);
    
    DirectoryEnumerationContext mEnumCtx;
    VirtualFile* mParent;

    fuse_char *mName;
    void *mStream;

    int64 mSize;
    int64 mAllocationSize;

    int mMode;
    int mUid;
    int mGid;

    int64 mCreationTime;
    int64 mLastAccessTime;
    int64 mLastWriteTime;

};

#endif //#if !defined _VIRTUAL_FILE_H