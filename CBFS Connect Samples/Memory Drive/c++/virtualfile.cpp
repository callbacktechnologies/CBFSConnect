#include <assert.h> 

#ifdef _UNICODE
#include "../../include/unicode/cbfs.h"
#else
#include "../../include/cbfs.h"
#endif

#include "virtualfile.h"

//class VirtualFile
VirtualFile::VirtualFile()
{

}

VirtualFile::VirtualFile(LPCWSTR Name)
    :mStream(NULL)
    ,mSize(0)
    ,mAllocationSize(0)
    ,mAttributes(0)
    ,mParent(NULL)
    ,mName(NULL)
{
    Initializer(Name);
}

VirtualFile::VirtualFile(LPCWSTR Name, INT InitialSize)
    :mSize(0)
    ,mAllocationSize(0)
    ,mAttributes(0)  
    ,mParent(NULL)
    ,mName(NULL)
{
    mStream = malloc(InitialSize);
    set_AllocationSize(InitialSize);
    Initializer(Name);
}

VirtualFile::~VirtualFile()
{
    if(mStream)
    {
        free(mStream);
    }
    if(mName)
    {
        free(mName);
    }
}

VOID VirtualFile::set_AllocationSize(__int64 Value)
{
    if(mAllocationSize != Value) {

        mStream = realloc(mStream, (size_t)Value);
        if(Value > 0) {
            assert(mStream);
        }
        mAllocationSize = Value;
    }
}

__int64 VirtualFile::get_AllocationSize(VOID)
{
    return mAllocationSize;
}

VOID VirtualFile::set_Size(__int64 Value)
{
    mSize = Value;
}

__int64 VirtualFile::get_Size(VOID)
{
    return mSize;
}

LPWSTR VirtualFile::get_Name(VOID)
{
    assert(mName);
    return (mName);
}

FILETIME VirtualFile::get_CreationTime(VOID)
{
    return (mCreationTime);
}
VOID VirtualFile::set_CreationTime(FILETIME Value)
{
    mCreationTime = Value;
}

FILETIME VirtualFile::get_LastAccessTime(VOID)
{
    return (mLastAccessTime);
}
VOID VirtualFile::set_LastAccessTime(FILETIME Value)
{
    mLastAccessTime = Value;
}

FILETIME VirtualFile::get_LastWriteTime(VOID)
{
    return (mLastWriteTime);
}

VOID VirtualFile::set_LastWriteTime(FILETIME Value)
{
    mLastWriteTime = Value;
}

INT VirtualFile::get_FileAttributes(VOID)
{
    return (mAttributes);
}

VOID VirtualFile::set_FileAttributes(INT Value)
{
        mAttributes = Value;
}

VirtualFile* VirtualFile::get_Parent(VOID)
{
    return (mParent);
}

VOID VirtualFile::set_Parent(VirtualFile* Value)
{
        mParent = Value;
}

VOID VirtualFile::Rename(LPCWSTR NewName)
{

    if(mName)
    {
        free(mName);
        mName = NULL;
    }
    assert(NewName);
    Initializer(NewName);
}

VOID VirtualFile::AddFile(VirtualFile* vfile)
{
    get_Context()->AddFile(vfile);
    vfile->set_Parent(this);
}

VOID VirtualFile::Remove(VOID)
{
    assert(mParent);
        mParent->get_Context()->Remove(this);
        mParent = NULL;
}
/*
VOID VirtualFile::set_Context(DirectoryEnumerationContext* Value)
{
        mEnumCtx = Value;
}
*/
DirectoryEnumerationContext* VirtualFile::get_Context(VOID)
{
        return (&mEnumCtx);
   
}

VOID VirtualFile::Write(PVOID WriteBuf, LONG Position, INT BytesToWrite, PDWORD BytesWritten)
{
    assert(WriteBuf);
    
    if(mSize - Position < BytesToWrite)
    {
        set_Size(Position + BytesToWrite);
    }
    if(mAllocationSize < mSize)
        set_AllocationSize(mSize);

    memcpy((PVOID)&((PBYTE)mStream)[Position] , WriteBuf, BytesToWrite);
    *BytesWritten = BytesToWrite;
}

VOID VirtualFile::Read(PVOID ReadBuf, LONG Position, INT BytesToRead, PDWORD BytesRead)
{
    assert(ReadBuf);
    assert((Position + BytesToRead) <= mAllocationSize);
    {
        LONG MaxRead = (mSize - (__int64)Position) < BytesToRead ? (LONG)(mSize - (__int64)Position) : BytesToRead;
        memcpy(ReadBuf, (PVOID)&((PBYTE)mStream)[Position], MaxRead);
        *BytesRead = MaxRead;
    }
}

VOID VirtualFile::Initializer(LPCWSTR Name)
{
    assert(Name);
    assert(mName == NULL);

    mCreationTime.dwLowDateTime = 0;
    mCreationTime.dwHighDateTime = 0;

    mLastAccessTime.dwHighDateTime = 0;
    mLastAccessTime.dwLowDateTime = 0;

    mLastWriteTime.dwHighDateTime = 0;
    mLastWriteTime.dwLowDateTime = 0;

    mName = (LPWSTR)malloc( ( wcslen(Name) + 1 ) * sizeof(WCHAR) );
    wcscpy(mName, Name);
}



//class DiskEnumerationContext

DirectoryEnumerationContext::DirectoryEnumerationContext()
{
    mIndex = mFileList.begin();
}

BOOL DirectoryEnumerationContext::GetNextFile(VirtualFile*& vfile)
{
    bool Result = FALSE;
    
    if( mIndex != mFileList.end())
    {
        vfile = *mIndex;
        ++mIndex;
        Result = TRUE;
    }
    return Result;
}

BOOL DirectoryEnumerationContext::GetFile(INT Index, VirtualFile*& vfile)
{
    bool Result = FALSE;
    
    if( Index < (INT)mFileList.size())
    {
        std::list<VirtualFile*>::const_iterator p = mFileList.begin();
        while(Index-- > 0) { ++p; }
        vfile = *p;            
        Result = TRUE;
    }
    return Result;
}

BOOL DirectoryEnumerationContext::GetFile(LPCWSTR FileName, VirtualFile*& vfile)
{
    vfile = NULL;
    std::list<VirtualFile*>::const_iterator p = mFileList.begin();

    while(p != mFileList.end())
    {
        if(!wcsicmp((*p)->get_Name(), FileName))
        {
            vfile = *p;
            return TRUE;
        }
        ++p;
    }
    return FALSE;
}

VOID DirectoryEnumerationContext::AddFile(VirtualFile* vfile)
{
    mFileList.push_back(vfile);
    ResetEnumeration();
}

VOID DirectoryEnumerationContext::Remove(VirtualFile* vfile)
{
    std::list<VirtualFile*>::const_iterator p = mFileList.begin();
    while(p != mFileList.end())
    {
        if(!wcsicmp((*p)->get_Name(), vfile->get_Name()))
        {
            mFileList.remove(vfile);
            ResetEnumeration();
            break;
        }
        ++p;
    }
}

VOID DirectoryEnumerationContext::ResetEnumeration()
{
    mIndex = mFileList.begin();
}

BOOL DirectoryEnumerationContext::IsEmpty()
{
    return (mFileList.size() == 0);
}
