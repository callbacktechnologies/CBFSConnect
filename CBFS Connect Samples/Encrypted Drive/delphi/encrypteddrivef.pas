(*
 * CBFS Connect 2022 Delphi Edition - Sample Project
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
unit EncryptedDriveF;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms,
  Dialogs, StdCtrls, IOUtils, RegularExpressions,
  cbccore, cbcconstants, cbccbfs,
  uEncrypt, ExtCtrls;

const
  SERVICE_STOPPED = 1;
  SERVICE_START_PENDING = 2;
  SERVICE_STOP_PENDING = 3;
  SERVICE_RUNNING = 4;
  SERVICE_CONTINUE_PENDING = 5;
  SERVICE_PAUSE_PENDING = 6;
  SERVICE_PAUSED = 7;

  SECTOR_SIZE = 512;

  FILE_READ_DATA            = $0001;   
  FILE_WRITE_DATA           = $0002;
  FILE_APPEND_DATA          = $0004;
  FILE_READ_EA              = $0008;
  FILE_WRITE_EA             = $0010;
  FILE_FLAG_OPEN_REPARSE_POINT = $00200000;
  
type
  PWIN32_FIND_STREAM_DATA = ^WIN32_FIND_STREAM_DATA;
  WIN32_FIND_STREAM_DATA = packed record
    StreamSize : Int64;
    cStreamName : array [0..MAX_PATH + 36-1] of WideChar;
  end;

  TFindFirstFileNameFunc = function (lpFileName: PChar; dwFlags: LongWord;
    var StringLength: LongWord; LinkName: PChar): THandle; stdcall;

  TFindNextFileNameFunc = function (hFile: THandle; var StringLength: LongWord;
    LinkName: PChar): Boolean; stdcall;

  TFindFirstStreamFunc = function (lpFileName: PChar; dwLevel: LongWord;
    var StreamData : WIN32_FIND_STREAM_DATA; dwFlags: LongWord): THandle; stdcall;

  TFindNextStreamFunc = function (hFile: THandle; var StreamData : WIN32_FIND_STREAM_DATA): Boolean; stdcall;

  TGetFileInfoFunc = function(hFile: THandle; FileInformationClass: Cardinal;
  lpFileInformation: Pointer; dwBufferSize: LongWord):Boolean; stdcall;

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

const
   WM_SET_INPUTBOX_LEN = WM_USER + 1;

var
  FindFirstStream: TFindFirstStreamFunc;
  FindNextStream: TFindNextStreamFunc;
  FindFirstFileName: TFindFirstFileNameFunc;
  FindNextFileName: TFindNextFileNameFunc;

type

  TFormEncryptedDrive = class(TForm)
    lblIntro: TLabel;
    gbDriver: TGroupBox;
    btnInstall: TButton;
    btnUninstall: TButton;
    lblDriverStatus: TLabel;
    lblDriver: TLabel;
    gbMedia: TGroupBox;
    lblMedia: TLabel;
    edtRootPath: TEdit;
    btnMount: TButton;
    btnUnmount: TButton;
    gbEncryption: TGroupBox;
    lblPassword: TLabel;
    edtPassword: TEdit;
    gbMountingPoints: TGroupBox;
    lblMounting: TLabel;
    edtMountingPoint: TEdit;
    lblMountingPoints: TLabel;
    btnAddPoint: TButton;
    btnDeletePoint: TButton;
    lstPoints: TListBox;
    lblPoints: TLabel;
    dlgOpenDrv: TOpenDialog;

    procedure btnMountClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure btnInstallClick(Sender: TObject);
    procedure btnUninstallClick(Sender: TObject);
    procedure btnUnmountClick(Sender: TObject);
    procedure btnAddPointClick(Sender: TObject);
    procedure btnDeletePointClick(Sender: TObject);
    procedure lstPointsClick(Sender: TObject);
  private
    FCbFs: TcbcCBFS;

    FXORData : ByteArray;

    procedure AskForReboot(IsInstall : boolean);
    procedure UpdateDriverStatus;
    procedure UpdateMountingPoints;
    procedure IsEmptyDirectory(const DirectoryName: String; var IsEmpty: Boolean;
      var ResultCode: Integer);

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
    procedure CBEjectStorage(Sender: TObject; var ResultCode: Integer);

    function GetFileId(FileName: string): Int64;

    function ConvertRelativePathToAbsolute(const path: string; acceptMountingPoint: Boolean): string;

    function IsDriveLetter(const path: string): Boolean;
  public
    FRootPath: string;

    property XORData : ByteArray read FXORData;
  end;

var
  FormEncryptedDrive: TFormEncryptedDrive;
const
  FGuid = '{713CC6CE-B3E2-4fd9-838D-E28F558F6866}';
const
  FILE_WRITE_ATTRIBUTES     = $0100; // all

const
  NTC_DIRECTORY_ENUM			= $7534;
  NTC_NAMED_STREAMS_ENUM	= $6209;

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
  ENUM_INFO = record
    Header: CONTEXT_HEADER;
    finfo: TSearchRec;
    hFile: THandle;
  end;

  PENUM_INFO = ^ENUM_INFO;

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

procedure TFormEncryptedDrive.CBCanFileBeDeleted(
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

procedure TFormEncryptedDrive.CBCloseDirectoryEnumeration(
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
      pHeader := PCONTEXT_HEADER(Pointer(EnumerationContext));
      if NTC_DIRECTORY_ENUM = pHeader.NodeTypeCode then
      begin
        pDirectoryEnumInfo := PENUM_INFO(Pointer(EnumerationContext));
        FindClose(pDirectoryEnumInfo.finfo);
        FreeMem(pDirectoryEnumInfo);
      end
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormEncryptedDrive.CBCloseNamedStreamsEnumeration(
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
      pHeader := PCONTEXT_HEADER(Pointer(EnumerationContext));
      if NTC_NAMED_STREAMS_ENUM = pHeader.NodeTypeCode then
      begin
        pStreamEnumInfo := PNAMED_STREAM_CONTEXT(Pointer(EnumerationContext));
        CloseHandle(pStreamEnumInfo.hFile);
        FreeMem(pStreamEnumInfo);
      end;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormEncryptedDrive.CBCloseFile(
  Sender: TObject;
  const FileName: String;
  PendingDeletion: Boolean;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  Context: TEncryptContext;
begin
  try
    Context := TEncryptContext(FileContext);
    if (Context <> nil) and (Context.DecrementRef = 0) then
    begin
      Context.Free;
      // FileContext := nil;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormEncryptedDrive.CBCreateFile(
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
  Context: TEncryptContext;
  ZeroizeFileContext : Boolean;
begin
  try
    if FileContext = nil then
    begin
      Context := TEncryptContext.Create(TcbcCBFS(Sender), FileName, FileAttributes, DesiredAccess, NTCreateDisposition, true, ZeroizeFileContext);
      if ZeroizeFileContext then
        FileContext := nil;

      if Context.Initialized then
        FileContext := Pointer(Context)
      else
      begin
        FileContext := nil;
        Context.Free;
      end;
    end
    else
    begin
      Context := TEncryptContext(Pointer(FileContext));
      if Context.ReadOnly and (DesiredAccess and (FILE_WRITE_DATA or FILE_APPEND_DATA or FILE_WRITE_ATTRIBUTES or FILE_WRITE_EA) <> 0 ) then
      begin
        ResultCode := ERROR_ACCESS_DENIED;
        Exit;
      end;
      Context.IncrementRef;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormEncryptedDrive.CBDeleteFile(
  Sender: TObject;
  const FileName: String;
  var FileContext: Pointer;
  var ResultCode: Integer);
var
  FullName: string;
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

procedure TFormEncryptedDrive.CBEnumerateDirectory(
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
      Size := Int64(P.finfo.FindData.nFileSizeLow) or
        Int64(P.finfo.FindData.nFileSizeHigh) shl 32;
      AllocationSize := Size;
      Attributes := P.finfo.FindData.dwFileAttributes;
      FileId := 0;
      //
      // If FileId is supported then get FileId for the file.
      //
      if @FCbFs.OnGetFileNameByFileId <> nil then
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

procedure TFormEncryptedDrive.CBReadFile(
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
  Context: TEncryptContext;
  Read : Cardinal;
begin
  try
    if BytesToRead > $7fffffff then
      BytesToRead := $7fffffff;

    Context := TEncryptContext(FileContext);
    if Context <> nil then
    begin
      ResultCode := Context.Read(Position, Buffer^, BytesToRead, Read);
      BytesRead := Read;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormEncryptedDrive.CBWriteFile(
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
  Context: TEncryptContext;
  Written : Cardinal;
begin
  try
    if BytesToWrite > $7fffffff then
      BytesToWrite := $7fffffff;

    Context := TEncryptContext(FileContext);
    if Context <> nil then
    begin
      ResultCode := Context.Write(Position, Buffer^, BytesToWrite, Written);
      BytesWritten := Written;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormEncryptedDrive.CBGetFileInfo(
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
  FullName : string;
  fi: BY_HANDLE_FILE_INFORMATION;
  hFile: THandle;
  IsADS : Boolean;
  ADSMarker : Integer;
  Error, Attr : LongWord;
begin
  try
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

    Attributes := GetFileAttributes(PChar(FullName));
    FileExists := Attributes <> $FFFFFFFF;
    if FileExists then
    begin
      hFile := CreateFile(PChar(FullName),
            READ_CONTROL,
            0,
            nil,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
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
          Size := Int64(fi.nFileSizeLow) or
            (Int64(fi.nFileSizeHigh) shl 32);
          AllocationSize := Size;
          HardLinkCount := fi.nNumberOfLinks;
          if @FCbFs.OnGetFileNameByFileId = nil then
            FileId := 0
          else
            FileId := ((Int64(fi.nFileIndexHigh) shl 32) and $FFFFFFFF00000000) or Int64(fi.nFileIndexLow);

          Attributes := fi.dwFileAttributes;
        end;

        CloseHandle(hFile);
      end;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormEncryptedDrive.CBGetVolumeId(Sender: TObject;
  var VolumeId: Integer; var ResultCode: Integer);
begin
  VolumeID := $12345678;
end;

procedure TFormEncryptedDrive.CBGetVolumeLabel(Sender: TObject;
  var VolumeLabel: String; var ResultCode: Integer);
begin
  VolumeLabel := 'CbFs Test';
end;

procedure TFormEncryptedDrive.CBGetVolumeSize(
  Sender: TObject;
  var TotalSectors: Int64;
  var AvailableSectors: Int64;
  var ResultCode: Integer);
var
  FreeBytesToCaller, TotalBytes: Int64;
  FreeBytes: Int64;
begin
  try
    Win32Check(GetDiskFreeSpaceEx(PChar(FRootPath), FreeBytesToCaller,
      TotalBytes, PLargeInteger(@FreeBytes)));
    TotalSectors := TotalBytes div SECTOR_SIZE;
    AvailableSectors := FreeBytes div SECTOR_SIZE;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormEncryptedDrive.CBMount(Sender: TObject; var ResultCode: Integer);
begin
;
end;

procedure TFormEncryptedDrive.CBOpenFile(
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
  ZeroizeFileContext : Boolean;
  Context: TEncryptContext;
begin
  try
    if FileContext = nil then
    begin
      Context := TEncryptContext.Create(TcbcCBFS(Sender), FileName, FileAttributes, DesiredAccess, NTCreateDisposition, false, ZeroizeFileContext);
      if ZeroizeFileContext then
        FileContext := nil;

      if Context.Initialized then
        FileContext := Pointer(Context)
      else
      begin
        FileContext := nil;
        Context.Free;
      end;
    end
    else
    begin
      Context := TEncryptContext(Pointer(FileContext));
      if Context.ReadOnly and (DesiredAccess and (FILE_WRITE_DATA or FILE_APPEND_DATA or FILE_WRITE_ATTRIBUTES or FILE_WRITE_EA) <> 0 ) then

      if Context.ReadOnly and (not isReadRightForAttrOrDeleteRequested(DesiredAccess)) and
        ((not isWriteRightForAttrRequested(DesiredAccess)) or isWriteRightRequested(DesiredAccess)) then
      begin
        ResultCode := ERROR_ACCESS_DENIED;
        Exit;
      end;
      Context.IncrementRef;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormEncryptedDrive.CBRenameOrMoveFile(
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

procedure TFormEncryptedDrive.CBSetFileAttributes(
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
  Context: TEncryptContext;
  hFile, Attr: LongWord;
begin
  try
    try
      hFile := INVALID_HANDLE_VALUE;

      // the case when FileAttributes == 0 indicates that file attributes
      // not changed during this callback
      if FileAttributes <> 0 then
      begin
        // If we set the read-only attribute now, we won't be able to change file times below
        // So we need to postpone setting of the attributes if they include the read-only flag.

        if (FileAttributes and FILE_ATTRIBUTE_READONLY) = 0 then
          Win32Check(SetFileAttributes(PChar(FRootPath + FileName), FileAttributes));
      end;

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
        Win32Check(SetFileTime(hFile, CreationPtr,
          LastAccessPtr, LastWritePtr));

      if ((Attr and FILE_ATTRIBUTE_READONLY) <> 0) and (FileAttributes = 0) then
      begin
        Win32Check(SetFileAttributes(PChar(FRootPath + FileName), Attr));
      end;

      //
      // the case when Attributes == 0 indicates that file attributes
      // not changed during this callback
      //
      if FileAttributes <> 0 then
      begin
        // If we had to set the read-only attribute, we didn't do this above
        // and we need to set the attributes now.
        if (FileAttributes and FILE_ATTRIBUTE_READONLY) <> 0 then
          Win32Check(SetFileAttributes(PChar(FRootPath + FileName), FileAttributes));
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

procedure TFormEncryptedDrive.CBSetAllocationSize(
  Sender: TObject;
  const FileName: String;
  AllocationSize: Int64;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
begin
  // NOTHING TO DO
end;

procedure TFormEncryptedDrive.CBSetFileSize(
  Sender: TObject;
  const FileName: String;
  Size: Int64;
  HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  Context: TEncryptContext;
begin
  try
    Context := TEncryptContext(Pointer(FileContext));
    if Context <> nil then
      Context.CurrentSize := Size;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormEncryptedDrive.IsEmptyDirectory(const DirectoryName: String;
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

procedure TFormEncryptedDrive.CBIsDirectoryEmpty(
  Sender: TObject;
  const DirectoryName: String;
  var IsEmpty: Boolean;
  var DirectoryContext: Pointer;
  var ResultCode: Integer);
begin
  IsEmptyDirectory(DirectoryName, IsEmpty, ResultCode);
end;

procedure TFormEncryptedDrive.CBEnumerateNamedStreams(
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
        nsc.hFile := FindFirstStream(PChar(FRootPath + FileName), 0, fsd, 0);
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

procedure TFormEncryptedDrive.CBSetFileSecurity(
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
  if Not SetFileSecurity( PChar(FRootPath + FileName), SecurityInformation, SecurityDescriptor) then
    ResultCode := GetLastError();
end;

procedure TFormEncryptedDrive.CBGetFileSecurity(
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
  cLengthNeeded : LongWord;
  Error: LongWord;
begin
  try
    //
    // Getting SACL_SECURITY_INFORMATION requires the program to
    // execute elevated as well as the SE_SECURITY_NAME privilege
    // to be set. So just remove it from the request.
    //
    SACL_SECURITY_INFORMATION := $00000008;
    SecurityInformation := SecurityInformation and not SACL_SECURITY_INFORMATION;
    if SecurityInformation = 0 then
      Exit;

    cLengthNeeded := DescriptorLength;
    UpcasePath := UpperCase(FileName);

    //
    // For recycle bin Windows expects some specific attributes
    // (if they aren't set then it's reported that the recycle
    // bin is corrupted). So just get attributes from the recycle
    // bin files located on the volume "C:".
    //
    if (Pos('$RECYCLE.BIN', UpcasePath) <> 0) or
       (Pos('RECYCLER', UpcasePath) <> 0) then
    begin
      PathOnVolumeC := 'C:' + FileName;
      if not GetFileSecurity(PChar(PathOnVolumeC),
        SecurityInformation, SecurityDescriptor, BufferLength, cLengthNeeded) then
      begin
        //
        // If the SecurityDescriptor buffer is smaller than required then
        // GetFileSecurity itself sets in the LengthNeeded parameter the required
        // size and the last error is set to ERROR_INSUFFICIENT_BUFFER.
        //
        DescriptorLength := cLengthNeeded;
        Error := GetLastError;
        ResultCode := Error;
        Exit;
      end;
    end
    else
    if not GetFileSecurity(PChar(FRootPath + FileName),
       SecurityInformation, SecurityDescriptor, BufferLength, cLengthNeeded) then
    begin
      DescriptorLength := cLengthNeeded;
      Error := GetLastError;
      ResultCode := Error;
      Exit;
    end;

    DescriptorLength := cLengthNeeded;
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

procedure TFormEncryptedDrive.CBGetFileNameByFileId(
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
      RaiseLastOsError;

    dllHandle := LoadLibrary('kernel32.dll');
    if dllHandle = 0 then RaiseLastWin32Error;
    @GetFileInformationByHandleEx := GetProcAddress(dllHandle, 'GetFileInformationByHandleEx');
    if not Assigned(GetFileInformationByHandleEx) then
    begin
      FreeLibrary(dllHandle);
      RaiseLastWin32Error;
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
      Exit;
    end;

    //FileNameLength := NameInfo.FileNameLength div SizeOf(WCHAR) - (RootPathLength-Length('X:'));
    FilePath := Copy(NameInfo.FileName, RootPathLength - Cardinal(Length('X:')), MaxInt);
    FreeMemory(NameInfo);
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

function TFormEncryptedDrive.GetFileId(FileName: string): Int64;
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
    FILE_FLAG_BACKUP_SEMANTICS,
    0 );
  if (hFile <> INVALID_HANDLE_VALUE) and GetFileInformationByHandle(hFile, fi) then
    Result := ((Int64(fi.nFileIndexHigh) shl 32) and $FFFFFFFF00000000) or Int64(fi.nFileIndexLow);
  if (hFile <> INVALID_HANDLE_VALUE) then
    CloseHandle(hFile);
end;

procedure TFormEncryptedDrive.CBSetVolumeLabel(Sender: TObject;
  const VolumeLabel: String; var ResultCode: Integer);
begin
;
end;

procedure TFormEncryptedDrive.CBUnmount(Sender: TObject; var ResultCode: Integer);
begin
;
end;

procedure TFormEncryptedDrive.CBEjectStorage(Sender: TObject; var ResultCode: Integer);
begin
  try
    lstPoints.Clear;
    btnUnmount.Enabled := false;
    btnAddPoint.Enabled := false;
    btnDeletePoint.Enabled := false;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormEncryptedDrive.FormCreate(Sender: TObject);
begin
  FCbFs := TcbcCBFS.Create(Self);
  FCbFs.Config('SectorSize=' + IntToStr(SECTOR_SIZE));
  FCbFs.UseWindowsSecurity := true;
  FCbFs.UseAlternateDataStreams := true;

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
  FCbFs.OnCloseNamedStreamsEnumeration := CBCloseNamedStreamsEnumeration;
  FCbFs.OnSetAllocationSize := CBSetAllocationSize;
  FCbFs.OnSetFileSize := CBSetFileSize;
  FCbFs.OnSetFileAttributes := CBSetFileAttributes;
  FCbFs.OnCanFileBeDeleted := CBCanFileBeDeleted;
  FCbFs.OnDeleteFile := CBDeleteFile;
  FCbFs.OnRenameOrMoveFile := CBRenameOrMoveFile;
  FCbFs.OnReadFile := CBReadFile;
  FCbFs.OnWriteFile := CBWriteFile;
  FCbFs.OnIsDirectoryEmpty := CBIsDirectoryEmpty;
  FCbFs.OnEnumerateNamedStreams := CBEnumerateNamedStreams;
  FCbFs.OnSetFileSecurity := CBSetFileSecurity;
  FCbFs.OnGetFileSecurity := CBGetFileSecurity;
  // Uncomment in order to support NFS sharing.
  //FCbFs.OnGetFileNameByFileId := CBGetFileNameByFileId;
  FCbFS.OnEjected := CBEjectStorage;

  UpdateDriverStatus;
end;

procedure TFormEncryptedDrive.FormDestroy(Sender: TObject);
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

procedure TFormEncryptedDrive.AskForReboot(isInstall: Boolean);
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

procedure TFormEncryptedDrive.btnInstallClick(Sender: TObject);
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

procedure TFormEncryptedDrive.btnUninstallClick(Sender: TObject);
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

procedure TFormEncryptedDrive.UpdateDriverStatus;
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
    lblDriverStatus.Caption := Format('Driver (ver %d.%d.%d.%d) installed, service %s',
      [Version shr 48, (Version shr 32) and $FFFF, (Version shr 16) and $FFFF, Version and $FFFF, StrStat]);
  end
  else
    lblDriverStatus.Caption := 'Driver not installed';

  btnMount.Enabled := (Status <> 0);
end;

procedure TFormEncryptedDrive.btnMountClick(Sender: TObject);
var
  Handle: THandle;
  Psw : string;
  i : Integer;
begin
  if Length(edtPassword.Text) = 0 then
  begin
    MessageDlg('Please enter password used for file encryption', mtInformation, [mbOk], 0);
    Exit;
  end
  else
  begin
    Psw := edtPassword.Text;

    i := Length(Psw);
    if i < 16 then
      i := 16; // extend password to 16 bytes

    SetLength(FXORData, i); // extend password to 16 bytes
    for i := 0 to Length(FXORData) - 1 do
      FXORData[i] := Byte(Psw[(i mod Length(Psw)) + 1]);
  end;

  FRootPath := ExcludeTrailingBackslash(ConvertRelativePathToAbsolute(edtRootPath.Text, false));
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

  FCbFs.SerializeAccess := true;
  FCbFs.SerializeEvents := TcbccbfsSerializeEvents.seOnOneWorkerThread;

  FCbFs.Initialize(FGuid);
  FCbFs.CreateStorage;
  FCbFs.MountMedia(0);

  btnMount.Enabled := false;
  btnUnmount.Enabled := true;
  btnAddPoint.Enabled := true;
end;

procedure TFormEncryptedDrive.btnUnmountClick(Sender: TObject);
begin
  FCbFs.UnmountMedia(true);
  FCbFs.DeleteStorage(true);

  lstPoints.Clear;
  btnMount.Enabled := true;
  btnUnmount.Enabled := false;
  btnAddPoint.Enabled := false;
  btnDeletePoint.Enabled := false;
end;

procedure TFormEncryptedDrive.UpdateMountingPoints;
var
  Index: Integer;
begin
  lstPoints.Items.BeginUpdate;
  try
    lstPoints.Clear;
    for Index := 0 to FCbFs.MountingPointCount - 1 do
    begin
      lstPoints.Items.Add(FCbFs.MountingPointName[Index]);
    end;
  finally
    lstPoints.Items.EndUpdate;
  end;
  
  if lstPoints.Items.Count > 0 then
    lstPoints.Selected[lstPoints.Items.Count - 1] := true;
    
  btnDeletePoint.Enabled := lstPoints.SelCount > 0;
end;

function TFormEncryptedDrive.ConvertRelativePathToAbsolute(const path: string; acceptMountingPoint: Boolean): string;
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

function TFormEncryptedDrive.IsDriveLetter(const Path: string): Boolean;
begin
  Result := False;

  if (Path <> '') and (not Path.IsEmpty) then
  begin
    if (Path[1] in ['A'..'Z', 'a'..'z']) and (Length(Path) = 2) and (Path[2] = ':') then
      Result := True;
  end;
end;

procedure TFormEncryptedDrive.btnAddPointClick(Sender: TObject);
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
  
  FCbFs.AddMountingPoint(absolutePath, STGMP_MOUNT_MANAGER, 0);
  UpdateMountingPoints;
end;

procedure TFormEncryptedDrive.btnDeletePointClick(Sender: TObject);
var
  Index: Integer;
begin
  for Index := 0 to lstPoints.Items.Count - 1 do
    if lstPoints.Selected[Index] then
      FCbFs.RemoveMountingPoint(Index, '', 0, 0);

  UpdateMountingPoints;
end;

procedure TFormEncryptedDrive.lstPointsClick(Sender: TObject);
begin
  btnDeletePoint.Enabled := lstPoints.SelCount > 0;
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





