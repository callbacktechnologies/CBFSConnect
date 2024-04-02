#include <assert.h>

#include "virtualfile.h"

#ifdef _UNICODE
#include "../../include/unicode/fuse.h"
#else
#include "../../include/fuse.h"
#endif

//class VirtualFile
VirtualFile::VirtualFile()
{

}

VirtualFile::VirtualFile(const fuse_char *Name)
    :mStream(NULL)
    ,mSize(0)
    ,mAllocationSize(0)
    ,mMode(0)
    ,mParent(NULL)
    ,mName(NULL)
{
    Initializer(Name);
}

VirtualFile::VirtualFile(const fuse_char *Name, int Mode)
    :mStream(NULL)
    ,mSize(0)
    ,mAllocationSize(0)
    ,mMode(Mode)
    ,mParent(NULL)
    ,mName(NULL)
{
    Initializer(Name);
}

VirtualFile::VirtualFile(const fuse_char *Name, int Mode, int InitialSize)
    :mStream(NULL)
    ,mSize(0)
    ,mAllocationSize(0)
    ,mMode(Mode)
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

void VirtualFile::set_AllocationSize(int64 Value)
{
    if(mAllocationSize != Value) {

        mStream = realloc(mStream, (size_t)Value);
        if(Value > 0) {
            assert(mStream);
        }
        mAllocationSize = Value;
    }
}

int64 VirtualFile::get_AllocationSize(void)
{
    return mAllocationSize;
}

void VirtualFile::set_Size(int64 Value)
{
    mSize = Value;
}

int64 VirtualFile::get_Size(void)
{
    return mSize;
}

fuse_char *VirtualFile::get_Name(void)
{
    assert(mName);
    return (mName);
}

int64 VirtualFile::get_CreationTime(void)
{
    return (mCreationTime);
}

void VirtualFile::set_CreationTime(int64 Value)
{
    mCreationTime = Value;
}

int64 VirtualFile::get_LastAccessTime(void)
{
    return (mLastAccessTime);
}
void VirtualFile::set_LastAccessTime(int64 Value)
{
    mLastAccessTime = Value;
}

int64 VirtualFile::get_LastWriteTime(void)
{
    return (mLastWriteTime);
}
void VirtualFile::set_LastWriteTime(int64 Value)
{
    mLastWriteTime = Value;
}

int VirtualFile::get_Mode(void)
{
    return (mMode);
}

void VirtualFile::set_Mode(int Value)
{
    mMode = Value;
}

int VirtualFile::get_Uid(void)
{
  return (mUid);
}

void VirtualFile::set_Uid(int Value)
{
  mUid = Value;
}

int VirtualFile::get_Gid(void)
{
  return (mGid);
}

void VirtualFile::set_Gid(int Value)
{
  mGid = Value;
}

VirtualFile* VirtualFile::get_Parent(void)
{
    return (mParent);
}

void VirtualFile::set_Parent(VirtualFile* Value)
{
        mParent = Value;
}

void VirtualFile::Rename(const fuse_char *NewName)
{
    if(mName)
    {
        free(mName);
        mName = NULL;
    }
    assert(NewName);
    Initializer(NewName);
}

void VirtualFile::AddFile(VirtualFile* vfile)
{
    get_Context()->AddFile(vfile);
    vfile->set_Parent(this);
}

void VirtualFile::Remove(void)
{
    assert(mParent);
    mParent->get_Context()->Remove(this);
    mParent = NULL;
}

DirectoryEnumerationContext* VirtualFile::get_Context(void)
{
    return (&mEnumCtx);   
}

void VirtualFile::Write(void *WriteBuf, int64 Position, int BytesToWrite, int *BytesWritten)
{
    assert(WriteBuf);
    
    if(mSize - Position < BytesToWrite)
    {
        set_Size(Position + BytesToWrite);
    }
    if(mAllocationSize < mSize)
        set_AllocationSize(mSize);

    memcpy((void*)&((char*)mStream)[Position] , WriteBuf, BytesToWrite);
    *BytesWritten = BytesToWrite;
}

void VirtualFile::Read(void *ReadBuf, int64 Position, int BytesToRead, int *BytesRead)
{
    assert(ReadBuf);
    int MaxRead;
    if (Position > mSize)
        MaxRead = 0;
    else
        MaxRead = (mSize - Position) < (int64)BytesToRead ? (int)(mSize - Position) : BytesToRead;
    if (MaxRead > 0)
        memcpy(ReadBuf, (void*)&((char*)mStream)[Position], MaxRead);
    *BytesRead = MaxRead;
}

void VirtualFile::Initializer(const fuse_char *Name)
{
    assert(Name);
    assert(mName == NULL);

    mCreationTime = 0;
    mLastAccessTime = 0;
    mLastWriteTime = 0;

    mName = (fuse_char*)malloc((fuse_slen(Name) + 1) * sizeof(fuse_char));
    fuse_scpy(mName, Name);
}

//class DiskEnumerationContext

DirectoryEnumerationContext::DirectoryEnumerationContext()
{
    mIndex = mFileList.begin();
}

int DirectoryEnumerationContext::GetCount()
{
  return (int)mFileList.size();
}

bool DirectoryEnumerationContext::GetNextFile(VirtualFile*& vfile)
{
    bool Result = false;
    
    if( mIndex != mFileList.end())
    {
        vfile = *mIndex;
        ++mIndex;
        Result = true;
    }
    return Result;
}

bool DirectoryEnumerationContext::GetFile(int Index, VirtualFile*& vfile)
{
    bool Result = false;
    
    if( Index < (int)mFileList.size())
    {
        std::list<VirtualFile*>::const_iterator p = mFileList.begin();
        while(Index-- > 0) { ++p; }
        vfile = *p;            
        Result = true;
    }
    return Result;
}

bool DirectoryEnumerationContext::GetFile(const fuse_char *FileName, VirtualFile*& vfile)
{
    vfile = NULL;
    std::list<VirtualFile*>::const_iterator p = mFileList.begin();

    while(p != mFileList.end())
    {
        if(!fuse_scmp((*p)->get_Name(), FileName))
        {
            vfile = *p;
            return true;
        }
        ++p;
    }
    return false;
}

void DirectoryEnumerationContext::AddFile(VirtualFile* vfile)
{
    mFileList.push_back(vfile);
    ResetEnumeration();
}

void DirectoryEnumerationContext::Remove(VirtualFile* vfile)
{
    std::list<VirtualFile*>::const_iterator p = mFileList.begin();
    while(p != mFileList.end())
    {
        if(!fuse_scmp((*p)->get_Name(), vfile->get_Name()))
        {
            mFileList.remove(vfile);
            ResetEnumeration();
            break;
        }
        ++p;
    }
}

void DirectoryEnumerationContext::ResetEnumeration()
{
    mIndex = mFileList.begin();
}

bool DirectoryEnumerationContext::IsEmpty()
{
    return (mFileList.size() == 0);
}
