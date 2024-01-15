(*
 * CBFS Connect 2024 Delphi Edition - Sample Project
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
 *)
// Uncomment the following if you want to support disk quotas.
{.$define DISK_QUOTAS_SUPPORTED}

unit FolderDriveF;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms,
  Dialogs, StdCtrls, StrUtils, ComObj, ActiveX, ShellAPI, IOUtils, RegularExpressions,
  cbccore, cbcconstants, cbccbfs;

const
  SERVICE_STOPPED = 1;
  SERVICE_START_PENDING = 2;
  SERVICE_STOP_PENDING = 3;
  SERVICE_RUNNING = 4;
  SERVICE_CONTINUE_PENDING = 5;
  SERVICE_PAUSE_PENDING = 6;
  SERVICE_PAUSED = 7;

  SECTOR_SIZE = 512;

  FILE_READ_DATA            = $0001;    // file & pipe
  FILE_WRITE_DATA           = $0002;    // file & pipe
  FILE_APPEND_DATA          = $0004;    // file
  FILE_READ_EA              = $0008;    // file & directory
  FILE_WRITE_EA             = $0010;    // file & directory

  FILE_OPEN = 1;
  FILE_OPEN_IF = 3;

type

  PWIN32_FIND_STREAM_DATA = ^WIN32_FIND_STREAM_DATA;
  WIN32_FIND_STREAM_DATA = packed record
    StreamSize : Int64;
    cStreamName : array [0..MAX_PATH + 36-1] of WideChar;
  end;

  TGetFileInfoFunc = function(hFile: THandle; FileInformationClass: Cardinal;
    lpFileInformation: Pointer; dwBufferSize: LongWord):Boolean; stdcall;

  TFindFirstFileNameFunc = function (lpFileName: PChar; dwFlags: LongWord;
    var StringLength: LongWord; LinkName: PChar): THandle; stdcall;

  TFindNextFileNameFunc = function (hFile: THandle; var StringLength: LongWord;
    LinkName: PChar): Boolean; stdcall;

  TFindFirstStreamFunc = function (lpFileName: PChar; dwLevel: LongWord;
    var StreamData : WIN32_FIND_STREAM_DATA; dwFlags: LongWord): THandle; stdcall;

  TFindNextStreamFunc = function (hFile: THandle; var StreamData : WIN32_FIND_STREAM_DATA): Boolean; stdcall;

  PSYSTEM_STRING = ^SYSTEM_STRING;
  SYSTEM_STRING = packed record
    Length : WORD;
    MaximumLength : WORD;
    Buffer : PChar;
  end;

  PIO_STATUS_BLOCK = ^IO_STATUS_BLOCK;
  IO_STATUS_BLOCK = packed record
   Status : Integer;
   Information : ULONG;
  end;

  POBJECT_ATTRIBUTES = ^OBJECT_ATTRIBUTES;
  OBJECT_ATTRIBUTES = packed record
    Length : ULONG;
    RootDirectory : THandle;
    ObjectName : PSYSTEM_STRING;
    Attributes : ULONG;
    SecurityDescriptor : Pointer;
    SecurityQualityOfService : Pointer;
  end;

  PFILE_NAME_INFO = ^FILE_NAME_INFO;
  FILE_NAME_INFO = packed record
    FileNameLength: DWORD;
    FileName: array[0..255] of Char;
  end;

var
  FindFirstStream: TFindFirstStreamFunc;
  FindNextStream: TFindNextStreamFunc;
  FindFirstFileName: TFindFirstFileNameFunc;
  FindNextFileName: TFindNextFileNameFunc;

const
   WM_SET_INPUTBOX_LEN = WM_USER + 1;

type

  TFormFolderDrive = class(TForm)
    lblIntro: TLabel;
    gbInstallDrivers: TGroupBox;
    lblInstallDrivers: TLabel;
    gbCreateVirtualDrive: TGroupBox;
    gbMountVirtualMedia: TGroupBox;
    gbAddMountingPoint: TGroupBox;
    lblCreateVirtualDrive: TLabel;
    btnCreateVirtualDrive: TButton;
    btnDeleteVirtualDrive: TButton;
    lblMountVirtualMedia: TLabel;
    lblMediaLocation: TLabel;
    lblAddMountingPoint: TLabel;
    lblDriverStatus: TLabel;
    btnMount: TButton;
    btnInstall: TButton;
    btnUninstall: TButton;
    edtRootPath: TEdit;
    btnUnmount: TButton;
    edtMountingPoint: TEdit;
    btnAddPoint: TButton;
    btnDeletePoint: TButton;
    lblMountingPoints: TLabel;
    dlgOpenDrv: TOpenDialog;

    procedure btnCreateVirtualDriveClick(Sender: TObject);
    procedure btnDeleteVirtualDriveClick(Sender: TObject);
    procedure btnMountClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure btnInstallClick(Sender: TObject);
    procedure btnUninstallClick(Sender: TObject);
    procedure btnUnmountClick(Sender: TObject);
    procedure btnAddPointClick(Sender: TObject);
    procedure btnDeletePointClick(Sender: TObject);
  private
    FCbFs: TcbcCBFS;
    FRootPath: string;

    procedure AskForReboot(isInstall: Boolean);
    procedure UpdateDriverStatus;
    procedure IsEmptyDirectory(const DirectoryName: String;
      var IsEmpty: Boolean; var ResultCode: Integer);

    procedure CBCanFileBeDeleted(
      Sender: TObject;
      const FileName: String;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var CanBeDeleted: Boolean;
      var ResultCode: Integer
    );
    procedure CBCloseDirectoryEnumeration(
      Sender: TObject;
      const DirectoryName: String;
      var DirectoryContext: Pointer;
      EnumerationContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBCloseFile(
      Sender: TObject;
      const FileName: String;
      PendingDeletion: Boolean;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBCreateFile(
      Sender: TObject;
      const FileName: String;
      DesiredAccess: Integer;
      FileAttributes: Integer;
      ShareMode: Integer;
      NTCreateDisposition: Integer;
      NTDesiredAccess: Integer;
      FileInfo: Int64;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBDeleteFile(
      Sender: TObject;
      const FileName: String;
      var FileContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBEnumerateDirectory(
      Sender: TObject;
      const DirectoryName: String;
      const Mask: String;
      CaseSensitive: Boolean;
      Restart: Boolean;
      RequestedInfo: Integer;
      var FileFound: Boolean;
      var FileName: String;
      var ShortFileName: String;
      var CreationTime: TDateTime;
      var LastAccessTime: TDateTime;
      var LastWriteTime: TDateTime;
      var ChangeTime: TDateTime;
      var Size: Int64;
      var AllocationSize: Int64;
      var FileId: Int64;
      var Attributes: Int64;
      var ReparseTag: Int64;
      HandleInfo: Int64;
      var DirectoryContext: Pointer;
      var HandleContext: Pointer;
      var EnumerationContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBReadFile(
      Sender: TObject;
      const FileName: String;
      Position: Int64;
      Buffer: Pointer;
      BytesToRead: Int64;
      var BytesRead: Int64;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBWriteFile(
      Sender: TObject;
      const FileName: String;
      Position: Int64;
      Buffer: Pointer;
      BytesToWrite: Int64;
      var BytesWritten: Int64;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBGetFileInfo(
      Sender: TObject;
      const FileName: String;
      RequestedInfo: Integer;
      var FileExists: Boolean;
      var CreationTime: TDateTime;
      var LastAccessTime: TDateTime;
      var LastWriteTime: TDateTime;
      var ChangeTime: TDateTime;
      var Size: Int64;
      var AllocationSize: Int64;
      var FileId: Int64;
      var Attributes: Int64;
      var ReparseTag: Int64;
      var HardLinkCount: Integer;
      var ShortFileName: String;
      var RealFileName: String;
      var ResultCode: Integer
    );
    procedure CBGetVolumeId(
      Sender: TObject;
      var VolumeId: Integer;
      var ResultCode: Integer
    );
    procedure CBGetVolumeLabel(
      Sender: TObject;
      var VolumeLabel: String;
      var ResultCode: Integer
    );
    procedure CBGetVolumeSize(
      Sender: TObject;
      var TotalSectors: Int64;
      var AvailableSectors: Int64;
      var ResultCode: Integer
    );
    procedure CBMount(
      Sender: TObject;
      var ResultCode: Integer
    );
    procedure CBOpenFile(
      Sender: TObject;
      const FileName: String;
      DesiredAccess: Integer;
      FileAttributes: Integer;
      ShareMode: Integer;
      NTCreateDisposition: Integer;
      NTDesiredAccess: Integer;
      FileInfo: Int64;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBRenameOrMoveFile(
      Sender: TObject;
      const FileName: String;
      const NewFileName: String;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBSetFileAttributes(
      Sender: TObject;
      const FileName: String;
      CreationTime: TDateTime;
      LastAccessTime: TDateTime;
      LastWriteTime: TDateTime;
      ChangeTime: TDateTime;
      FileAttributes: Integer;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBSetAllocationSize(
      Sender: TObject;
      const FileName: String;
      AllocationSize: Int64;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBSetFileSize(
      Sender: TObject;
      const FileName: String;
      Size: Int64;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBSetVolumeLabel(
      Sender: TObject;
      const VolumeLabel: String;
      var ResultCode: Integer
    );
    procedure CBIsDirectoryEmpty(
      Sender: TObject;
      const DirectoryName: String;
      var IsEmpty: Boolean;
      var DirectoryContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBUnmount(
      Sender: TObject;
      var ResultCode: Integer
    );

    procedure CBCloseNamedStreamsEnumeration(
      Sender: TObject;
      const FileName: String;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      EnumerationContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBEnumerateNamedStreams(
      Sender: TObject;
      const FileName: String;
      var NamedStreamFound: Boolean;
      var StreamName: String;
      var StreamSize: Int64;
      var StreamAllocationSize: Int64;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var EnumerationContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBGetFileSecurity(
      Sender: TObject;
      const FileName: String;
      SecurityInformation: Integer;
      SecurityDescriptor: Pointer;
      BufferLength: Integer;
      var DescriptorLength: Integer;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBSetFileSecurity(
      Sender: TObject;
      const FileName: String;
      SecurityInformation: Integer;
      SecurityDescriptor: Pointer;
      Length: Integer;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBGetFileNameByFileId(
      Sender: TObject;
      FileId: Int64;
      var FilePath: String;
      var ResultCode: Integer
    );
    procedure CBEjected(Sender: TObject; var ResultCode: Integer);

    procedure CBCreateHardLink(
      Sender: TObject;
      const FileName: String;
      const LinkName: String;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBEnumerateHardLinks(
      Sender: TObject;
      const FileName: String;
      var LinkFound: Boolean;
      var LinkName: String;
      var ParentId: Int64;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var EnumerationContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBCloseHardLinksEnumeration(
      Sender: TObject;
      const FileName: String;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      EnumerationContext: Pointer;
      var ResultCode: Integer
    );
    {$ifdef DISK_QUOTAS_SUPPORTED}
    procedure CBQueryQuotas(
      Sender: TObject;
      SID: Pointer;
      SIDLength: Integer;
      Index: Integer;
      var QuotaFound: Boolean;
      var QuotaUsed: Int64;
      var QuotaThreshold: Int64;
      var QuotaLimit: Int64;
      SIDOut: Pointer;
      SIDOutLength: Integer;
      var EnumerationContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBSetQuotas(
      Sender: TObject;
      SID: Pointer;
      SIDLength: Integer;
      RemoveQuota: Boolean;
      var QuotaFound: Boolean;
      QuotaUsed: Int64;
      QuotaThreshold: Int64;
      QuotaLimit: Int64;
      var EnumerationContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBCloseQuotasEnumeration(
      Sender: TObject;
      EnumerationContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBGetDefaultQuotaInfo(
      Sender: TObject;
      var DefaultQuotaThreshold: Int64;
      var DefaultQuotaLimit: Int64;
      var FileSystemControlFlags: Int64;
      var ResultCode: Integer
    );
    procedure CBSetDefaultQuotaInfo(
      Sender: TObject;
      DefaultQuotaThreshold: Int64;
      DefaultQuotaLimit: Int64;
      FileSystemControlFlags: Int64;
      var ResultCode: Integer
    );
    {$endif}
    procedure CBSetReparsePoint(
      Sender: TObject;
      const FileName: String;
      ReparseTag: Int64;
      ReparseBuffer: Pointer;
      ReparseBufferLength: Integer;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBGetReparsePoint(
      Sender: TObject;
      const FileName: String;
      ReparseBuffer: Pointer;
      var ReparseBufferLength: Integer;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBDeleteReparsePoint(
      Sender: TObject;
      const FileName: String;
      ReparseBuffer: Pointer;
      ReparseBufferLength: Integer;
      HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );

    function GetFileId(FileName: string): Int64;
    function ConvertRelativePathToAbsolute(const path: string; acceptMountingPoint: Boolean): string;
    function IsDriveLetter(const path: string): Boolean;
    {$ifdef DISK_QUOTAS_SUPPORTED}
    function AllocateQuotaContext(Sid: PSID): Pointer;
    procedure FreeQuotaContext(Context: Pointer);
    {$endif}
    function IsQuotasIndexFile(Path: string; var FileId: Int64): Boolean;
    // function IsAdmin(): Boolean;
  public

  end;

{$ifdef DISK_QUOTAS_SUPPORTED}
type
  PTokenUser = ^TTokenUser;
  TTokenUser = packed record
    User: SID_AND_ATTRIBUTES;
  end;

  //
  // Per-user quota information.
  //
  PDISKQUOTA_USER_INFORMATION = ^DISKQUOTA_USER_INFORMATION;
  {$EXTERNALSYM PDISKQUOTA_USER_INFORMATION}
  DiskQuotaUserInformation = record
    QuotaUsed: LONGLONG;
    QuotaThreshold: LONGLONG;
    QuotaLimit: LONGLONG;
  end;

  {$EXTERNALSYM DiskQuotaUserInformation}
  DISKQUOTA_USER_INFORMATION = DiskQuotaUserInformation;
  {$EXTERNALSYM DISKQUOTA_USER_INFORMATION}
  TDiskQuotaUserInformation = DISKQUOTA_USER_INFORMATION;
  PDiskQuotaUserInformation = PDISKQUOTA_USER_INFORMATION;
{$endif}

//
// IDiskQuotaUser represents a single user quota record on a particular
// NTFS volume.  Objects using this interface are instantiated
// through several IDiskQuotaControl methods.
//

{$ifdef DISK_QUOTAS_SUPPORTED}

type
  IDiskQuotaUser = interface (IUnknown)
  ['{7988B574-EC89-11cf-9C00-00AA00A14F56}']
    function GetID(var pulID: ULONG): HRESULT; stdcall;
    function GetName(pszAccountContainer: LPWSTR; cchAccountContainer: DWORD;
      pszLogonName: LPWSTR; cchLogonName: DWORD; pszDisplayName: LPWSTR;
      cchDisplayName: DWORD): HRESULT; stdcall;
    function GetSidLength(var pdwLength: DWORD): HRESULT; stdcall;
    function GetSid(pbSidBuffer: PBYTE; cbSidBuffer: DWORD): HRESULT; stdcall;
    function GetQuotaThreshold(var pllThreshold: LONGLONG): HRESULT; stdcall;
    function GetQuotaThresholdText(pszText: LPWSTR; cchText: DWORD): HRESULT; stdcall;
    function GetQuotaLimit(var pllLimit: LONGLONG): HRESULT; stdcall;
    function GetQuotaLimitText(pszText: LPWSTR; cchText: DWORD): HRESULT; stdcall;
    function GetQuotaUsed(var pllUsed: LONGLONG): HRESULT; stdcall;
    function GetQuotaUsedText(pszText: LPWSTR; cchText: DWORD): HRESULT; stdcall;
    function GetQuotaInformation(var pbQuotaInfo: TDiskQuotaUserInformation; cbQuotaInfo: DWORD): HRESULT; stdcall;
    function SetQuotaThreshold(llThreshold: LONGLONG; fWriteThrough: BOOL): HRESULT; stdcall;
    function SetQuotaLimit(llLimit: LONGLONG; fWriteThrough: BOOL): HRESULT; stdcall;
    function Invalidate: HRESULT; stdcall;
    function GetAccountStatus(var pdwStatus: DWORD): HRESULT; stdcall;
  end;
{$endif}

//
// IEnumDiskQuotaUsers represents an enumerator created by
// IDiskQuotaControl for the purpose of enumerating individual user quota
// records on a particular volume.  Each record is represented through
// the IDiskQuotaUser interface.
//

{$ifdef DISK_QUOTAS_SUPPORTED}
  IEnumDiskQuotaUsers = interface (IUnknown)
  ['{7988B577-EC89-11cf-9C00-00AA00A14F56}']
    function Next(cUsers: DWORD; var rgUsers: IDiskQuotaUser; pcUsersFetched: LPDWORD): HRESULT; stdcall;
    function Skip(cUsers: DWORD): HRESULT; stdcall;
    function Reset: HRESULT; stdcall;
    function Clone(out ppEnum: IEnumDiskQuotaUsers): HRESULT; stdcall;
  end;
{$endif}

//
// Class IDs
//
// {7988B571-EC89-11cf-9C00-00AA00A14F56}
const
  CLSID_DiskQuotaControl: TGUID = (
    D1:$7988b571; D2:$ec89; D3:$11cf; D4:($9c, $0, $0, $aa, $0, $a1, $4f, $56));

//
// Interface IDs
//
// {7988B572-EC89-11cf-9C00-00AA00A14F56}

  IID_IDiskQuotaControl: TGUID = (
    D1:$7988b572; D2:$ec89; D3:$11cf; D4:($9c, $0, $0, $aa, $0, $a1, $4f, $56));

//
// Values for fNameResolution argument to:
//
//      IDiskQuotaControl::AddUserSid
//      IDiskQuotaControl::AddUserName
//      IDiskQuotaControl::FindUserSid
//      IDiskQuotaControl::CreateEnumUsers
//

const
  DISKQUOTA_USERNAME_RESOLVE_NONE  = 0;
  DISKQUOTA_USERNAME_RESOLVE_SYNC  = 1;
  DISKQUOTA_USERNAME_RESOLVE_ASYNC = 2;

//
// IDiskQuotaControl represents a disk volume, providing query and
// control of that volume's quota information.
//
{$ifdef DISK_QUOTAS_SUPPORTED}
type
  IDiskQuotaControl = interface (IConnectionPointContainer)
  ['{7988B571-EC89-11cf-9C00-00AA00A14F56}']
    function Initialize(pszPath: LPCWSTR; bReadWrite: BOOL): HRESULT; stdcall;
    function SetQuotaState(dwState: DWORD): HRESULT; stdcall;
    function GetQuotaState(var pdwState: DWORD): HRESULT; stdcall;
    function SetQuotaLogFlags(dwFlags: DWORD): HRESULT; stdcall;
    function GetQuotaLogFlags(var pdwFlags: DWORD): HRESULT; stdcall;
    function SetDefaultQuotaThreshold(llThreshold: LONGLONG): HRESULT; stdcall;
    function GetDefaultQuotaThreshold(var pllThreshold: LONGLONG): HRESULT; stdcall;
    function GetDefaultQuotaThresholdText(pszText: LPWSTR; cchText: DWORD): HRESULT; stdcall;
    function SetDefaultQuotaLimit(llLimit: LONGLONG): HRESULT; stdcall;
    function GetDefaultQuotaLimit(var pllLimit: LONGLONG): HRESULT; stdcall;
    function GetDefaultQuotaLimitText(pszText: LPWSTR; cchText: DWORD): HRESULT; stdcall;
    function AddUserSid(pUserSid: PSID; fNameResolution: DWORD;
      out ppUser: IDiskQuotaUser): HRESULT; stdcall;
    function AddUserName(pszLogonName: LPCWSTR; fNameResolution: DWORD;
      out ppUser: IDiskQuotaUser): HRESULT; stdcall;
    function DeleteUser(pUser: IDiskQuotaUser): HRESULT; stdcall;
    function FindUserSid(pUserSid: PSID; fNameResolution: DWORD;
      out ppUser: IDiskQuotaUser): HRESULT; stdcall;
    function FindUserName(pszLogonName: LPCWSTR; out ppUser: IDiskQuotaUser): HRESULT; stdcall;
    function CreateEnumUsers(rgpUserSids: PSID; cpSids, fNameResolution: DWORD;
      out ppEnum: IEnumDiskQuotaUsers): HRESULT; stdcall;
    //function CreateUserBatch(out ppBatch: IDiskQuotaUserBatch): HRESULT; stdcall;
    function InvalidateSidNameCache: HRESULT; stdcall;
    function GiveUserNameResolutionPriority(pUser: IDiskQuotaUser): HRESULT; stdcall;
    function ShutdownNameResolution: HRESULT; stdcall;
  end;
{$endif}

var
  FormFolderDrive: TFormFolderDrive;

const
  FGuid = '{713CC6CE-B3E2-4fd9-838D-E28F558F6866}';

const
  FILE_WRITE_ATTRIBUTES     = $0100; // all

  FILE_ATTRIBUTE_REPARSE_POINT = $400;

  FILE_FLAG_OPEN_REPARSE_POINT = $00200000;

  FSCTL_SET_REPARSE_POINT    = $900A4;
  FSCTL_GET_REPARSE_POINT    = $900A8;
  FSCTL_DELETE_REPARSE_POINT = $900AC;

const
  QUOTA_INDEX_FILE = '\$Extend\$Quota:$Q:$INDEX_ALLOCATION';

const
  NTC_DIRECTORY_ENUM			= $7534;
  NTC_NAMED_STREAMS_ENUM	= $6209;
  NTC_FILE_LINKS_ENUM    = $1385;

type
  NODE_TYPE_CODE = DWORD;

  PNODE_TYPE_CODE = ^NODE_TYPE_CODE;

//
// CONTEXT_HEADER
//
// Header is used to mark the structures
//

type
  CONTEXT_HEADER = record
    NodeTypeCode: DWORD;
    NodeByteSize: DWORD;
end;

PCONTEXT_HEADER = ^CONTEXT_HEADER;


type
  NAMED_STREAM_CONTEXT = record
    Header: CONTEXT_HEADER;
    hFile: THandle;
    EnumCtx: Pointer;
  end;

 PNAMED_STREAM_CONTEXT = ^NAMED_STREAM_CONTEXT;

type
  HARD_LINK_CONTEXT = record
    Header: CONTEXT_HEADER;
    hFile: THandle;
  end;

 PHARD_LINK_CONTEXT = ^HARD_LINK_CONTEXT;

type
  ENUM_INFO = record
    Header: CONTEXT_HEADER;
    finfo: TSearchRec;
    hFile: THandle;
end;

PENUM_INFO = ^ENUM_INFO;

type
  FILE_CONTEXT = record
    OpenCount: Integer;
    hFile: THandle;
    ReadOnly: Boolean;
    QuotasIndexFile: Boolean;
  end;

  PFILE_CONTEXT = ^FILE_CONTEXT;

{$ifdef DISK_QUOTAS_SUPPORTED}
  DISK_QUOTAS_CONTEXT = record
    pDiskQuotaControl: IDiskQuotaControl;
    OleInitialized: Boolean;
  end;

  PDISK_QUOTAS_CONTEXT = ^DISK_QUOTAS_CONTEXT;
{$endif}

  FS_INFORMATION_CLASS = (
    FileFsVolumeInformation       = 1,
    FileFsLabelInformation,      // 2
    FileFsSizeInformation,       // 3
    FileFsDeviceInformation,     // 4
    FileFsAttributeInformation,  // 5
    FileFsControlInformation,    // 6
    FileFsFullSizeInformation,   // 7
    FileFsObjectIdInformation,   // 8
    FileFsDriverPathInformation, // 9
    FileFsMaximumInformation
  );

  FILE_FS_CONTROL_INFORMATION = record
    FreeSpaceStartFiltering: Int64;
    FreeSpaceThreshold: Int64;
    FreeSpaceStopFiltering: Int64;
    DefaultQuotaThreshold: Int64;
    DefaultQuotaLimit: Int64;
    FileSystemControlFlags: LongWord;
  end;

PFILE_FS_CONTROL_INFORMATION = ^FILE_FS_CONTROL_INFORMATION;

  ECBFSConnectCustomError = class (EcbcCBFS)
  protected
    FErrorCode : Integer;
  public
    constructor Create(const AMessage: string); overload;
    constructor Create(AErrorCode : Integer); overload;
    constructor Create(AErrorCode : Integer; const AMessage : string); overload;

    property ErrorCode : Integer read FErrorCode;
  end;

implementation

{$R *.dfm}

uses
  DateUtils;

var
  ZeroFiletime : TDateTime;

type
  PSearchRec = ^TSearchRec;

function ExceptionToErrorCode(E : Exception) : Integer;
begin
  if E is EOSError then
    Result := EOSError(E).ErrorCode
  else
    Result := ERROR_INTERNAL_ERROR;
end;

//
//Input:
//  Path - source file path relative to CBFS root path
//Output:
// returns TRUE - if Path is a part of QUOTA_INDEX_FILE path
// FileId set to NTFS disk quotas index file if Path equal to QUOTA_INDEX_FILE,
// otherwise FileId = 0
//
function TFormFolderDrive.IsQuotasIndexFile(Path: string; var FileId: Int64): Boolean;
begin
  Result := false;
  if (Length(Path) > 1) and (Path[2] = '$') then
  begin
    if Copy(QUOTA_INDEX_FILE, 1, Length(Path)) = Path then
    begin
      if Path = QUOTA_INDEX_FILE then
        FileId := GetFileId(FRootPath[1] + ':' + Path);

      Result := true;
    end;
  end;
end;

(*
function TFormFolderDrive.IsAdmin(): Boolean;
var
hAccessToken: THANDLE;
InfoBuffer: Array[ 0..1024 ] Of Byte ;
ptgGroups: PTokenGroups;
dwInfoBufferSize: DWORD;
psidAdministrators: PSID;
siaNtAuthority: SID_IDENTIFIER_AUTHORITY;
i:UINT;
bRet:Boolean;
begin
  Result := false;
  ZeroMemory(@(siaNtAuthority.Value), sizeof(siaNtAuthority.Value));
  siaNtAuthority.Value[5] := 5; {SECURITY_NT_AUTHORITY}
  if not OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, hAccessToken) then Exit;
  bRet := GetTokenInformation(hAccessToken, TokenGroups, Pointer(@InfoBuffer), 1024, dwInfoBufferSize);
  CloseHandle(hAccessToken);
  if not bRet then Exit;
  if not AllocateAndInitializeSid(siaNtAuthority,
    2,
    $20 {SECURITY_BUILTIN_DOMAIN_RID},
    $220{DOMAIN_ALIAS_RID_ADMINS},
    0, 0, 0, 0, 0, 0,
    psidAdministrators) then Exit;
  ptgGroups := PTokenGroups(@InfoBuffer);
  i := 0;
  while i < ptgGroups.GroupCount do
  begin
    if EqualSid(psidAdministrators, ptgGroups.Groups[i].Sid) then
      Result := (ptgGroups.Groups[i].Attributes and $04 {SE_GROUP_ENABLED}) <> 0;
    i := i + 1;
  end;
  FreeSid(psidAdministrators);
end;
*)

procedure TFormFolderDrive.IsEmptyDirectory(const DirectoryName: String;
  var IsEmpty: Boolean;
  var ResultCode: Integer);
var
  P: TSearchRec;
begin
  try
    IsEmpty := FindFirst(FRootPath + DirectoryName + '\*', faAnyFile, P) <> 0;
    while (P.Name = '.') or (P.Name = '..') do
    begin
      IsEmpty := FindNext(P) <> 0;
      if IsEmpty then
        Break;
    end;

    FindClose(P);
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBCanFileBeDeleted(
  Sender: TObject;
  const FileName: String;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var CanBeDeleted: Boolean;
  var ResultCode: Integer);
var
  Attrs: DWORD;
  Empty: boolean;
begin
  Attrs := GetFileAttributes(PChar(FRootPath + FileName));
  CanBeDeleted := True;
  if Pos(':', FileName) > 1 then
    exit; 
  if (Attrs <> INVALID_FILE_ATTRIBUTES) and ((Attrs and FILE_ATTRIBUTE_DIRECTORY) <> 0) and ((Attrs and FILE_ATTRIBUTE_REPARSE_POINT) = 0) then
  begin
    IsEmptyDirectory(FileName, Empty, ResultCode);
    if (ResultCode = 0) and (not Empty) then
    begin
      CanBeDeleted := false;
      ResultCode := ERROR_DIR_NOT_EMPTY;
    end;
  end;
end;

procedure TFormFolderDrive.CBCloseDirectoryEnumeration(
  Sender: TObject;
  const DirectoryName: String;
  var DirectoryContext: Pointer;
  EnumerationContext: Pointer;
  var ResultCode: Integer);
var
  pHeader: PCONTEXT_HEADER;
  pDirectoryEnumInfo: PENUM_INFO;
begin
  try
    if EnumerationContext <> nil then
    begin
      pHeader := PCONTEXT_HEADER(EnumerationContext);
      if NTC_DIRECTORY_ENUM = pHeader.NodeTypeCode then
      begin
        pDirectoryEnumInfo := PENUM_INFO(EnumerationContext);
        FindClose(pDirectoryEnumInfo.finfo);
        FreeMem(pDirectoryEnumInfo);
      end
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBCloseNamedStreamsEnumeration(
  Sender: TObject;
  const FileName: String;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  EnumerationContext: Pointer;
  var ResultCode: Integer);
var
  pHeader: PCONTEXT_HEADER;
  pStreamEnumInfo: PNAMED_STREAM_CONTEXT;
begin
  try
    if EnumerationContext <> nil then
    begin
      pHeader := PCONTEXT_HEADER(EnumerationContext);
      if NTC_NAMED_STREAMS_ENUM = pHeader.NodeTypeCode then
      begin
        pStreamEnumInfo := PNAMED_STREAM_CONTEXT(EnumerationContext);
        if pStreamEnumInfo.hFile <> INVALID_HANDLE_VALUE then
          Windows.FindClose(pStreamEnumInfo.hFile);
        FreeMem(pStreamEnumInfo);
      end;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBCloseHardLinksEnumeration(
  Sender: TObject;
  const FileName: String;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  EnumerationContext: Pointer;
  var ResultCode: Integer);
var
  pLinkEnumInfo: PHARD_LINK_CONTEXT;
begin
  try
    if EnumerationContext <> nil then
    begin
      pLinkEnumInfo := PHARD_LINK_CONTEXT(EnumerationContext);
      Windows.FindClose(pLinkEnumInfo.hFile);
      FreeMem(pLinkEnumInfo);
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBCloseFile(
  Sender: TObject;
  const FileName: String;
  PendingDeletion: Boolean;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  Ctx: PFILE_CONTEXT;
begin
  try
    if FileContext <> nil then
    begin
      Ctx := PFILE_CONTEXT(FileContext);
      if Ctx.OpenCount = 1 then
      begin
        if Ctx.hFile <> INVALID_HANDLE_VALUE then
        begin
          CloseHandle(Ctx.hFile);
          Ctx.hFile := INVALID_HANDLE_VALUE;
        end;

        FreeMem(Ctx);
      end
      else
        Dec(Ctx.OpenCount);
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBCreateFile(
  Sender: TObject;
  const FileName: String;
  DesiredAccess: Integer;
  FileAttributes: Integer;
  ShareMode: Integer;
  NTCreateDisposition: Integer;
  NTDesiredAccess: Integer;
  FileInfo: Int64;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  Ctx: PFILE_CONTEXT;
  FullName : string;
  IsDirectory : Boolean;
begin
  try
    Ctx := AllocMem(SizeOf(FILE_CONTEXT));
    if Ctx = nil then
      RaiseLastWin32Error;

    Ctx.ReadOnly := false;
    Ctx.hFile := INVALID_HANDLE_VALUE;
    FullName := FRootPath + FileName;
    IsDirectory := (FileAttributes and FILE_ATTRIBUTE_DIRECTORY) <> 0;
    if IsDirectory then
    begin
      Win32Check(CreateDirectory(PChar(FullName), nil));

      Ctx.hFile := CreateFile(PChar(FullName),
        GENERIC_READ or FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ or FILE_SHARE_WRITE or FILE_SHARE_DELETE,
        nil, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);

      if Ctx.hFile = INVALID_HANDLE_VALUE then
      begin
        FreeMem(Ctx);
        RaiseLastWin32Error;
      end;
    end
    else
    begin
      Ctx.hFile := CreateFile(PChar(FullName),
        GENERIC_READ or GENERIC_WRITE,
        FILE_SHARE_READ or FILE_SHARE_WRITE or FILE_SHARE_DELETE,
        nil, CREATE_NEW, FileAttributes, 0);

      if Ctx.hFile = INVALID_HANDLE_VALUE then
      begin
        ResultCode := GetLastError;
        FreeMem(Ctx);
        Exit;
      end;
    end;
    
    if Pos(':', FullName) <= 0 then
      Win32Check(SetFileAttributes(PChar(FullName), FileAttributes and $ffff)); // Attributes contains creation flags as well, which we need to strip

    Inc(Ctx.OpenCount);
    FileContext := Ctx;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

function IsWriteRightForAttrRequested(Acc: LongWord): boolean;
begin
  Result := ((Acc and (FILE_WRITE_DATA or FILE_APPEND_DATA or FILE_WRITE_EA)) = 0) and
    ((Acc and FILE_WRITE_ATTRIBUTES) <> 0);
end;

function IsReadRightForAttrOrDeleteRequested(Acc: LongWord): boolean;
begin
  Result := ((Acc and (FILE_READ_DATA or FILE_READ_EA)) = 0) and
    ((Acc and (FILE_READ_ATTRIBUTES or $10000 {Delete})) <> 0);
end;

procedure TFormFolderDrive.CBOpenFile(
  Sender: TObject;
  const FileName: String;
  DesiredAccess: Integer;
  FileAttributes: Integer;
  ShareMode: Integer;
  NTCreateDisposition: Integer;
  NTDesiredAccess: Integer;
  FileInfo: Int64;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  Ctx: PFILE_CONTEXT;
  FileId : Int64;
  Attr, Error : LongWord;
  IsDirectory, IsReparsePoint : Boolean;
  OpenAsReparsePoint : Boolean;

begin
  try
    if FileContext <> nil then
    begin
      Ctx := PFILE_CONTEXT(Pointer(FileContext));
      if Ctx.ReadOnly and (DesiredAccess and (FILE_WRITE_DATA or FILE_APPEND_DATA or FILE_WRITE_ATTRIBUTES or FILE_WRITE_EA) <> 0 ) then
      begin
        ResultCode := ERROR_ACCESS_DENIED;
        Exit;
      end;
      Inc(Ctx.OpenCount);
      Exit;
    end
    else
    begin
      Ctx := AllocMem(SizeOf(FILE_CONTEXT));
      if Ctx = nil then
        RaiseLastWin32Error;
    end;

    Ctx.ReadOnly := false;
    Ctx.hFile := INVALID_HANDLE_VALUE;
    if IsQuotasIndexFile(FileName, FileId) then
    begin
      //
      // at this point we emulate a
      // disk quotas index file path on
      // NTFS volumes (\\$Extend\\$Quota:$Q:$INDEX_ALLOCATION)
      //
      Ctx.QuotasIndexFile := true;
    end
    else
    begin
      Attr := 0;
      if Pos(':', FileName) <= 0 then
      begin
        Attr := GetFileAttributes(PChar(FRootPath + FileName));
        if Attr = $ffffffff then
        begin
          ResultCode := GetLastError;
          FreeMem(Ctx);
          Exit;
        end;
      end;

      IsDirectory := (Attr and FILE_ATTRIBUTE_DIRECTORY) = FILE_ATTRIBUTE_DIRECTORY;
      IsReparsePoint := (Attr and FILE_ATTRIBUTE_REPARSE_POINT) = FILE_ATTRIBUTE_REPARSE_POINT;
      OpenAsReparsePoint := ((FileAttributes and FILE_FLAG_OPEN_REPARSE_POINT) = FILE_FLAG_OPEN_REPARSE_POINT);

      if IsReparsePoint and OpenAsReparsePoint and TcbcCBFS(Sender).UseReparsePoints then
      begin
        Ctx.hFile := CreateFile(
          PChar(FRootPath + FileName),
          GENERIC_READ or FILE_WRITE_ATTRIBUTES,
          FILE_SHARE_READ or FILE_SHARE_WRITE or FILE_SHARE_DELETE,
          nil,
          OPEN_EXISTING,
          FILE_FLAG_BACKUP_SEMANTICS or FILE_FLAG_OPEN_REPARSE_POINT,
          0);

        if Ctx.hFile = INVALID_HANDLE_VALUE then
        begin
          ResultCode := GetLastError;
          FreeMem(Ctx);
          Exit;
        end;
      end
      else
      if not IsDirectory then
      begin
        Ctx.hFile := CreateFile(PChar(FRootPath + FileName),
          GENERIC_READ or GENERIC_WRITE,
          FILE_SHARE_READ or FILE_SHARE_WRITE,
          nil,
          OPEN_EXISTING,
          FILE_FLAG_OPEN_REPARSE_POINT,
          0);

        if Ctx.hFile = INVALID_HANDLE_VALUE then
        begin
          Error := GetLastError;
          if (Error = ERROR_ACCESS_DENIED) then // (DesiredAccess and (FILE_WRITE_DATA or FILE_APPEND_DATA or FILE_WRITE_ATTRIBUTES or FILE_WRITE_EA) = 0 ) then
          begin
            if not IsReadRightForAttrOrDeleteRequested(DesiredAccess) then
            begin
              if IsWriteRightForAttrRequested(DesiredAccess) or
                ((DesiredAccess and (FILE_WRITE_DATA or FILE_APPEND_DATA or FILE_WRITE_ATTRIBUTES or FILE_WRITE_EA)) <> 0) then
              begin
                Ctx.hFile := CreateFile(PChar(FRootPath + FileName),
                  GENERIC_READ,
                  FILE_SHARE_READ or FILE_SHARE_WRITE or FILE_SHARE_DELETE,
                  nil,
                  OPEN_EXISTING,
                  FILE_FLAG_OPEN_REPARSE_POINT,
                  0);
                Ctx.ReadOnly := true;

                if Ctx.hFile = INVALID_HANDLE_VALUE then
                begin
                  ResultCode := GetLastError;
                  FreeMem(Ctx);
                  Exit;
                end;
              end;
            end;
          end
          else
          begin
            ResultCode := Error;
            FreeMem(Ctx);
            Exit;
          end;
        end;
      end;
    end;
    
    if (NTCreateDisposition <> FILE_OPEN) and (NTCreateDisposition <> FILE_OPEN_IF) then
      Win32Check(SetFileAttributes(PChar(FRootPath + FileName), FileAttributes and $ffff)); // Attributes contains creation flags as well, which we need to strip

    Inc(Ctx.OpenCount);
    FileContext := Ctx;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBDeleteFile(
  Sender: TObject;
  const FileName: String;
  var FileContext: Pointer;
  var ResultCode: Integer);
var
  FullName: String;
begin
  try
    FullName := FRootPath + FileName;
    if (GetFileAttributes(PChar(FullName)) and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
      Win32Check(RemoveDirectory(PChar(FullName)))
    else
      Win32Check(DeleteFile(PChar(FullName)));
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBEnumerateDirectory(
  Sender: TObject;
  const DirectoryName: String;
  const Mask: String;
  CaseSensitive: Boolean;
  Restart: Boolean;
  RequestedInfo: Integer;
  var FileFound: Boolean;
  var FileName: String;
  var ShortFileName: String;
  var CreationTime: TDateTime;
  var LastAccessTime: TDateTime;
  var LastWriteTime: TDateTime;
  var ChangeTime: TDateTime;
  var Size: Int64;
  var AllocationSize: Int64;
  var FileId: Int64;
  var Attributes: Int64;
  var ReparseTag: Int64;
  HandleInfo: Int64;
  var DirectoryContext: Pointer;
  var HandleContext: Pointer;
  var EnumerationContext: Pointer;
  var ResultCode: Integer);
var
  P: PENUM_INFO;
  SysTime: TSystemTime;
  FullFileName: string;
begin
  try
    if Restart and (EnumerationContext <> nil) then
    begin
      FindClose(PENUM_INFO(EnumerationContext)^.finfo);
      FreeMem(Pointer(EnumerationContext));
      EnumerationContext := nil;
    end;

    if EnumerationContext = nil then
    begin
      EnumerationContext := AllocMem(SizeOf(ENUM_INFO));
      P := PENUM_INFO(EnumerationContext);
      ZeroMemory(P, SizeOf(ENUM_INFO));
      P.Header.NodeTypeCode := NTC_DIRECTORY_ENUM;
      FileFound := FindFirst(FRootPath + DirectoryName + '\' + Mask,
        faAnyFile, P.finfo) = 0;
    end
    else
    begin
      P := PENUM_INFO(EnumerationContext);
      FileFound := FindNext(P.finfo) = 0;
    end;

    while FileFound and ((P.finfo.Name = '.') or (P.finfo.Name = '..') ) do
      FileFound := FindNext(P.finfo) = 0;

    if FileFound then
    begin
      FileName := P.finfo.Name;
      FileTimeToSystemTime(P.finfo.FindData.ftCreationTime, SysTime);
      CreationTime := SystemTimeToDateTime(SysTime);
      FileTimeToSystemTime(P.finfo.FindData.ftLastAccessTime, SysTime);
      LastAccessTime := SystemTimeToDateTime(SysTime);
      FileTimeToSystemTime(P.finfo.FindData.ftLastWriteTime, SysTime);
      LastWriteTime := SystemTimeToDateTime(SysTime);
      Attributes := P.finfo.FindData.dwFileAttributes;
      if (Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
        Size := 0
      else
        Size := Int64(P.finfo.FindData.nFileSizeLow) or
          Int64(P.finfo.FindData.nFileSizeHigh) shl 32;

      AllocationSize := Size;
      FileId := 0;

      if (Attributes and FILE_ATTRIBUTE_REPARSE_POINT) <> 0 then
      begin
        if FCbFs.UseReparsePoints then
          ReparseTag := p.finfo.FindData.dwReserved0;
      end;

      //
      // If FileId is supported then get FileId for the file.
      //
      if Assigned(FCbFs.OnGetFileNameByFileId) then
      begin
        FullFileName := FRootPath + DirectoryName + '\' + p.finfo.Name;
        FileId := GetFileId(FullFileName);
      end;
    end
    else if EnumerationContext <> nil then
    begin
      FindClose(PENUM_INFO(EnumerationContext)^.finfo);
      FreeMem(Pointer(EnumerationContext));
      EnumerationContext := nil;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBReadFile(
  Sender: TObject;
  const FileName: String;
  Position: Int64;
  Buffer: Pointer;
  BytesToRead: Int64;
  var BytesRead: Int64;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  Overlapped: TOverlapped;
  Ctx: PFILE_CONTEXT;
  cBytesRead : LongWord;
begin
  try
    Ctx := PFILE_CONTEXT(FileContext);
    FillChar(Overlapped, SizeOf(Overlapped), 0);
    Overlapped.Offset := Position and $FFFFFFFF;
    Overlapped.OffsetHigh := Position shr 32;
    Win32Check(ReadFile(Ctx.hFile, Buffer^,
      BytesToRead, cBytesRead, @Overlapped));
    BytesRead := cBytesRead;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBWriteFile(
  Sender: TObject;
  const FileName: String;
  Position: Int64;
  Buffer: Pointer;
  BytesToWrite: Int64;
  var BytesWritten: Int64;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  Overlapped: TOverlapped;
  Ctx: PFILE_CONTEXT;
  cBytesWritten : LongWord;
begin
  try
    Ctx := PFILE_CONTEXT(FileContext);
    FillChar(Overlapped, SizeOf(Overlapped), 0);
    Overlapped.Offset := Position and $FFFFFFFF;
    Overlapped.OffsetHigh := Position shr 32;
    Win32Check(WriteFile(Ctx.hFile, Buffer^,
      BytesToWrite, cBytesWritten, @Overlapped));
    BytesWritten := cBytesWritten;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBGetFileInfo(
  Sender: TObject;
  const FileName: String;
  RequestedInfo: Integer;
  var FileExists: Boolean;
  var CreationTime: TDateTime;
  var LastAccessTime: TDateTime;
  var LastWriteTime: TDateTime;
  var ChangeTime: TDateTime;
  var Size: Int64;
  var AllocationSize: Int64;
  var FileId: Int64;
  var Attributes: Int64;
  var ReparseTag: Int64;
  var HardLinkCount: Integer;
  var ShortFileName: String;
  var RealFileName: String;
  var ResultCode: Integer);
var
  SysTime: TSystemTime;
  fi: BY_HANDLE_FILE_INFORMATION;
  hFile: THandle;
  FullName : string;
  IsADS : Boolean;
  ADSMarker : Integer;
  Error, Attr : LongWord;
begin
  try
    if IsQuotasIndexFile(FileName, FileId) then
    begin
      //
      // at this point we emulate a
      // disk quotas index file path on
      // NTFS volumes (\\$Extend\\$Quota:$Q:$INDEX_ALLOCATION)
      //
      CreationTime := Now;
      LastAccessTime := CreationTime;
      LastWriteTime := CreationTime;
      ChangeTime := CreationTime;
      FileExists := true;
      Exit;
    end;

    FullName := FRootPath + FileName;

    IsADS := false;
    ADSMarker := LastDelimiter(':', FileName);
    //
    // at this point we check for ADS of a file
    // File.GetAttributes throws an exception for ADS name
    // so we must do somethis like this:
    // "somefile.dat:adsStream"   ->  "somefile.dat"
    // also verifying next situations:
    // "somefile.dat:"
    // ":adsStream"
    //
    if (ADSMarker > 1) and (ADSMarker <> Length(FileName) - 1) then
    begin
      IsADS := true;

      hFile := CreateFile(PChar(FullName),
            READ_CONTROL,
            0,
            nil,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS or FILE_FLAG_OPEN_REPARSE_POINT,
            0);

      if hFile = INVALID_HANDLE_VALUE then
      begin
        Error := GetLastError;
        if Error <> ERROR_FILE_NOT_FOUND then
        begin
          ResultCode := Error;
          FileExists := true;
        end
        else
          FileExists := false;

        Exit;
      end;

      if GetFileInformationByHandle(hFile, fi) then
      begin
        Size := Int64(fi.nFileSizeLow) or
          (Int64(fi.nFileSizeHigh) shl 32);
      end;

      CloseHandle(hFile);

      FullName := FRootPath + Copy(FileName, 1, adsMarker - 1);
    end;

    Attr := GetFileAttributes(PChar(FullName));
    if Attr = $ffffffff then
    begin
      Error := GetLastError;
      if Error <> ERROR_FILE_NOT_FOUND then
      begin
        ResultCode := Error;
        FileExists := true;
      end
      else
        FileExists := false;

      Exit;
    end;

    FileExists := true;
    Attributes := Attr;

    hFile := CreateFile(PChar(FullName),
          READ_CONTROL,
          0,
          nil,
          OPEN_EXISTING,
          FILE_FLAG_BACKUP_SEMANTICS or FILE_FLAG_OPEN_REPARSE_POINT,
          0);

    if hFile <> INVALID_HANDLE_VALUE then
    begin
      if GetFileInformationByHandle(hFile, fi) then
      begin
        FileTimeToSystemTime(fi.ftCreationTime, SysTime);
        CreationTime := SystemTimeToDateTime(SysTime);
        FileTimeToSystemTime(fi.ftLastAccessTime, SysTime);
        LastAccessTime := SystemTimeToDateTime(SysTime);
        FileTimeToSystemTime(fi.ftLastWriteTime, SysTime);
        LastWriteTime := SystemTimeToDateTime(SysTime);

        if not IsADS then
        begin
          if (Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
            Size := 0
          else
            Size := Int64(fi.nFileSizeLow) or
              (Int64(fi.nFileSizeHigh) shl 32);
        end;

        AllocationSize := Size;
        FileId := 0;
        HardLinkCount := 0;

        if Assigned(FCbFs.OnGetFileNameByFileId) and not IsADS then
        begin
          FileId := ((Int64(fi.nFileIndexHigh) shl 32) and $FFFFFFFF00000000) or Int64(fi.nFileIndexLow);

          if FCbFS.UseHardLinks then
            HardLinkCount := fi.nNumberOfLinks;
        end;
      end
      else
        ResultCode := GetLastError;

      CloseHandle(hFile);
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBGetVolumeId(Sender: TObject;
  var VolumeId: Integer; var ResultCode: Integer);
begin
  VolumeID := $12345678;
end;

procedure TFormFolderDrive.CBGetVolumeLabel(Sender: TObject;
  var VolumeLabel: String; var ResultCode: Integer);
begin
  VolumeLabel := 'CbFs Test';
end;

procedure TFormFolderDrive.CBGetVolumeSize(
  Sender: TObject;
  var TotalSectors: Int64;
  var AvailableSectors: Int64;
  var ResultCode: Integer);
var
  {$ifdef DISK_QUOTAS_SUPPORTED}
  dqui: TDiskQuotaUserInformation;
  RemainingQuota: UInt64;
  pUser: IDiskQuotaUser;
  pDiskQuotaControl: IDiskQuotaControl;
  tu: PTokenUser;
  DiskPath : WideString;
  QuotaState : LongWord;
  {$endif}
  FreeBytesToCaller, TotalBytes: Int64;
  FreeBytes: Int64;
  userToken: THandle;
  Res: Boolean;
  ReturnedLength: LongWord;
  oleInitialized: Boolean;
begin
  try
    Win32Check(GetDiskFreeSpaceEx(PChar(FRootPath), FreeBytesToCaller,
      TotalBytes, PLargeInteger(@FreeBytes)));

    {$ifndef DISK_QUOTAS_SUPPORTED}
    TotalSectors := TotalBytes div SECTOR_SIZE;
    AvailableSectors := FreeBytes div SECTOR_SIZE;
    {$else}
    if not FCbFs.UseDiskQuotas then
    begin
      TotalSectors := TotalBytes div SECTOR_SIZE;
      AvailableSectors := FreeBytes div SECTOR_SIZE;
      Exit;
    end;

    dqui.QuotaUsed := 0;
    dqui.QuotaThreshold := -1; // unlimited
    dqui.QuotaLimit := -1; // unlimited

    userToken := THandle(FCbFs.GetOriginatorToken);
    GetTokenInformation(userToken,
      TokenUser, nil, 0, ReturnedLength);
    tu := PTokenUser(AllocMem(ReturnedLength));
    Res := GetTokenInformation(userToken,
      TokenUser, tu, ReturnedLength, ReturnedLength);
    CloseHandle(userToken);
    if not Res then
    begin
      FreeMemory(tu);
      RaiseLastWin32Error;
    end;
    //
    // get the user disk quota limits
    //
    OleInitialized := false;
    try
      oleInitialize(nil);
      oleInitialized := true;
      OleCheck(CoCreateInstance(CLSID_DiskQuotaControl, nil, CLSCTX_INPROC_SERVER,
        IID_IDiskQuotaControl, pDiskQuotaControl));
      DiskPath := FRootPath[1] +':\';
      OleCheck(pDiskQuotaControl.Initialize(PWideChar(DiskPath), true));
      OleCheck(pDiskQuotaControl.GetQuotaState(QuotaState));
      if QuotaState = 2 {dqStateEnforce} then
      begin
        OleCheck(pDiskQuotaControl.FindUserSid(tu.User.Sid,
          DISKQUOTA_USERNAME_RESOLVE_SYNC, pUser));
        OleCheck(pUser.GetQuotaInformation(dqui, SizeOf(TDiskQuotaUserInformation)));
      end;
    finally
      begin
        pUser := nil;
        pDiskQuotaControl := nil;
        if oleInitialized then OleUninitialize;
        FreeMemory(tu);
      end;
    end;

    if QuotaState = 2 {dqStateEnforce} then
    begin
      if UInt64(dqui.QuotaLimit) < UInt64(TotalBytes) then
        TotalSectors := dqui.QuotaLimit div SECTOR_SIZE
      else
        TotalSectors := TotalBytes div SECTOR_SIZE;

      if UInt64(dqui.QuotaLimit) <= UInt64(dqui.QuotaUsed) then
        RemainingQuota := 0
      else
        RemainingQuota := UInt64(dqui.QuotaLimit) - UInt64(dqui.QuotaUsed);

      if RemainingQuota < UInt64(FreeBytes) then
        AvailableSectors := RemainingQuota div SECTOR_SIZE
      else
        AvailableSectors := FreeBytes div SECTOR_SIZE;
    end
    else
    begin
      TotalSectors := TotalBytes div SECTOR_SIZE;
      AvailableSectors := FreeBytes div SECTOR_SIZE;
    end
    {$endif}
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBMount(Sender: TObject; var ResultCode: Integer);
begin
;
end;

procedure TFormFolderDrive.CBRenameOrMoveFile(
  Sender: TObject;
  const FileName: String;
  const NewFileName: String;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
begin
  try
    Win32Check(MoveFileEx(PChar(FRootPath + FileName),
      PChar(FRootPath + NewFileName), MOVEFILE_REPLACE_EXISTING));
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBCreateHardLink(
  Sender: TObject;
  const FileName: String;
  const LinkName: String;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
begin
  try
    Win32Check(CreateHardLink(PChar(FRootPath + LinkName),
      PChar(FRootPath + FileName), nil));
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBSetFileAttributes(
  Sender: TObject;
  const FileName: String;
  CreationTime: TDateTime;
  LastAccessTime: TDateTime;
  LastWriteTime: TDateTime;
  ChangeTime: TDateTime;
  FileAttributes: Integer;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  SysTime: TSystemTime;
  Creation, LastAccess, LastWrite: TFileTime;
  CreationPtr, LastAccessPtr, LastWritePtr: PFileTime;
  Ctx: PFILE_CONTEXT;
  hFile, Attr: LongWord;
begin
  try
    try
      Ctx := PFILE_CONTEXT(FileContext);
      hFile := INVALID_HANDLE_VALUE;

      // the case when FileAttributes == 0 indicates that file attributes
      // not changed during this callback
      if(FileAttributes <> 0) then
        Win32Check(SetFileAttributes(PChar(FRootPath + FileName),
          FileAttributes));

      Attr := GetFileAttributes(PChar(FRootPath + FileName));
      if Attr = INVALID_FILE_ATTRIBUTES then
      begin
        ResultCode := GetLastError;
        Exit;
      end;

      if (Attr and FILE_ATTRIBUTE_READONLY) <> 0 then
      begin
        Win32Check(SetFileAttributes(PChar(FRootPath + FileName), Attr and (not FILE_ATTRIBUTE_READONLY)));
      end;

      hFile := CreateFile(PChar(FRootPath + FileName), GENERIC_READ or GENERIC_WRITE,
        FILE_SHARE_READ or FILE_SHARE_WRITE or FILE_SHARE_DELETE, nil,
        OPEN_EXISTING, 0, 0);
      if hFile = INVALID_HANDLE_VALUE then
      begin
        ResultCode := GetLastError;
        Exit;
      end;

      if CreationTime <> ZeroFiletime then
      begin
        DateTimeToSystemTime(CreationTime, SysTime);
        SystemTimeToFileTime(SysTime, Creation);
        CreationPtr := @Creation;
      end
      else
        CreationPtr := nil;

      if LastAccessTime <> ZeroFiletime then
      begin
        DateTimeToSystemTime(LastAccessTime, SysTime);
        SystemTimeToFileTime(SysTime, LastAccess);
        LastAccessPtr := @LastAccess;
      end
      else
        LastAccessPtr := nil;

      if LastWriteTime <> ZeroFiletime then
      begin
        DateTimeToSystemTime(LastWriteTime, SysTime);
        SystemTimeToFileTime(SysTime, LastWrite);
        LastWritePtr := @LastWrite;
      end
      else
        LastWritePtr := nil;

      if (CreationPtr <> nil) or (LastAccessPtr <> nil) or (LastWritePtr <> nil) then
        Win32Check(SetFileTime(Ctx.hFile, CreationPtr,
          LastAccessPtr, LastWritePtr));

      if (Attr and FILE_ATTRIBUTE_READONLY) <> 0 then
      begin
        Win32Check(SetFileAttributes(PChar(FRootPath + FileName), Attr));
      end;
    except
      on E : Exception do
        ResultCode := ExceptionToErrorCode(E);
    end;
  finally
    if hFile <> INVALID_HANDLE_VALUE then
      CloseHandle(hFile);
  end;
end;

procedure TFormFolderDrive.CBSetAllocationSize(
  Sender: TObject;
  const FileName: String;
  AllocationSize: Int64;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
begin
//NOTHING TO DO
end;

procedure TFormFolderDrive.CBSetFileSize(
  Sender: TObject;
  const FileName: String;
  Size: Int64;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  Error: LongWord;
  OffsetHigh: LongInt;
  Ctx: PFILE_CONTEXT;
begin
  try
    Ctx := PFILE_CONTEXT(FileContext);
    OffsetHigh := Size shr 32;
    if SetFilePointer(Ctx.hFile, Size and $FFFFFFFF,
      @OffsetHigh, FILE_BEGIN) = $FFFFFFFF then
      begin
        Error := GetLastError;
        if Error <> ERROR_SUCCESS then
          begin
            SetLastError(Error);
            RaiseLastWin32Error;
          end;
      end;
    Win32Check(SetEndOfFile(Ctx.hFile));
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBIsDirectoryEmpty(
  Sender: TObject;
  const DirectoryName: String;
  var IsEmpty: Boolean;
  var DirectoryContext: Pointer;
  var ResultCode: Integer);
begin
  IsEmptyDirectory(DirectoryName, IsEmpty, ResultCode);
end;

procedure TFormFolderDrive.CBEnumerateNamedStreams(
  Sender: TObject;
  const FileName: String;
  var NamedStreamFound: Boolean;
  var StreamName: String;
  var StreamSize: Int64;
  var StreamAllocationSize: Int64;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var EnumerationContext: Pointer;
  var ResultCode: Integer);
var
  fsd: WIN32_FIND_STREAM_DATA;
  nsc: PNAMED_STREAM_CONTEXT;
  SearchResult : boolean;
  // WStreamName: array[0..MAX_PATH] of WideChar;

begin
  try
    NamedStreamFound := false;
    while (not NamedStreamFound) do
    begin
      if EnumerationContext = nil then
      begin
        nsc := AllocMem(SizeOf(NAMED_STREAM_CONTEXT));
        ZeroMemory(nsc, SizeOf(NAMED_STREAM_CONTEXT));
        nsc.Header.NodeTypeCode := NTC_NAMED_STREAMS_ENUM;
        nsc.hFile := FindFirstStream(PWideChar(FRootPath + FileName), 0, fsd, 0);
        SearchResult := nsc.hFile <> INVALID_HANDLE_VALUE;

        if SearchResult then
          EnumerationContext := nsc;
      end
      else
      begin
        nsc := PNAMED_STREAM_CONTEXT(EnumerationContext);
        SearchResult := (nsc <> nil) and (nsc.hFile <> INVALID_HANDLE_VALUE) and FindNextStream(nsc.hFile, &fsd);
      end;

      if not SearchResult then
      begin
        ResultCode := GetLastError();
        if (ResultCode = ERROR_HANDLE_EOF) then
          ResultCode := 0
        else
        begin
          if (nsc.hFile <> INVALID_HANDLE_VALUE) then
            Windows.FindClose(nsc.hFile);
          FreeMem(nsc, sizeof(NAMED_STREAM_CONTEXT));
        end;
		
		Exit;
      end
      else
      begin
        StreamName := StrPas(PWideChar(@fsd.cStreamName[0]));
        if StreamName = '::$DATA' then
          continue;
        NamedStreamFound := true;
        StreamSize := fsd.StreamSize;
        StreamAllocationSize := fsd.StreamSize;
      end;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBEnumerateHardLinks(
  Sender: TObject;
  const FileName: String;
  var LinkFound: Boolean;
  var LinkName: String;
  var ParentId: Int64;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var EnumerationContext: Pointer;
  var ResultCode: Integer);
var
  AFileName: array[0..MAX_PATH] of Char;
  flc: PHARD_LINK_CONTEXT;
  AFileNameLength: LongWord;
  I: Cardinal;
  Error: LongWord;
begin
  try
    AFileNameLength := MAX_PATH;
    while true do
    begin
      LinkFound := false;
      if EnumerationContext = nil then
      begin
        flc := AllocMem(SizeOf(HARD_LINK_CONTEXT));
        ZeroMemory(flc, SizeOf(HARD_LINK_CONTEXT));
        flc.Header.NodeTypeCode := NTC_FILE_LINKS_ENUM;

        if not Assigned(FindFirstFileName) then
        begin
          Error := GetLastError;
          SetLastError(Error);
          RaiseLastWin32Error;
        end;

        flc.hFile := FindFirstFileName(PChar(FRootPath + FileName),
          0, AFileNameLength, AFileName);
        if flc.hFile = INVALID_HANDLE_VALUE then
        begin
          Error := GetLastError;
          FreeMem(flc,sizeof(HARD_LINK_CONTEXT));
          SetLastError(Error);
          RaiseLastWin32Error;
        end;

        EnumerationContext := flc;
        LinkFound := true;
      end
      else
      begin
        flc := PHARD_LINK_CONTEXT(EnumerationContext);
        if not Assigned(FindNextFileName) then
        begin
          RaiseLastWin32Error;
        end;

        LinkFound := FindNextFileName(flc.hFile, AFileNameLength, AFileName);
      end;
      if not LinkFound then
      begin
        if GetLastError <> ERROR_HANDLE_EOF then
        begin
          Error := GetLastError;
          FreeMem(flc,sizeof(HARD_LINK_CONTEXT));
          SetLastError(Error);
          RaiseLastWin32Error;
        end
        else
          break;
      end;
      if LinkFound then
      begin
        //
        // The link name returned from FindFirstFileNameW and FindNextFileNameW can
        // contain path without drive letter.
        //
        if AFileName[0] = '\' then
        begin
          for I := 3 to Length(FRootPath)-1 do
            if FRootPath[I] = '\' then break;
          Move(AFileName, AFileName[I-1], (Length(AFileName)+1)*SizeOf(Char));
          Move(PChar(FRootPath)^, AFileName, (I-1)*SizeOf(Char));
        end;
        //
        // Report only file names (hard link names) that are only
        // inside the mapped directory.
        //
        if AnsiCompareText(AnsiLeftStr(AFileName, Length(FRootPath)),FRootPath) <> 0 then
          continue;
        if ExtractFileDir(AFileName) = FRootPath then
          ParentId := $7FFFFFFFFFFFFFFF
        else
          ParentId := GetFileId(ExtractFilePath(AFileName));
        LinkName := ExtractFileName(AFileName);
        break;
      end;
    end;

    if not LinkFound then
    begin
      Windows.FindClose(flc.hFile);
      FreeMem(flc,sizeof(HARD_LINK_CONTEXT));
      EnumerationContext := nil;
    end;

  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBSetFileSecurity(
  Sender: TObject;
  const FileName: String;
  SecurityInformation: Integer;
  SecurityDescriptor: Pointer;
  Length: Integer;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
begin
  //
  // Disable setting of new security for the backend file because
  // these new security attributes can prohibit manipulation
  // with the file from the callbacks (for example if read/write
  // is not allowed for this process).
  //
  if not SetFileSecurity( PChar(FRootPath + FileName), SecurityInformation, SecurityDescriptor) then
    ResultCode := GetLastError;
end;

procedure TFormFolderDrive.CBGetFileSecurity(
  Sender: TObject;
  const FileName: String;
  SecurityInformation: Integer;
  SecurityDescriptor: Pointer;
  BufferLength: Integer;
  var DescriptorLength: Integer;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  SACL_SECURITY_INFORMATION: LongWord;
  UpcasePath, PathOnVolumeC: string;
  Error, CLengthNeeded: LongWord;
begin
  try
    //
    // Getting SACL_SECURITY_INFORMATION requires the program to
    // execute elevated as well as the SE_SECURITY_NAME privilege
    // to be set. So just remove it from the request.
    //
    SACL_SECURITY_INFORMATION := $00000008;
    SecurityInformation := SecurityInformation and not SACL_SECURITY_INFORMATION;
    if SecurityInformation = 0 then exit;
    //
    // For recycle bin Windows expects some specific attributes
    // (if they aren't set then it's reported that the recycle
    // bin is corrupted). So just get attributes from the recycle
    // bin files located on the volume "C:".
    //

    CLengthNeeded := DescriptorLength;
    UpcasePath := UpperCase(FileName);
    if (Pos('$RECYCLE.BIN', UpcasePath) <> 0) or
       (Pos('RECYCLER', UpcasePath) <> 0) then begin
      PathOnVolumeC := 'C:' + FileName;
      if not GetFileSecurity(PChar(PathOnVolumeC),
        SecurityInformation, SecurityDescriptor, BufferLength, CLengthNeeded) then
      begin
        //
        // If the SecurityDescriptor buffer is smaller than required then
        // GetFileSecurity itself sets in the LengthNeeded parameter the required
        // size and the last error is set to ERROR_INSUFFICIENT_BUFFER.
        //
        DescriptorLength := CLengthNeeded;
        Error := GetLastError;
        ResultCode := Error;
        exit;
      end;
    end
    else
    if not GetFileSecurity(PChar(FRootPath + FileName),
      SecurityInformation, SecurityDescriptor, BufferLength, CLengthNeeded) then
    begin
     DescriptorLength := CLengthNeeded;
     Error := GetLastError;
     ResultCode := Error;
     exit;
    end;

    DescriptorLength := CLengthNeeded;
    //if DescriptorLength = 0 then
    //  DescriptorLength := GetSecurityDescriptorLength(SecurityDescriptor);
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

function NtOpenFile(FileHandle: PHandle; DesiredAccess: ACCESS_MASK;
  ObjectAttributes: POBJECT_ATTRIBUTES; IoStatusBlock: PIO_STATUS_BLOCK;
  ShareAccess: ULONG; OpenOptions: ULONG): Integer; stdcall; external 'ntdll.dll';

function LsaNtStatusToWinError(NtStatus: ULONG): ULONG; stdcall; external 'advapi32.dll';

procedure TFormFolderDrive.CBGetFileNameByFileId(
  Sender: TObject;
  FileId: Int64;
  var FilePath: String;
  var ResultCode: Integer);
var
  ObjAttributes: POBJECT_ATTRIBUTES;
  Name: SYSTEM_STRING;
  Iosb: IO_STATUS_BLOCK;
  RootHandle, FileHandle: THandle;
  Error, NtError, RootPathLength: LongWord;
  NameInfo: PFILE_NAME_INFO;
  b: Boolean;
  dllHandle: Cardinal;
  GetFileInformationByHandleEx: TGetFileInfoFunc;
begin
  try
    FileHandle := 0;
    RootPathLength := Length(FRootPath);
    RootHandle := THandle(CreateFile( PChar(FRootPath),
      0,
      FILE_SHARE_WRITE or FILE_SHARE_READ,
      nil,
      OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS,
      0 ));

    Name.Length := 8;
    Name.MaximumLength := 8;
    Name.Buffer := @FileId;
    objAttributes := POBJECT_ATTRIBUTES(GetMemory(SizeOf(OBJECT_ATTRIBUTES)));
    objAttributes^.Length := SizeOf(OBJECT_ATTRIBUTES);
    objAttributes^.ObjectName := @Name;
    objAttributes^.RootDirectory := RootHandle;
    objAttributes^.Attributes := 0;
    objAttributes^.SecurityDescriptor := nil;
    objAttributes^.SecurityQualityOfService := nil;
    NtError := NtOpenFile(@FileHandle,
      GENERIC_READ,
      objAttributes,
      @Iosb,
      FILE_SHARE_READ or FILE_SHARE_WRITE,
      $2000 or $4000 //FILE_OPEN_BY_FILE_ID | FILE_OPEN_FOR_BACKUP_INTENT
      );
    Error := LsaNtStatusToWinError(NtError);
    CloseHandle(RootHandle);
    FreeMemory(objAttributes);
    SetLastError(Error);
    if Error <> ERROR_SUCCESS then
    begin
      ResultCode := Error;
      exit;
    end;
    dllHandle := LoadLibrary('kernel32.dll');
    if dllHandle = 0 then RaiseLastWin32Error;
    @GetFileInformationByHandleEx := GetProcAddress(dllHandle, 'GetFileInformationByHandleEx');
    if not Assigned(GetFileInformationByHandleEx) then
    begin
      FreeLibrary(dllHandle);
      ResultCode := Error;
      exit;
    end;
    NameInfo := PFILE_NAME_INFO(GetMemory(SizeOf(FILE_NAME_INFO)));
    b := GetFileInformationByHandleEx(FileHandle, 2{FileNameInfo}, NameInfo, SizeOf(FILE_NAME_INFO));
    Error := GetLastError;
    CloseHandle(FileHandle);
    FreeLibrary(dllHandle);
    if not b then
    begin
      FreeMemory(NameInfo);
      ResultCode := Error;
      exit;
    end;

    //FileNameLength := NameInfo.FileNameLength div SizeOf(WCHAR) - (RootPathLength-Length('X:'));
    FilePath := Copy(NameInfo.FileName, RootPathLength - Cardinal(Length('X:')), MaxInt);
    FreeMemory(NameInfo);
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

{$ifdef DISK_QUOTAS_SUPPORTED}
function TFormFolderDrive.AllocateQuotaContext(Sid: PSID): Pointer;
var
  pDiskQuotaControl: IDiskQuotaControl;
  ctx: PDISK_QUOTAS_CONTEXT;
  DiskPath : WideString;
begin
  Result := nil;
  ctx := AllocMem(SizeOf(DISK_QUOTAS_CONTEXT));
  ZeroMemory(ctx, SizeOf(DISK_QUOTAS_CONTEXT));
  try
    OleCheck(OleInitialize(nil));
    ctx.OleInitialized := true;
    OleCheck(CoCreateInstance(CLSID_DiskQuotaControl, nil, CLSCTX_INPROC_SERVER,
      IID_IDiskQuotaControl, pDiskQuotaControl));
    DiskPath := FRootPath[1] +':\';
    OleCheck(pDiskQuotaControl.Initialize(PWideChar(DiskPath), true));
    ctx.pDiskQuotaControl := pDiskQuotaControl;
    Result := ctx;
  except
    on E:EOleSysError do
    begin
      pDiskQuotaControl := nil;
      if ctx.OleInitialized then
        OleUninitialize;

      FreeMem(ctx, SizeOf(DISK_QUOTAS_CONTEXT));
    end;
  end;
end;

procedure TFormFolderDrive.FreeQuotaContext(Context: Pointer);
var
  Ctx: PDISK_QUOTAS_CONTEXT;
begin
  if not Assigned(Context) then
    Exit;

  Ctx := PDISK_QUOTAS_CONTEXT(Context);
  if Assigned(Ctx.pDiskQuotaControl) then
    Ctx.pDiskQuotaControl := nil;

  if Ctx.OleInitialized then
    OleUninitialize;

  FreeMem(Context);
end;

procedure TFormFolderDrive.CBQueryQuotas(
  Sender: TObject;
  SID: Pointer;
  SIDLength: Integer;
  Index: Integer;
  var QuotaFound: Boolean;
  var QuotaUsed: Int64;
  var QuotaThreshold: Int64;
  var QuotaLimit: Int64;
  SIDOut: Pointer;
  SIDOutLength: Integer;
  var EnumerationContext: Pointer;
  var ResultCode: Integer);
var
  pEnumDiskQuotaUsers: IEnumDiskQuotaUsers;
  pUser: IDiskQuotaUser;
  Context: PDISK_QUOTAS_CONTEXT;
  DiskQuotaInformation : DiskQuotaUserInformation;
begin
  try
    if not Assigned(EnumerationContext) then
    begin
      Context := AllocateQuotaContext(Sid);
      EnumerationContext := Context;
    end
    else
      Context := PDISK_QUOTAS_CONTEXT(EnumerationContext);

    if Assigned(Sid) then
    begin
      OleCheck(Context.pDiskQuotaControl.FindUserSid( Sid,
        DISKQUOTA_USERNAME_RESOLVE_SYNC, pUser));

      OleCheck(pUser.GetQuotaInformation(DiskQuotaInformation, SizeOf(DiskQuotaInformation)));
    end
    else // Sid is null
    begin
      OleCheck(Context.pDiskQuotaControl.CreateEnumUsers (nil,
        0, DISKQUOTA_USERNAME_RESOLVE_SYNC, pEnumDiskQuotaUsers));
      OleCheck(pEnumDiskQuotaUsers.Skip(Index));
      OleCheck(pEnumDiskQuotaUsers.Next(1, pUser, nil));
      if not Assigned(pUser) then
      begin
        pEnumDiskQuotaUsers := nil;
        Exit;
      end;

      OleCheck(pUser.GetQuotaInformation(DiskQuotaInformation, sizeof(DiskQuotaInformation)));
      OleCheck(pUser.GetSid(PBYTE(SIDOut), SIDOutLength));
    end;

    QuotaFound := true;
    QuotaUsed := DiskQuotaInformation.QuotaUsed;
    QuotaThreshold := DiskQuotaInformation.QuotaThreshold;
    QuotaLimit := DiskQuotaInformation.QuotaLimit;
  except
    on E: EOleSysError do
      ResultCode := HResultCode(E.ErrorCode);

    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;

  pUser := nil;
  pEnumDiskQuotaUsers := nil;
end;

procedure TFormFolderDrive.CBSetQuotas(
  Sender: TObject;
  SID: Pointer;
  SIDLength: Integer;
  RemoveQuota: Boolean;
  var QuotaFound: Boolean;
  QuotaUsed: Int64;
  QuotaThreshold: Int64;
  QuotaLimit: Int64;
  var EnumerationContext: Pointer;
  var ResultCode: Integer);
var
  //pEnumDiskQuotaUsers: IEnumDiskQuotaUsers;
  pUser: IDiskQuotaUser;
  Context: PDISK_QUOTAS_CONTEXT;
begin
  try
    if not Assigned(EnumerationContext) then
    begin
      Context := AllocateQuotaContext(Sid);
      EnumerationContext := Context;
    end
    else
      Context := PDISK_QUOTAS_CONTEXT(EnumerationContext);

    OleCheck(Context.pDiskQuotaControl.FindUserSid( Sid,
      DISKQUOTA_USERNAME_RESOLVE_SYNC, pUser));

    if RemoveQuota then
      OleCheck(Context.pDiskQuotaControl.DeleteUser(pUser))
    else
    begin
      OleCheck(pUser.SetQuotaThreshold(QuotaThreshold, true));
      OleCheck(pUser.SetQuotaLimit(QuotaLimit, true));
    end;

    QuotaFound := true;
  except
    on E: EOleSysError do
      ResultCode := HResultCode(E.ErrorCode);

    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;

  pUser := nil;
end;

procedure TFormFolderDrive.CBCloseQuotasEnumeration(
  Sender: TObject;
  EnumerationContext: Pointer;
  var ResultCode: Integer);
var
  pDiskQuotasEnumInfo: PDISK_QUOTAS_CONTEXT;
begin
  try
    if Assigned(EnumerationContext) then
    begin
      pDiskQuotasEnumInfo := PDISK_QUOTAS_CONTEXT(EnumerationContext);
      FreeQuotaContext(pDiskQuotasEnumInfo);
    end;
  except
    on E: EOleSysError do
      ResultCode := HResultCode(E.ErrorCode);

    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;
procedure TFormFolderDrive.CBGetDefaultQuotaInfo(
  Sender: TObject;
  var DefaultQuotaThreshold: Int64;
  var DefaultQuotaLimit: Int64;
  var FileSystemControlFlags: Int64;
  var ResultCode: Integer);
var
  pDiskQuotaControl: IDiskQuotaControl;
  DiskPath: WideString;
  QuotaState : LongWord;
  LogFlags: LongWord;
begin
  try
    oleInitialize(nil);
    OleCheck(CoCreateInstance(CLSID_DiskQuotaControl, nil, CLSCTX_INPROC_SERVER,
      IID_IDiskQuotaControl, pDiskQuotaControl));
    DiskPath := FRootPath[1] +':\';
    OleCheck(pDiskQuotaControl.Initialize(PWideChar(DiskPath), true));
    OleCheck(pDiskQuotaControl.GetQuotaState(QuotaState));
    OleCheck(pDiskQuotaControl.GetQuotaLogFlags(LogFlags));
    FileSystemControlFlags := QuotaState or (LogFlags shl 4);
    OleCheck(pDiskQuotaControl.GetDefaultQuotaThreshold(DefaultQuotaThreshold));
    OleCheck(pDiskQuotaControl.GetDefaultQuotaLimit(DefaultQuotaLimit));
  finally
    begin
      pDiskQuotaControl := nil;
      OleUninitialize;
    end;
  end;
end;

procedure TFormFolderDrive.CBSetDefaultQuotaInfo(
  Sender: TObject;
  DefaultQuotaThreshold: Int64;
  DefaultQuotaLimit: Int64;
  FileSystemControlFlags: Int64;
  var ResultCode: Integer);
var
  pDiskQuotaControl: IDiskQuotaControl;
  DiskPath: WideString;
begin
  try
    oleInitialize(nil);
    OleCheck(CoCreateInstance(CLSID_DiskQuotaControl, nil, CLSCTX_INPROC_SERVER,
      IID_IDiskQuotaControl, pDiskQuotaControl));
    DiskPath := FRootPath[1] +':\';
    OleCheck(pDiskQuotaControl.Initialize(PWideChar(DiskPath), true));
    OleCheck(pDiskQuotaControl.SetDefaultQuotaThreshold(DefaultQuotaThreshold));
    OleCheck(pDiskQuotaControl.SetDefaultQuotaLimit(DefaultQuotaLimit));
    OleCheck(pDiskQuotaControl.SetQuotaLogFlags((FileSystemControlFlags and $000000F0) shr 4));
    OleCheck(pDiskQuotaControl.SetQuotaState(FileSystemControlFlags and $0F));
  finally
    begin
      pDiskQuotaControl := nil;
      OleUninitialize;
    end;
  end;
end;
{$endif}

procedure TFormFolderDrive.CBSetReparsePoint(
  Sender: TObject;
  const FileName: String;
  ReparseTag: Int64;
  ReparseBuffer: Pointer;
  ReparseBufferLength: Integer;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  Overlapped: TOverlapped;
  Ctx: PFILE_CONTEXT;
  BytesReturned: LongWord;
begin
  try
    FillChar(Overlapped, SizeOf(Overlapped), 0);
    Ctx := PFILE_CONTEXT(FileContext);
    if not DeviceIoControl(Ctx.hFile,
          FSCTL_SET_REPARSE_POINT,
          Pointer(ReparseBuffer),
          ReparseBufferLength,
          nil,
          0,
          BytesReturned,
          @Overlapped) then
      ResultCode := GetLastError;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBGetReparsePoint(
  Sender: TObject;
  const FileName: String;
  ReparseBuffer: Pointer;
  var ReparseBufferLength: Integer;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  Overlapped: TOverlapped;
  Ctx: PFILE_CONTEXT;
  BytesReturned: LongWord;
  Result: Boolean;
  Error: LongWord;
  hFile: THandle;
begin
  try
    BytesReturned := 0;
    FillChar(Overlapped, SizeOf(Overlapped), 0);
    Ctx := PFILE_CONTEXT(FileContext);
    if (Ctx = nil) or (Ctx.hFile = INVALID_HANDLE_VALUE) then // The file is not opened
    begin
      BytesReturned := 0;
      FillChar(Overlapped, SizeOf(Overlapped), 0);
      hFile := CreateFile( PChar(FRootPath + FileName),
        READ_CONTROL,
        0,
        nil,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS or FILE_FLAG_OPEN_REPARSE_POINT,
        0 );

      Error := GetLastError;
      if (hFile = INVALID_HANDLE_VALUE) then
      begin
        ResultCode := Error;
        exit;
      end;

      Result := DeviceIoControl( hFile,              // handle to file or directory
          FSCTL_GET_REPARSE_POINT, // dwIoControlCode
          nil,                     // input buffer
          0,                       // size of input buffer
          Pointer(ReparseBuffer),  // lpOutBuffer
          ReparseBufferLength,     // nOutBufferSize
          BytesReturned,          // lpBytesReturned
          @Overlapped);            // OVERLAPPED structure

      ReparseBufferLength := BytesReturned;
      Error := GetLastError;
      CloseHandle(hFile);
      if not Result then
      begin
        ResultCode := Error;
        exit;
      end;
      exit;
    end;
    Result := DeviceIoControl( Ctx.hFile,              // handle to file or directory
          FSCTL_GET_REPARSE_POINT, // dwIoControlCode
          nil,                     // input buffer
          0,                       // size of input buffer
          Pointer(ReparseBuffer),  // lpOutBuffer
          ReparseBufferLength,     // nOutBufferSize
          BytesReturned,           // lpBytesReturned
          @Overlapped);            // OVERLAPPED structure

    ReparseBufferLength := BytesReturned;
    Error := GetLastError;
    if not Result then
    begin
      ResultCode := Error;
      exit;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormFolderDrive.CBDeleteReparsePoint(
  Sender: TObject;
  const FileName: String;
  ReparseBuffer: Pointer;
  ReparseBufferLength: Integer;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  Overlapped: TOverlapped;
  Ctx: PFILE_CONTEXT;
  BytesReturned, Error: LongWord;
begin
  try
    BytesReturned := 0;
    FillChar(Overlapped, SizeOf(Overlapped), 0);
    Ctx := PFILE_CONTEXT(FileContext);
    if not DeviceIoControl( Ctx.hFile,                 // handle to file or directory
          FSCTL_DELETE_REPARSE_POINT, // dwIoControlCode
          Pointer(ReparseBuffer),     // input buffer
          ReparseBufferLength,        // size of input buffer
          nil,                        // lpOutBuffer
          0,                          // nOutBufferSize
          BytesReturned,              // lpBytesReturned
          @Overlapped) then           // OVERLAPPED structure
    begin
      Error := GetLastError;
      ResultCode := Error;
      exit;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

function TFormFolderDrive.GetFileId(FileName: string): Int64;
var
  fi: BY_HANDLE_FILE_INFORMATION;
  hFile: THandle;
begin
  Result := 0;
  hFile := CreateFile( PChar(FileName),
    READ_CONTROL,
    0,
    nil,
    OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS or FILE_FLAG_OPEN_REPARSE_POINT,
    0 );

  if (hFile <> INVALID_HANDLE_VALUE) and GetFileInformationByHandle(hFile, fi) then
    Result := ((Int64(fi.nFileIndexHigh) shl 32) and $FFFFFFFF00000000) or fi.nFileIndexLow;
  if (hFile <> INVALID_HANDLE_VALUE) then
    CloseHandle(hFile);
end;

function TFormFolderDrive.ConvertRelativePathToAbsolute(const path: string; acceptMountingPoint : boolean): string;
var
  res, remainedPath, homeDir: string;
  semicolonCount: Integer;
begin
  res := '';
  if not path.IsEmpty then
  begin
    res := path;

    // Linux-specific case of using a home directory
    if (path = '~') or path.StartsWith('~/') then
    begin
      homeDir := TPath.GetHomePath;
      if (path = '~') or (path = '~/') then
        Exit(homeDir)
      else
        Exit(TPath.Combine(homeDir, Copy(path, 2, MaxInt)));
    end;

    semicolonCount := Length(path.Split([';'])) - 1;
    if semicolonCount = 2 then
    begin
      if not acceptMountingPoint then
      begin
        MessageDlg('The network folder "' + path + '" format cannot be equal to the Network Mounting Point',
          mtError, [mbOk], 0);
        Exit('');
      end;

      var pos := Pos(';', path);
      if (pos <> 0) and (pos <> Length(path)) then
      begin
        res := Copy(path, 1, pos - 1);
        if res.IsEmpty then
          Exit(path);
        remainedPath := Copy(path, pos, MaxInt);
      end;
    end;

    if IsDriveLetter(res) then
    begin
      if not acceptMountingPoint then
      begin
        MessageDlg('The path "' + res + '" format cannot be equal to the Drive Letter',
          mtError, [mbOk], 0);
        Exit('');
      end;
      Exit(path);
    end;

    if not TPath.IsPathRooted(res) then
    begin
      res := TPath.GetFullPath(res);
      Exit(res + remainedPath);
    end;
  end;

  Result := path;
end;

function TFormFolderDrive.IsDriveLetter(const Path: string): Boolean;
begin
  Result := False;

  if (Path <> '') and (not Path.IsEmpty) then
  begin
    if (Path[1] in ['A'..'Z', 'a'..'z']) and (Length(Path) = 2) and (Path[2] = ':') then
      Result := True;
  end;
end;

procedure TFormFolderDrive.CBSetVolumeLabel(Sender: TObject;
  const VolumeLabel: String; var ResultCode: Integer);
begin
;
end;

procedure TFormFolderDrive.CBUnmount(Sender: TObject; var ResultCode: Integer);
begin
;
end;

procedure TFormFolderDrive.CBEjected(Sender: TObject; var ResultCode: Integer);
begin
  btnUnmount.Enabled := false;
  btnAddPoint.Enabled := false;
  btnDeletePoint.Enabled := false;
end;

procedure TFormFolderDrive.FormCreate(Sender: TObject);
begin
  FCbFs := TcbcCBFS.Create(Self);
  FCbFs.Config('SectorSize=' + IntToStr(SECTOR_SIZE));

  FCbFs.OnMount := CBMount;
  FCbFs.OnUnmount := CBUnmount;
  FCbFs.OnGetVolumeSize := CBGetVolumeSize;
  FCbFs.OnGetVolumeLabel := CBGetVolumeLabel;
  FCbFs.OnSetVolumeLabel := CBSetVolumeLabel;
  FCbFs.OnGetVolumeId := CBGetVolumeId;
  FCbFs.OnCreateFile := CBCreateFile;
  FCbFs.OnOpenFile := CBOpenFile;
  FCbFs.OnCloseFile := CBCloseFile;
  FCbFs.OnGetFileInfo := CBGetFileInfo;
  FCbFs.OnEnumerateDirectory := CBEnumerateDirectory;
  FCbFs.OnCloseDirectoryEnumeration := CBCloseDirectoryEnumeration;
  FCbFs.OnSetAllocationSize := CBSetAllocationSize;
  FCbFs.OnSetFileSize := CBSetFileSize;
  FCbFs.OnSetFileAttributes := CBSetFileAttributes;
  FCbFs.OnCanFileBeDeleted := CBCanFileBeDeleted;
  FCbFs.OnDeleteFile := CBDeleteFile;
  FCbFs.OnRenameOrMoveFile := CBRenameOrMoveFile;
  FCbFs.OnReadFile := CBReadFile;
  FCbFs.OnWriteFile := CBWriteFile;
  FCbFs.OnIsDirectoryEmpty := CBIsDirectoryEmpty;

  FCbFs.UseAlternateDataStreams := true;
  FCbFs.OnEnumerateNamedStreams := CBEnumerateNamedStreams;
  FCbFs.OnCloseNamedStreamsEnumeration := CBCloseNamedStreamsEnumeration;
  FCbFS.OnEjected := CBEjected;

  // Uncomment in order to support file security.
  FCbFs.UseWindowsSecurity := true;
  FCbFs.OnSetFileSecurity := CBSetFileSecurity;
  FCbFs.OnGetFileSecurity := CBGetFileSecurity;

  FCbFs.FileSystemName := 'NTFS';

  // Uncomment in order to support file IDs (necessary for NFS sharing) or
  // hard links (the CBEnumerateHardLinks and CBCreateHardLink callbacks).
  //FCbFs.OnGetFileNameByFileId := CBGetFileNameByFileId;
  //FCbFs.UseFileIds:= true;

  // Uncomment in order to support hard links (several names for the same file,
  // it works only if the CBGetFileNameByFileId callback is defined too).
  //FCbFS.UseHardLinks := true;
  //FCbFS.OnEnumerateHardLinks := CBEnumerateHardLinks;
  //FCbFS.OnCloseHardLinksEnumeration := CBCloseHardLinksEnumeration;
  //FCbFS.OnCreateHardLink := CBCreateHardLink;
  //

  //
  // Uncomment in order to support reparse points (symbolic links, mounting points, etc.) support. Also required for NFS sharing
  //
  //FCbFs.UseReparsePoints := true;
  //FCbFs.OnSetReparsePoint := CBSetReparsePoint;
  //FCbFs.OnGetReparsePoint := CBGetReparsePoint;
  //FCbFs.OnDeleteReparsePoint := CBDeleteReparsePoint;

  {$ifdef DISK_QUOTAS_SUPPORTED}
  FCbFs.UseDiskQuotas := true;
  FCbFs.OnQueryQuotas := CBQueryQuotas;
  FCbFs.OnSetQuotas := CBSetQuotas;
  FCbFs.OnCloseQuotasEnumeration := CBCloseQuotasEnumeration;
  FCbFs.OnGetDefaultQuotaInfo := CBGetDefaultQuotaInfo;
  FCbFs.OnSetDefaultQuotaInfo := CBSetDefaultQuotaInfo;

  // This feature works only if the CBGetFileNameByFileId callback is defined too
  FCbFs.OnGetFileNameByFileId := CBGetFileNameByFileId;
  {$endif}

  UpdateDriverStatus;
end;

procedure TFormFolderDrive.FormDestroy(Sender: TObject);
var
  Index: Integer;
begin
  if FCbFs.Active then
    for Index := FCbFs.MountingPointCount - 1 downto 0 do
      FCbFs.RemoveMountingPoint(Index, '', 0, 0);

  if FCbFs.Active then
    FCbFs.UnmountMedia(True);
  if FCbFs.StoragePresent then
    FCbFs.DeleteStorage(True);
  FCbFs.Free;
end;

procedure TFormFolderDrive.AskForReboot(isInstall: Boolean);
var
  t: String;
begin
  if isInstall then
    t := 'install'
  else
    t := 'uninstall';
  MessageDlg('System restart is needed in order to ' + t + ' the drivers.' +
    #13#10 + 'Please, reboot your computer now.', mtInformation, [mbOK], 0);
end;

procedure TFormFolderDrive.btnInstallClick(Sender: TObject);
var
  Reboot: Integer;
  PathToInstall: string;
begin
  SetLength(PathToInstall, MAX_PATH);
  GetSystemDirectory(PChar(PathToInstall), MAX_PATH);
  SetLength(PathToInstall,strlen(PChar(PathToInstall)));
  if dlgOpenDrv.Execute then
  begin
    Reboot := 0;

    try
      Reboot := FCbFs.Install(dlgOpenDrv.FileName, FGuid, PathToInstall,
        MODULE_DRIVER or MODULE_PNP_BUS, INSTALL_REMOVE_OLD_VERSIONS);
    except on E:Exception do
      begin
        if GetLastError = ERROR_PRIVILEGE_NOT_HELD then
          MessageDlg('Installation requires administrator rights. Run the app as administrator', mtError, [mbOk], 0)
        else
          Application.ShowException(E);
      end;
    end;

    UpdateDriverStatus;
    if Reboot <> 0 then
      AskForReboot(true);
  end;
end;

procedure TFormFolderDrive.btnUninstallClick(Sender: TObject);
var
  Reboot: Integer;
  InstalledPath: string;
begin
  SetLength(InstalledPath, 0);
  if dlgOpenDrv.Execute then
  begin
    Reboot := 0;

    try
      Reboot := FCbFs.Uninstall(dlgOpenDrv.FileName, FGuid, InstalledPath, 0);
    except on E:Exception do
      begin
        if GetLastError = ERROR_PRIVILEGE_NOT_HELD then
          MessageDlg('Uninstallation requires administrator rights. Run the app as administrator', mtError, [mbOk], 0)
        else
          Application.ShowException(E);
      end;
    end;

    UpdateDriverStatus;
    if Reboot <> 0 then
      AskForReboot(false);
  end;
end;

procedure TFormFolderDrive.UpdateDriverStatus;
var
  StrStat : string;
  Version : Int64;
  Status : Integer;
begin
  Status := FCbFs.GetDriverStatus(FGuid, MODULE_DRIVER);

  if Status <> 0 then
  begin
    case Status of
      SERVICE_CONTINUE_PENDING : strStat := 'continue is pending';
      SERVICE_PAUSE_PENDING : strStat := 'pause is pending';
      SERVICE_PAUSED : strStat := 'is paused';
      SERVICE_RUNNING : strStat := 'is running';
      SERVICE_START_PENDING : strStat := 'is starting';
      SERVICE_STOP_PENDING : strStat := 'is stopping';
      SERVICE_STOPPED : strStat := 'is stopped';
    else
      strStat := 'in undefined state';
    end;

    Version := FCbFs.GetModuleVersion(FGuid, MODULE_DRIVER);
    lblDriverStatus.Caption := Format('Driver Status: (ver %d.%d.%d.%d) installed, service %s',
      [Version shr 48, (Version shr 32) and $FFFF, (Version shr 16) and $FFFF, Version and $FFFF, StrStat]);
  end
  else
    lblDriverStatus.Caption := 'Driver not installed';

  btnCreateVirtualDrive.Enabled := (Status <> 0);
end;

procedure TFormFolderDrive.btnCreateVirtualDriveClick(Sender: TObject);
begin
  //
  // Uncomment for creation of PnP storage, ejectable from system tray
  //
  {
  FCbFs.StorageType := STGT_DISK_PNP;
  FCbFs.StorageCharacteristics := STGC_REMOVABLE_MEDIA or STGC_SHOW_IN_EJECTION_TRAY or STGC_ALLOW_EJECTION;
  }

  //
  // Uncomment in order to support NFS sharing.
  //
  {
  FCbFs.StorageType := STGT_DISK_PNP;
  FCbFs.StorageCharacteristics := 0;
  FCbFs.FileSystemName := 'NTFS';
  }

  //
  // Uncomment to use advanced features Windows Security, Reparse points etc.
  //
  {
  FCbFs.FileSystemName := 'NTFS';
  }

{$ifdef DISK_QUOTAS_SUPPORTED}
  if not IsAdmin() then
  begin
    MessageDlg('Disk quota demo requires administrator rights. Run the app as administrator', mtError, [mbOk], 0);
    Exit;
  end;
{$endif}

  FCbFs.Initialize(FGuid);
  FCbFs.CreateStorage;

  btnCreateVirtualDrive.Enabled := false;
  btnDeleteVirtualDrive.Enabled := true;
  btnMount.Enabled := true;
end;

procedure TFormFolderDrive.btnDeleteVirtualDriveClick(Sender: TObject);
begin
  FCbFs.DeleteStorage(true);
  btnUnmount.Enabled := false;
  btnMount.Enabled := false;
  btnDeleteVirtualDrive.Enabled := false;
  btnCreateVirtualDrive.Enabled := true;
end;

procedure TFormFolderDrive.btnMountClick(Sender: TObject);
var
  Handle: THandle;
begin
  FRootPath := ExcludeTrailingBackslash(ConvertRelativePathToAbsolute(edtRootPath.Text, false));
  // The directory, pointed to by FRootPath, is opened in read-write mode 
  // to ensure that the mapped directory is accessible and can be used as a backend media.
  // When implementing your filesystem, you may want to implement a similar check of availability of your backend. 
  Handle := CreateFile( PChar(FRootPath),
    GENERIC_READ,
    FILE_SHARE_READ or FILE_SHARE_WRITE or FILE_SHARE_DELETE,
    nil,
    OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS,
    0);

  if Handle = INVALID_HANDLE_VALUE then
  begin
    if not CreateDirectory(PChar(FRootPath), nil) then
      begin
        MessageDlg('Cannot create root directory, invalid path name', mtError, [mbOk], 0);
        Exit;
      end;
  end
  else
    CloseHandle(Handle);

  FCbFs.MountMedia(0);
  btnMount.Enabled := false;
  btnUnmount.Enabled := true;
  btnAddPoint.Enabled := true;
end;

procedure TFormFolderDrive.btnUnmountClick(Sender: TObject);
begin
  FCbFs.UnmountMedia(true);

  btnMount.Enabled := true;
  btnUnmount.Enabled := false;
  btnAddPoint.Enabled := false;
  btnDeletePoint.Enabled := false;
end;

procedure TFormFolderDrive.btnAddPointClick(Sender: TObject);
var
  absolutePath: string;
begin
  absolutePath := ConvertRelativePathToAbsolute(edtMountingPoint.Text, True);
  if (absolutePath = '') then
  begin
    MessageDlg('Error: Invalid Mounting Point Path.',
      mtError, [mbOk], 0);
    Exit;
  end;
  Screen.Cursor := crHourGlass;
  edtMountingPoint.Enabled := false;
  FCbFs.AddMountingPoint(absolutePath, STGMP_MOUNT_MANAGER, 0);
  btnAddPoint.Enabled := false;
  btnDeletePoint.Enabled := true;
  ShellExecute(Application.Handle, 'open', 'explorer.exe', PChar(absolutePath), nil, SW_NORMAL);
  Screen.Cursor := crDefault;
end;

procedure TFormFolderDrive.btnDeletePointClick(Sender: TObject);
begin
  Screen.Cursor := crHourGlass;
  FCbFs.RemoveMountingPoint(0, '', 0, 0);
  btnAddPoint.Enabled := true;
  btnDeletePoint.Enabled := false;
  edtMountingPoint.Enabled := true;
  Screen.Cursor := crDefault;
end;

{ ECBFSConnectCustomError }

constructor ECBFSConnectCustomError.Create(const AMessage: string);
begin
  inherited Create(AMessage);
  FErrorCode := 0;
end;

constructor ECBFSConnectCustomError.Create(AErrorCode : Integer);
begin
  inherited Create('');
  FErrorCode := AErrorCode;
end;

constructor ECBFSConnectCustomError.Create(AErrorCode: Integer;
  const AMessage: string);
begin
  inherited Create(AMessage);
  FErrorCode := AErrorCode;
end;

var
  dllHandle: Cardinal;

initialization

  ZeroFiletime := EncodeDateTime(1601, 1, 1, 0, 0, 0, 0);
  dllHandle := LoadLibrary('kernel32.dll');
  if dllHandle = 0 then
    RaiseLastWin32Error;
  @FindFirstFileName := GetProcAddress(dllHandle, 'FindFirstFileNameW');
  if not Assigned(FindFirstFileName) then
    RaiseLastWin32Error;
  @FindNextFileName := GetProcAddress(dllHandle, 'FindNextFileNameW');
  if not Assigned(FindNextFileName) then
    RaiseLastWin32Error;

  @FindFirstStream := GetProcAddress(dllHandle, 'FindFirstStreamW');
  if not Assigned(FindFirstStream) then
    RaiseLastWin32Error;
  @FindNextStream := GetProcAddress(dllHandle, 'FindNextStreamW');
  if not Assigned(FindNextStream) then
    RaiseLastWin32Error;

end.



