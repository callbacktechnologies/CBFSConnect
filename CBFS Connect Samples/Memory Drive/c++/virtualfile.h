#if !defined _VIRTUAL_FILE_H
#define _VIRTUAL_FILE_H

//disable compiler warnings about functions that was marked with deprecated
#pragma warning(disable : 4996)


#include <list>

class VirtualFile;//forward declaration

//class DirectoryEnumerationContext

class DirectoryEnumerationContext
{
public:
    DirectoryEnumerationContext();
    
    //DirectoryEnumerationContext(VirtualFile* vfile);
    
    BOOL GetNextFile(VirtualFile*& vfile);
    
    BOOL GetFile(LPCWSTR FileName, VirtualFile*& vfile);

    BOOL GetFile(INT Index, VirtualFile*& vfile);

    VOID AddFile(VirtualFile* vfile);
    
    VOID Remove(VirtualFile* vfile);
    
    VOID ResetEnumeration(void);

    BOOL IsEmpty(void);
private:
    std::list <VirtualFile*> mFileList;
    std::list<VirtualFile*>::const_iterator mIndex;    
};

// class VirtualFile
// represent directories and files information

class VirtualFile
{
public:
    
    VirtualFile(LPCWSTR Name);
    
    VirtualFile(LPCWSTR Name, INT InitialSize);
    
    ~VirtualFile();
        
    VOID AddFile(VirtualFile* vfile);
    
    VOID Rename(LPCWSTR NewName);

    VOID Remove(VOID);

    VOID Write(PVOID WriteBuf, LONG Position, INT BytesToWrite, PDWORD BytesWritten);

    VOID Read(PVOID ReadBuf, LONG Position, INT BytesToRead, PDWORD BytesRead);

    

//property
    VOID set_AllocationSize(__int64 Value);
    __int64 get_AllocationSize(VOID);

    VOID set_Size(__int64 Value);
    __int64 get_Size(VOID);
    
    LPWSTR get_Name(VOID);

    FILETIME get_CreationTime(VOID);
    VOID set_CreationTime(FILETIME Value);

    FILETIME get_LastAccessTime(VOID);
    VOID set_LastAccessTime(FILETIME Value);

    FILETIME get_LastWriteTime(VOID);    
    VOID set_LastWriteTime(FILETIME Value);

    INT get_FileAttributes(VOID);
    VOID set_FileAttributes(INT Value);

    __int64 get_ReparseTag(VOID);
    VOID set_ReparseTag(__int64 Value);

    PVOID get_ReparseBuffer(VOID);
    VOID set_ReparseBuffer(PVOID Value);

    WORD get_ReparseBufferLength(VOID);
    VOID set_ReparseBufferLength(SHORT Value);

    //VOID set_Context(DirectoryEnumerationContext* Value);
    DirectoryEnumerationContext* get_Context(VOID);
    
    VirtualFile* get_Parent(VOID);
    VOID set_Parent(VirtualFile* Value);

    VOID VirtualFile::DeleteReparsePoint();
    VOID VirtualFile::CreateReparsePoint(LPWSTR ReparsePath);

private:
    VirtualFile();    
    VOID Initializer(LPCWSTR Name);
    
    DirectoryEnumerationContext mEnumCtx;
    VirtualFile* mParent;

    LPWSTR mName;
    PVOID mStream;
    __int64 mSize;
    __int64 mAllocationSize;
    DWORD mAttributes;  
    FILETIME mCreationTime;
    FILETIME mLastAccessTime;
    FILETIME mLastWriteTime; 
    __int64 mReparseTag;
    PVOID mReparseBuffer;
    WORD mReparseBufferLength;
};

#endif //#if !defined _VIRTUAL_FILE_H