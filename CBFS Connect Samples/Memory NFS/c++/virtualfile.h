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
#include "../../include/unicode/nfs.h"
typedef __int64 int64;
typedef wchar_t nfs_char;
#define nfs_slen(str) wcslen(str)
#define nfs_scmp(str1, str2) wcscmp(str1, str2)
#define nfs_scpy(dst, src) wcscpy(dst, src)
#define nfs_stok(s, delim) _wcstok(s, delim)
#define nfs_stoi(str) _wtoi(str)
#else
#include "../../include/nfs.h"
typedef char nfs_char;
#define TEXT(quote) quote
#define nfs_slen(str) strlen(str)
#define nfs_scmp(str1, str2) strcmp(str1, str2)
#define nfs_scpy(dst, src) strcpy(dst, src)
#define nfs_stok(s, delim) strtok(s, delim)
#define nfs_stoi(str) atoi(str)
#endif

class VirtualFile;//forward declaration

//class DirectoryEnumerationContext

class DirectoryEnumerationContext
{
public:
    DirectoryEnumerationContext();
    
    //DirectoryEnumerationContext(VirtualFile* vfile);

    int GetCount();
    
    bool GetNextFile(VirtualFile*& vfile);
    
    bool GetFile(const nfs_char *FileName, VirtualFile*& vfile);

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
    
    VirtualFile(const nfs_char * Name);
    
    VirtualFile(const nfs_char * Name, int Mode);

    VirtualFile(const nfs_char * Name, int Mode, int InitialSize);

    ~VirtualFile();
        
    void AddFile(VirtualFile* vfile);
    
    void Rename(const nfs_char * NewName);

    void Remove(void);

    void Write(void *WriteBuf, int64 Position, int BytesToWrite, int *BytesWritten);

    void Read(void *ReadBuf, int64 Position, int BytesToRead, int *BytesRead);

//property
    void set_AllocationSize(int64 Value);
    int64 get_AllocationSize(void);

    void set_Size(int64 Value);
    int64 get_Size(void);
    
    nfs_char *get_Name(void);

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
    void Initializer(const nfs_char * Name);
    
    DirectoryEnumerationContext mEnumCtx;
    VirtualFile* mParent;

    nfs_char *mName;
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