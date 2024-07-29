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
    , mReparseTag(0)
    , mReparseBuffer(NULL)
    , mReparseBufferLength(0)
{
    Initializer(Name);
}

VirtualFile::VirtualFile(LPCWSTR Name, INT InitialSize)
    :mSize(0)
    ,mAllocationSize(0)
    ,mAttributes(0)  
    ,mParent(NULL)
    ,mName(NULL)
    ,mReparseTag(0)
    ,mReparseBuffer(NULL)
    ,mReparseBufferLength(0)
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
    if (mReparseBuffer)
    {
        free(mReparseBuffer);
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

__int64 VirtualFile::get_ReparseTag(VOID)
{
    return mReparseTag;
}

VOID VirtualFile::set_ReparseTag(__int64 Value)
{
    mReparseTag = Value;
}

PVOID VirtualFile::get_ReparseBuffer(VOID)
{
    return mReparseBuffer;
}

VOID VirtualFile::set_ReparseBuffer(PVOID Value)
{
    mReparseBuffer = Value;
}

WORD VirtualFile::get_ReparseBufferLength(VOID)
{
    return mReparseBufferLength;
}

VOID VirtualFile::set_ReparseBufferLength(SHORT Value)
{
    mReparseBufferLength = Value;
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

VOID VirtualFile::CreateReparsePoint(LPWSTR ReparsePath)
{

    PREPARSE_DATA_BUFFER reparseDataBuffer = NULL;
    SHORT linkLength = 0, reparseDataBufferLength = 0;
    BOOL relative = FALSE;
    LPWSTR reparseTarget = NULL;

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
    reparseDataBuffer->SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
    reparseDataBuffer->SymbolicLinkReparseBuffer.SubstituteNameLength = linkLength;
    reparseDataBuffer->SymbolicLinkReparseBuffer.PrintNameLength = 0;
    reparseDataBuffer->SymbolicLinkReparseBuffer.PrintNameOffset = linkLength;
    wcscpy(reparseDataBuffer->SymbolicLinkReparseBuffer.PathBuffer, reparseTarget);
    if (!relative)
        free(reparseTarget);

    mReparseBuffer = reparseDataBuffer;
    mReparseBufferLength = reparseDataBufferLength;
    mReparseTag = reparseDataBuffer->ReparseTag;
    mAttributes = FILE_ATTRIBUTE_REPARSE_POINT;
}

VOID VirtualFile::DeleteReparsePoint()
{
    if (mReparseBuffer != NULL)
    {
        free(mReparseBuffer);
        mReparseBuffer = NULL;
    }
    mReparseTag = 0;
    mAttributes &= FILE_ATTRIBUTE_REPARSE_POINT;
    if (mAttributes == 0)
        mAttributes = FILE_ATTRIBUTE_NORMAL;
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
