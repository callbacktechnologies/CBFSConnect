#include <assert.h> 

#ifdef _UNICODE
#include "../../include/unicode/cbfs.h"
#else
#include "../../include/cbfs.h"
#endif

#include "virtualfile.h"
#include <sddl.h>
#include <tchar.h>

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
    ,mSecurityDescriptor(NULL)
    ,mSecurityDescriptorLength(0)
{
    Initializer(Name);
}

VirtualFile::VirtualFile(LPCWSTR Name, INT InitialSize)
    :mSize(0)
    ,mAllocationSize(0)
    ,mAttributes(0)  
    ,mParent(NULL)
    ,mName(NULL)
    ,mSecurityDescriptor(NULL)
    ,mSecurityDescriptorLength(0)
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
    if (mSecurityDescriptor)
    {
        LocalFree(mSecurityDescriptor);
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

PSECURITY_DESCRIPTOR VirtualFile::get_SecurityDescriptor(VOID)
{
    if (mSecurityDescriptor == NULL) AllocateDefaultSecurityDescriptor();
    return (mSecurityDescriptor);
}

INT VirtualFile::get_SecurityDescriptor(INT SecurityInformation, PVOID Buffer, INT BufferLength, PINT SecurityDescriptorLength)
{
    BOOL Result = TRUE;
    INT  ResultCode = ERROR_SUCCESS;

    SECURITY_DESCRIPTOR  NewSecurityDescriptor = { 0 };

    if (mSecurityDescriptor == NULL) AllocateDefaultSecurityDescriptor();

    InitializeSecurityDescriptor(&NewSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);

    //
    // Owner
    //
    if (SecurityInformation & OWNER_SECURITY_INFORMATION)
    {
        PSID Owner = NULL;
        BOOL OwnerDefaulted = FALSE;

        Result = GetSecurityDescriptorOwner(mSecurityDescriptor,
                                            &Owner,
                                            &OwnerDefaulted);
        if (Result == FALSE)
            goto end;
        
        Result = SetSecurityDescriptorOwner(&NewSecurityDescriptor,
                                            Owner,
                                            OwnerDefaulted);
        if (Result == FALSE)
            goto end;
    }

    //
    // Group
    //
    if (SecurityInformation & GROUP_SECURITY_INFORMATION)
    {
        PSID Group = NULL;
        BOOL GroupDefaulted = FALSE;

        Result = GetSecurityDescriptorGroup(mSecurityDescriptor,
                                            &Group,
                                            &GroupDefaulted);
        if (Result == FALSE)
            goto end;

        Result = SetSecurityDescriptorGroup(&NewSecurityDescriptor,
                                            Group,
                                            GroupDefaulted);
        if (Result == FALSE)
            goto end;
    }

    //
    // Dacl
    //
    if (SecurityInformation & DACL_SECURITY_INFORMATION)
    {
        BOOL DaclPresent = FALSE;
        PACL Dacl = NULL;
        BOOL DaclDefaulted = FALSE;

        Result = GetSecurityDescriptorDacl(mSecurityDescriptor,
                                           &DaclPresent,
                                           &Dacl,
                                           &DaclDefaulted);
        if (Result == FALSE)
            goto end;

        Result = SetSecurityDescriptorDacl(&NewSecurityDescriptor,
                                           DaclPresent,
                                           Dacl,
                                           DaclDefaulted);
        if (Result == FALSE)
            goto end;
    }

    //
    // Sacl
    //
    if (SecurityInformation & SACL_SECURITY_INFORMATION)
    {
        BOOL SaclPresent = FALSE;
        PACL Sacl = NULL;
        BOOL SaclDefaulted = FALSE;

        Result = GetSecurityDescriptorSacl(mSecurityDescriptor,
                                           &SaclPresent,
                                           &Sacl,
                                           &SaclDefaulted);
        if (Result == FALSE)
            goto end;

        Result = SetSecurityDescriptorSacl(&NewSecurityDescriptor,
                                           SaclPresent,
                                           Sacl,
                                           SaclDefaulted);
        if (Result == FALSE)
            goto end;
    }

    //
    // Make
    //
    {
        PSECURITY_DESCRIPTOR SRSD = NULL;
        DWORD SRSDLength = 0;

        Result = MakeSelfRelativeSD(&NewSecurityDescriptor, SRSD, &SRSDLength);
        if (Result == FALSE && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            goto end;

        *SecurityDescriptorLength = SRSDLength;

        if (BufferLength < SRSDLength ||
            Buffer == NULL)
        {
            ResultCode = ERROR_MORE_DATA;
            Result = TRUE;

            goto end;
        }

        SRSD = LocalAlloc(LPTR, SRSDLength);
        Result = MakeSelfRelativeSD(&NewSecurityDescriptor, SRSD, &SRSDLength);
        if (Result == FALSE)
        {
            LocalFree(SRSD);
            goto end;
        }

        memcpy(Buffer, SRSD, SRSDLength);

        LocalFree(SRSD);
    }

end:

    if (Result == FALSE)
        ResultCode = GetLastError();

    return ResultCode;
}

INT VirtualFile::set_SecurityDescriptor(INT SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, INT SecurityDescriptorLength)
{
    BOOL Result = TRUE;
    INT  ResultCode = ERROR_SUCCESS;

    PSECURITY_DESCRIPTOR TargetSecurityDescriptor = NULL;
    SECURITY_DESCRIPTOR  NewSecurityDescriptor = { 0 };

    if (mSecurityDescriptor == NULL) AllocateDefaultSecurityDescriptor();

    InitializeSecurityDescriptor(&NewSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);

    //
    // Owner
    //
    {
        PSID Owner = NULL;
        BOOL OwnerDefaulted = FALSE;

        if (SecurityInformation & OWNER_SECURITY_INFORMATION)
            TargetSecurityDescriptor = SecurityDescriptor;
        else
            TargetSecurityDescriptor = mSecurityDescriptor;

        Result = GetSecurityDescriptorOwner(TargetSecurityDescriptor,
                                            &Owner,
                                            &OwnerDefaulted);
        if (Result == FALSE)
            goto end;
        
        Result = SetSecurityDescriptorOwner(&NewSecurityDescriptor,
                                            Owner,
                                            OwnerDefaulted);
        if (Result == FALSE)
            goto end;
    }


    //
    // Group
    //
    {
        PSID Group = NULL;
        BOOL GroupDefaulted = FALSE;

        if (SecurityInformation & GROUP_SECURITY_INFORMATION)
            TargetSecurityDescriptor = SecurityDescriptor;
        else
            TargetSecurityDescriptor = mSecurityDescriptor;

        Result = GetSecurityDescriptorGroup(TargetSecurityDescriptor,
                                            &Group,
                                            &GroupDefaulted);
        if (Result == FALSE)
            goto end;

        Result = SetSecurityDescriptorGroup(&NewSecurityDescriptor,
                                            Group,
                                            GroupDefaulted);
        if (Result == FALSE)
            goto end;
    }

    //
    // Dacl
    //
    {
        BOOL DaclPresent = FALSE;
        PACL Dacl = NULL;
        BOOL DaclDefaulted = FALSE;

        if (SecurityInformation & DACL_SECURITY_INFORMATION)
            TargetSecurityDescriptor = SecurityDescriptor;
        else
            TargetSecurityDescriptor = mSecurityDescriptor;

        Result = GetSecurityDescriptorDacl(TargetSecurityDescriptor,
                                           &DaclPresent,
                                           &Dacl,
                                           &DaclDefaulted);
        if (Result == FALSE)
            goto end;

        Result = SetSecurityDescriptorDacl(&NewSecurityDescriptor,
                                           DaclPresent,
                                           Dacl,
                                           DaclDefaulted);
        if (Result == FALSE)
            goto end;
    }

    //
    // Sacl
    //
    {
        BOOL SaclPresent = FALSE;
        PACL Sacl = NULL;
        BOOL SaclDefaulted = FALSE;

        if (SecurityInformation & SACL_SECURITY_INFORMATION)
            TargetSecurityDescriptor = SecurityDescriptor;
        else
            TargetSecurityDescriptor = mSecurityDescriptor;

        Result = GetSecurityDescriptorSacl(TargetSecurityDescriptor,
                                           &SaclPresent,
                                           &Sacl,
                                           &SaclDefaulted);
        if (Result == FALSE)
            goto end;

        Result = SetSecurityDescriptorSacl(&NewSecurityDescriptor,
                                           SaclPresent,
                                           Sacl,
                                           SaclDefaulted);
        if (Result == FALSE)
            goto end;
    }

    //
    // Make
    //
    {
        PSECURITY_DESCRIPTOR SRSD = NULL;
        DWORD SRSDLength = 0;

        Result = MakeSelfRelativeSD(&NewSecurityDescriptor, SRSD, &SRSDLength);
        if (Result == FALSE && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            goto end;

        SRSD = LocalAlloc(LPTR, SRSDLength);
        Result = MakeSelfRelativeSD(&NewSecurityDescriptor, SRSD, &SRSDLength);
        if (Result == FALSE)
        {
            LocalFree(SRSD);
            goto end;
        }

        LocalFree(mSecurityDescriptor);

        mSecurityDescriptor = SRSD;
        mSecurityDescriptorLength = SRSDLength;
    }

end:

    if (Result == FALSE)
        ResultCode = GetLastError();

    return ResultCode;
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

LPWSTR VirtualFile::GetCurrentUserSid()
{
    HANDLE      AccessToken = NULL;
    PTOKEN_USER UserToken = NULL;
    LPWSTR      StringSid = NULL;
    DWORD       InfoSize;

    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &AccessToken))
    {
        if (GetLastError() == ERROR_NO_TOKEN)
        {
            OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &AccessToken);
        }
    }

    UserToken = (PTOKEN_USER)malloc(1024);
    GetTokenInformation(AccessToken, TokenUser, UserToken, 1024, &InfoSize);

    ConvertSidToStringSid(UserToken->User.Sid, &StringSid);

    if (AccessToken)
        CloseHandle(AccessToken);
    if (UserToken)
        free(UserToken);

    return StringSid;
}

VOID VirtualFile::AllocateDefaultSecurityDescriptor()
{
    if (mSecurityDescriptor)
    {
        LocalFree(mSecurityDescriptor);
        mSecurityDescriptor = NULL;
        mSecurityDescriptorLength = 0;
    }

    //
    // Format
    // O : owner_sid, SID string: https://docs.microsoft.com/en-us/windows/win32/secauthz/sid-strings
    // G : group_sid, SID string: https://docs.microsoft.com/en-us/windows/win32/secauthz/sid-strings
    // D : dacl_flags(string_ace1)(string_ace2)... (string_acen), ACE string: https://docs.microsoft.com/en-us/windows/win32/secauthz/ace-strings
    // S : sacl_flags(string_ace1)(string_ace2)... (string_acen), ACE string: https://docs.microsoft.com/en-us/windows/win32/secauthz/ace-strings
    //
    // ACE string syntax: ace_type;ace_flags;rights;object_guid;inherit_object_guid;account_sid;(resource_attribute)
    //                    https://docs.microsoft.com/en-us/windows/win32/secauthz/ace-strings
    //

    LPWSTR CurentUserSid = GetCurrentUserSid();
    WCHAR  StringSecurityDescriptor[2048];
    DWORD  CUAccessRights, AUAccessRights, BAAccessRights;

    //
    // Current User
    //
    CUAccessRights = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;

    //
    // Authenticated User
    //
    AUAccessRights = FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE | DELETE;

    //
    // Administrator User
    //
    BAAccessRights = FILE_ALL_ACCESS;


    //
    // ------------------- File ----------------
    // Owner: Current User
    // Group: Current User
    // -----------------------------------------
    //
    if (!(mAttributes & FILE_ATTRIBUTE_DIRECTORY))
        wsprintf(StringSecurityDescriptor,
                 _T("O:%sG:%sD:(A;;0x%X;;;%s)(A;;0x%X;;;AU)(A;;0x%X;;;BA)"),
                 CurentUserSid, CurentUserSid, CUAccessRights, CurentUserSid, AUAccessRights, BAAccessRights);
    //
    // ------------------ Directory -------------
    // Owner: Current User
    // Group: Current User
    // ------------------------------------------
    //
    else
        wsprintf(StringSecurityDescriptor,
                 _T("O:%sG:%sD:(A;OICI;0x%X;;;%s)(A;OICI;0x%X;;;AU)(A;OICI;0x%X;;;BA)"),
                 CurentUserSid, CurentUserSid, CUAccessRights, CurentUserSid, AUAccessRights, BAAccessRights);

    LocalFree(CurentUserSid);

    if (!ConvertStringSecurityDescriptorToSecurityDescriptor(StringSecurityDescriptor,
                                                             SECURITY_DESCRIPTOR_REVISION,
                                                             &mSecurityDescriptor,
                                                             &mSecurityDescriptorLength))
    {
        _tprintf(_T("ConvertStringSecurityDescriptorToSecurityDescriptor Error %u\n"),
            GetLastError());
        goto Cleanup;
    }

Cleanup:

    return;
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
