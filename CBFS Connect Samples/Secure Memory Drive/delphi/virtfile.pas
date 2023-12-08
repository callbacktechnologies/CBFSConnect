unit VirtFile;

interface

uses
  Windows, Messages, SysUtils, Variants, Classes, Graphics, Controls, Forms,
  Dialogs,
  cbccbfs;

  function ConvertStringSecurityDescriptorToSecurityDescriptor(
             StringSecurityDescriptor: PWideChar;
             StringSDRevision: DWORD;
             var SecurityDescriptor: PSECURITY_DESCRIPTOR;
             SecurityDescriptorSize: PULONG)
             : Boolean; stdcall;
             external 'Advapi32.dll' name'ConvertStringSecurityDescriptorToSecurityDescriptorW';

const
  DELETE       = $00010000;
  READ_CONTROL = $00020000;
  WRITE_DAC    = $00040000;
  WRITE_OWNER  = $00080000;
  SYNCHRONIZE  = $00100000;
  FILE_READ_DATA         = $0001; // file & pipe
  FILE_LIST_DIRECTORY    = $0001; // directory
  FILE_WRITE_DATA        = $0002; // file & pipe
  FILE_ADD_FILE          = $0002; // directory
  FILE_APPEND_DATA       = $0004; // file
  FILE_ADD_SUBDIRECTORY  = $0004; // directory
  FILE_CREATE_PIPE_INSTANCE = $0004; // named pipe
  FILE_READ_EA           = $0008; // file & directory
  FILE_WRITE_EA          = $0010; // file & directory
  FILE_EXECUTE           = $0020; // file
  FILE_TRAVERSE          = $0020; // directory
  FILE_DELETE_CHILD      = $0040; // directory
  FILE_READ_ATTRIBUTES   = $0080; // all
  FILE_WRITE_ATTRIBUTES  = $0100; // all
  FILE_ALL_ACCESS        = STANDARD_RIGHTS_REQUIRED or SYNCHRONIZE or $1FF;
  FILE_GENERIC_READ      = STANDARD_RIGHTS_READ or FILE_READ_DATA or FILE_READ_ATTRIBUTES or FILE_READ_EA or SYNCHRONIZE;
  FILE_GENERIC_WRITE     = STANDARD_RIGHTS_WRITE or FILE_WRITE_DATA or FILE_WRITE_ATTRIBUTES or FILE_WRITE_EA or
    FILE_APPEND_DATA or SYNCHRONIZE;
  FILE_GENERIC_EXECUTE   = STANDARD_RIGHTS_EXECUTE or FILE_READ_ATTRIBUTES or FILE_EXECUTE or SYNCHRONIZE;

  OWNER_SECURITY_INFORMATION = ($00000001);
  GROUP_SECURITY_INFORMATION = ($00000002);
  DACL_SECURITY_INFORMATION  = ($00000004);
  SACL_SECURITY_INFORMATION  = ($00000008);

type
  VirtualFile = class;

  DirectoryEnumerationContext = class
  private
    FFileList: TList;
    FIndex: Integer;

  public
    constructor Create; overload;
    //constructor Create(vfile: VirtualFile);overload;

    function GetNextFile(var vfile: VirtualFile): Boolean;
    function GetFile(FileName: string; var vfile: VirtualFile): Boolean; overload;
    function GetFile(Index: integer; var vfile: VirtualFile): Boolean; overload;
    procedure AddFile(vfile: VirtualFile);
    procedure Remove(vfile: VirtualFile);
    procedure ResetEnumeration;
    function IsEmpty: Boolean;
  end;

  VirtualFile = class
  private
    FEnumCtx: DirectoryEnumerationContext;
    FName: string;
    FStream: array of Byte;
    FAllocationSize: Int64;
    FSize: Int64;
    FAttributes: DWORD;
    FCreationTime: TDateTime;
    FLastAccessTime: TDateTime;
    FLastWriteTime: TDateTime;
    FParent: VirtualFile;
    FSecurityDescriptor: PSECURITY_DESCRIPTOR;

    procedure AllocateDefaultSecurityDescriptor();
    function GetCurrentUserSid() : string;

  public
    constructor Create(Name: string); overload;
    constructor Create(Name: string; InitialSize: Integer); overload;

    destructor Destroy; override;

    procedure Rename(NewName: string);
    procedure Remove;
    procedure SetAllocationSize(Value: Int64);
    procedure Write(var WriteBuf; Position: Int64; BytesToWrite: Integer; var BytesWritten: Cardinal);
    procedure Read(var ReadBuf; Position: Int64; BytesToRead: Integer; var BytesRead: Cardinal);
    procedure AddFile(vfile: VirtualFile);
    function  SetSecurityDescriptor(SecurityInformation: Integer; SecurityDescriptor: Pointer; DescriptorLength: Integer) : Integer;
    function  GetSecurityDescriptor(SecurityInformation: Integer; Buffer: Pointer; BufferLength: Integer; var DescriptorLength: Integer) : Integer;

    //property
    property AllocationSize: Int64 read FAllocationSize write SetAllocationSize;
    property Size: Int64 read FSize write FSize;
    property Name: string read FName write FName;
    property CreationTime: TDateTime read FCreationTime write FCreationTime;
    property LastAccessTime: TDateTime read FLastAccessTime write FLastAccessTime;
    property LastWriteTime: TDateTime read FLastWriteTime write FLastWriteTime;
    property FileAttributes: DWORD read FAttributes write FAttributes;
    property Context: DirectoryEnumerationContext read FEnumCtx;
    property Parent: VirtualFile read FParent write FParent;
    property SecurityDescriptor: PSECURITY_DESCRIPTOR read FSecurityDescriptor;
  end;

implementation

// class DirectoryEnumerationContext

constructor DirectoryEnumerationContext.Create;
begin
  FFileList := TList.Create;
  FIndex := 0;
end;

function DirectoryEnumerationContext.GetNextFile(var vfile: VirtualFile): Boolean;
begin
  if FIndex < FFileList.Count then
  begin
      vfile := FFileList[FIndex];
      inc(FIndex);
      Result := true;
  end
  else
    Result := false;
end;

function DirectoryEnumerationContext.GetFile(Index: Integer; var vfile: VirtualFile): Boolean;

begin
  if Index < FFileList.Count then
  begin
    vfile := FFileList[Index];
    Result := true;
  end
  else
    Result := false;
end;

function DirectoryEnumerationContext.GetFile(FileName: string; var vfile: VirtualFile): Boolean;
var
  Index: Integer;
begin
  Index := 0;
  vfile := nil;
  while Index < FFileList.Count do
  begin
    if StrIComp(PChar(VirtualFile(FFileList[Index]).Name), PChar(FileName)) = 0 then
    begin
      vfile := FFileList[Index];
      Result := true;
      Exit;
    end
    else
      Inc(Index);
  end;
  
  Result := false;
end;

procedure DirectoryEnumerationContext.AddFile(vfile: VirtualFile);
begin
  FFileList.Add(vfile);
  ResetEnumeration;
end;

procedure DirectoryEnumerationContext.Remove(vfile: VirtualFile);
begin
  FFileList.Remove(vfile);
  ResetEnumeration();
end;

procedure DirectoryEnumerationContext.ResetEnumeration;
begin
  FIndex := 0;
end;

function DirectoryEnumerationContext.IsEmpty: Boolean;
begin
    Result := (FFileList.Count = 0);
end;

// VirtualFile

constructor VirtualFile.Create(Name: string);
begin
  FAllocationSize := 0;
  FSize := 0;
  FName := Name;
  FEnumCtx := DirectoryEnumerationContext.Create;
  AllocateDefaultSecurityDescriptor();
end;

constructor VirtualFile.Create(Name: string; InitialSize: Integer);
begin
  System.SetLength(FStream, InitialSize);
  AllocationSize := InitialSize;
  FName := Name;
  AllocateDefaultSecurityDescriptor();
end;

destructor VirtualFile.Destroy;
begin
  if FSecurityDescriptor <> nil then
    LocalFree(HLOCAL(FSecurityDescriptor));
end;

procedure VirtualFile.SetAllocationSize(Value: Int64);
begin
  if FAllocationSize <> Value then
  begin
    System.SetLength(FStream, Value);
    FAllocationSize := System.Length(FStream);
  end;
end;


procedure VirtualFile.Rename(NewName: string);
begin
  FName := NewName;
end;

procedure VirtualFile.Remove;
begin
  Parent.Context.Remove(self);
end;

procedure VirtualFile.AddFile(vfile: VirtualFile);
begin
  FEnumCtx.AddFile(vfile);
  vfile.Parent := self;
end;

procedure VirtualFile.Write(var WriteBuf; Position: Int64; BytesToWrite: Integer; var BytesWritten: Cardinal);
begin
  if (Size - Position) < BytesToWrite then
    Size := Position + BytesToWrite;

  if FAllocationSize < Size then
    System.SetLength(FStream, Size);

  Move(WriteBuf, FStream[Position], BytesToWrite);
  BytesWritten := BytesToWrite;
  FAllocationSize := System.Length(FStream);
end;

procedure VirtualFile.Read(var ReadBuf; Position: Int64; BytesToRead: Integer; var BytesRead: Cardinal);
var
  MaxRead: Int64;
begin
  if (System.Length(FStream) - Position) < BytesToRead then
  begin
    MaxRead := System.Length(FStream) - Position;
  end
  else
    MaxRead :=  BytesToRead;

  Move(FStream[Position], ReadBuf, MaxRead);
  BytesRead := MaxRead;
end;

type
 PTokenUser = ^TTokenUser;
 _TOKEN_USER = record
   User : TSIDAndAttributes;
 end;
 TTokenUser = _TOKEN_USER;
 TOKEN_USER = _TOKEN_USER;

function ConvertSidToStringSid(Sid: PSID; var StringSid: LPWSTR): BOOL; stdcall; external advapi32 name 'ConvertSidToStringSidW';

function VirtualFile.GetCurrentUserSid() : string;
var
  CurrentUserSid: LPWSTR;
  AccessToken: THANDLE;
  UserToken: PTokenUser;
  InfoSize: DWORD;
begin
  if ( not OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, AccessToken)) then
  begin
    if (GetLastError() = ERROR_NO_TOKEN) then
      OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, AccessToken);
  end;

    UserToken := AllocMem(1024);
    GetTokenInformation(AccessToken, TokenUser, UserToken, 1024, InfoSize);

    ConvertSidToStringSid(UserToken.User.Sid, CurrentUserSid);

    CloseHandle(AccessToken);
    Dispose(UserToken);

  Result := string(CurrentUserSid);
  LocalFree(HLOCAL(CurrentUserSid));
end;

procedure VirtualFile.AllocateDefaultSecurityDescriptor();
var
CurentUserSid : string;
CUAccessRights : DWORD;
AUAccessRights : DWORD;
BAAccessRights : DWORD;
StringSecurityDescriptor : string;
SecurityDescriptorLength : ULONG;
begin
  if (FSecurityDescriptor <> nil) then
  begin
    LocalFree(HLOCAL(FSecurityDescriptor));
    FSecurityDescriptor := nil;
  end;

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

    CurentUserSid := GetCurrentUserSid();

    //
    // Current User
    //
    CUAccessRights := FILE_GENERIC_READ or FILE_GENERIC_EXECUTE;

    //
    // Authenticated User
    //
    AUAccessRights := FILE_GENERIC_READ or FILE_GENERIC_WRITE or FILE_GENERIC_EXECUTE or DELETE;

    //
    // Administrator User
    //
    BAAccessRights := FILE_ALL_ACCESS;


    //
    // ------------------- File ----------------
    // Owner: Current User
    // Group: Current User
    // -----------------------------------------
    //
    if (FAttributes and FILE_ATTRIBUTE_DIRECTORY = 0) then
        StringSecurityDescriptor := Format(
                 'O:%sG:%sD:(A;;0x%X;;;%s)(A;;0x%X;;;AU)(A;;0x%X;;;BA)',
                 [CurentUserSid, CurentUserSid, CUAccessRights, CurentUserSid, AUAccessRights, BAAccessRights])
    //
    // ------------------ Directory -------------
    // Owner: Current User
    // Group: Current User
    // ------------------------------------------
    //
    else
        StringSecurityDescriptor := Format(
                 'O:%sG:%sD:(A;OICI;0x%X;;;%s)(A;OICI;0x%X;;;AU)(A;OICI;0x%X;;;BA)',
                 [CurentUserSid, CurentUserSid, CUAccessRights, CurentUserSid, AUAccessRights, BAAccessRights]);

    ConvertStringSecurityDescriptorToSecurityDescriptor(PWideChar(Widestring(StringSecurityDescriptor)),
                                                        SECURITY_DESCRIPTOR_REVISION,
                                                        FSecurityDescriptor,
                                                        PULONG(@SecurityDescriptorLength));

    ASSERT(IsValidSecurityDescriptor(FSecurityDescriptor));
end;

function VirtualFile.SetSecurityDescriptor(SecurityInformation: Integer; SecurityDescriptor: Pointer; DescriptorLength: Integer) : Integer;
var
ResultState : BOOL;
TargetSecurityDescriptor : PSECURITY_DESCRIPTOR;
NewSecurityDescriptor : SECURITY_DESCRIPTOR;
Owner : PSID;
OwnerDefaulted : BOOL;
Group : PSID;
GroupDefaulted : BOOL;
DaclPresent : BOOL;
Dacl : PACL;
DaclDefaulted : BOOL;
SaclPresent : BOOL;
Sacl : PACL;
SaclDefaulted : BOOL;
SRSD : PSECURITY_DESCRIPTOR;
SRSDLength : DWORD;
label
end_label;
begin

    Result := ERROR_SUCCESS;

    if (FSecurityDescriptor = nil) then AllocateDefaultSecurityDescriptor();
    InitializeSecurityDescriptor(@NewSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);

    //
    // Owner
    //

    if (SecurityInformation and OWNER_SECURITY_INFORMATION) <> 0 then
        TargetSecurityDescriptor := SecurityDescriptor
    else
        TargetSecurityDescriptor := FSecurityDescriptor;

    ResultState := GetSecurityDescriptorOwner(TargetSecurityDescriptor,
                                              Owner,
                                              OwnerDefaulted);
    if (ResultState = FALSE) then goto end_label;

    ResultState := SetSecurityDescriptorOwner(@NewSecurityDescriptor,
                                              Owner,
                                              OwnerDefaulted);
    if (ResultState = FALSE) then goto end_label;


    //
    // Group
    //

    if (SecurityInformation and GROUP_SECURITY_INFORMATION) <> 0 then
        TargetSecurityDescriptor := SecurityDescriptor
    else
        TargetSecurityDescriptor := FSecurityDescriptor;

    ResultState := GetSecurityDescriptorGroup(TargetSecurityDescriptor,
                                              Group,
                                              GroupDefaulted);
    if (ResultState = FALSE) then goto end_label;

    ResultState := SetSecurityDescriptorGroup(@NewSecurityDescriptor,
                                              Group,
                                              GroupDefaulted);
    if (ResultState = FALSE) then goto end_label;

    //
    // Dacl
    //

    if (SecurityInformation and DACL_SECURITY_INFORMATION) <> 0 then
        TargetSecurityDescriptor := SecurityDescriptor
    else
        TargetSecurityDescriptor := FSecurityDescriptor;

    ResultState := GetSecurityDescriptorDacl(TargetSecurityDescriptor,
                                             DaclPresent,
                                             Dacl,
                                             DaclDefaulted);
    if (ResultState = FALSE) then goto end_label;

    ResultState := SetSecurityDescriptorDacl(@NewSecurityDescriptor,
                                             DaclPresent,
                                             Dacl,
                                             DaclDefaulted);
    if (ResultState = FALSE) then goto end_label;

    //
    // Sacl
    //

    if (SecurityInformation and SACL_SECURITY_INFORMATION) <> 0 then
        TargetSecurityDescriptor := SecurityDescriptor
    else
        TargetSecurityDescriptor := FSecurityDescriptor;

    ResultState := GetSecurityDescriptorSacl(TargetSecurityDescriptor,
                                             SaclPresent,
                                             Sacl,
                                             SaclDefaulted);
    if (ResultState = FALSE) then goto end_label;

    ResultState := SetSecurityDescriptorSacl(@NewSecurityDescriptor,
                                             SaclPresent,
                                             Sacl,
                                             SaclDefaulted);
    if (ResultState = FALSE) then goto end_label;

    //
    // Make
    //

    SRSD := nil;
    SRSDLength := 0;

    ResultState := MakeSelfRelativeSD(@NewSecurityDescriptor, PSecurityDescriptor(SRSD), SRSDLength);
    if ((ResultState = FALSE) and (GetLastError() <> ERROR_INSUFFICIENT_BUFFER)) then goto end_label;

    SRSD := Pointer(LocalAlloc(LPTR, SRSDLength));
    ResultState := MakeSelfRelativeSD(@NewSecurityDescriptor, PSecurityDescriptor(SRSD), SRSDLength);
    if (ResultState = FALSE) then
    begin
       LocalFree(HLOCAL(SRSD));
       goto end_label;
    end;

    LocalFree(HLOCAL(FSecurityDescriptor));

    FSecurityDescriptor := SRSD;

end_label:

    if (ResultState = FALSE) then Result := GetLastError();

end;

function VirtualFile.GetSecurityDescriptor(SecurityInformation: Integer; Buffer: Pointer; BufferLength: Integer; var DescriptorLength: Integer) : Integer;
var
ResultState : BOOL;
NewSecurityDescriptor : SECURITY_DESCRIPTOR;
Owner : PSID;
OwnerDefaulted : BOOL;
Group : PSID;
GroupDefaulted : BOOL;
DaclPresent : BOOL;
Dacl : PACL;
DaclDefaulted : BOOL;
SaclPresent : BOOL;
Sacl : PACL;
SaclDefaulted : BOOL;
SRSD : PSECURITY_DESCRIPTOR;
SRSDLength : DWORD;
label
end_label;
begin

    Result := ERROR_SUCCESS;

    if (FSecurityDescriptor = nil) then AllocateDefaultSecurityDescriptor();
    InitializeSecurityDescriptor(@NewSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);

    //
    // Owner
    //

    if (SecurityInformation and OWNER_SECURITY_INFORMATION) <> 0 then
    begin
        ResultState := GetSecurityDescriptorOwner(FSecurityDescriptor,
                                                  Owner,
                                                  OwnerDefaulted);
        if (ResultState = FALSE) then goto end_label;

        ResultState := SetSecurityDescriptorOwner(@NewSecurityDescriptor,
                                                  Owner,
                                                  OwnerDefaulted);
        if (ResultState = FALSE) then goto end_label;
    end;

    //
    // Group
    //

    if (SecurityInformation and GROUP_SECURITY_INFORMATION) <> 0 then
    begin
        ResultState := GetSecurityDescriptorGroup(FSecurityDescriptor,
                                                  Group,
                                                  GroupDefaulted);
        if (ResultState = FALSE) then goto end_label;

        ResultState := SetSecurityDescriptorGroup(@NewSecurityDescriptor,
                                                  Group,
                                                  GroupDefaulted);
        if (ResultState = FALSE) then goto end_label;
    end;



    //
    // Dacl
    //

    if (SecurityInformation and DACL_SECURITY_INFORMATION) <> 0 then
    begin
        ResultState := GetSecurityDescriptorDacl(FSecurityDescriptor,
                                                 DaclPresent,
                                                 Dacl,
                                                 DaclDefaulted);
        if (ResultState = FALSE) then goto end_label;

        ResultState := SetSecurityDescriptorDacl(@NewSecurityDescriptor,
                                                 DaclPresent,
                                                 Dacl,
                                                 DaclDefaulted);
        if (ResultState = FALSE) then goto end_label;
    end;

    //
    // Sacl
    //

    if (SecurityInformation and SACL_SECURITY_INFORMATION) <> 0 then
    begin
        ResultState := GetSecurityDescriptorSacl(FSecurityDescriptor,
                                                 SaclPresent,
                                                 Sacl,
                                                 SaclDefaulted);
        if (ResultState = FALSE) then goto end_label;

        ResultState := SetSecurityDescriptorSacl(@NewSecurityDescriptor,
                                                 SaclPresent,
                                                 Sacl,
                                                 SaclDefaulted);
        if (ResultState = FALSE) then goto end_label;
    end;

    //
    // Make
    //

    SRSD := nil;
    SRSDLength := 0;

    ResultState := MakeSelfRelativeSD(@NewSecurityDescriptor, PSecurityDescriptor(SRSD), SRSDLength);
    if ((ResultState = FALSE) and (GetLastError() <> ERROR_INSUFFICIENT_BUFFER)) then goto end_label;

    DescriptorLength := SRSDLength;

    if (BufferLength < SRSDLength) or (Buffer = nil) then
    begin
      Result := ERROR_MORE_DATA;
      ResultState := TRUE;
      goto end_label;
    end;

    SRSD := Pointer(LocalAlloc(LPTR, SRSDLength));
    ResultState := MakeSelfRelativeSD(@NewSecurityDescriptor, PSecurityDescriptor(SRSD), SRSDLength);
    if (ResultState = FALSE) then
    begin
       LocalFree(HLOCAL(SRSD));
       goto end_label;
    end;

    CopyMemory(Buffer, SRSD, SRSDLength);

    LocalFree(HLOCAL(SRSD));

end_label:

    if (ResultState = FALSE) then Result := GetLastError();

end;
end.
