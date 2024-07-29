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
unit MemDriveF;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms,
  Dialogs, StdCtrls,
  IOUtils, RegularExpressions, 
  cbccore, cbcconstants, cbccbfs,
  VirtFile;

const
  SERVICE_STOPPED = 1;
  SERVICE_START_PENDING = 2;
  SERVICE_STOP_PENDING = 3;
  SERVICE_RUNNING = 4;
  SERVICE_CONTINUE_PENDING = 5;
  SERVICE_PAUSE_PENDING = 6;
  SERVICE_PAUSED = 7;

  SECTOR_SIZE = 512;

  ERROR_NOT_A_REPARSE_POINT = 4390;
  ERROR_REPARSE_ATTRIBUTE_CONFLICT = 4391;
  ERROR_INVALID_REPARSE_DATA = 4392;
  ERROR_REPARSE_TAG_INVALID = 4393;
  ERROR_REPARSE_TAG_MISMATCH = 4394;

  FSCTL_SET_REPARSE_POINT = $000900A4;
  FSCTL_GET_REPARSE_POINT = $000900A8;
  FSCTL_DELETE_REPARSE_POINT = $000900AC;

  REPARSE_MOUNTPOINT_HEADER_SIZE = 8;
  SYMLINK_FLAG_RELATIVE = 1;
  IO_REPARSE_TAG_SYMLINK = $A000000C;

type
  ENUM_INFO = record
    vfile: VirtualFile;
    ExactMatch: Boolean;
    Index: Integer;
  end;

type
   PENUM_INFO = ^ENUM_INFO;

type
  TFormMemDrive = class(TForm)
    lblIntro: TLabel;
    gbDriver: TGroupBox;
    lblDriverStatus: TLabel;
    lblDriver: TLabel;
    btnInstall: TButton;
    btnUninstall: TButton;
    gbMedia: TGroupBox;
    lblMedia: TLabel;
    btnMount: TButton;
    btnUnmount: TButton;
    gbMountingPoints: TGroupBox;
    lblMountingPoints: TLabel;
    lblMountingPoint: TLabel;
    lblPoints: TLabel;
    edtMountingPoint: TEdit;
    btnAddPoint: TButton;
    btnDeletePoint: TButton;
    lstPoints: TListBox;
    dlgOpenDrv: TOpenDialog;
    procedure lstPointsClick(Sender: TObject);
  private
    FCbFs: TcbcCBFS;
    FDiskContext: VirtualFile;
    procedure AskForReboot(isInstall: Boolean);
    procedure UpdateDriverStatus;
    procedure UpdateMountingPoints;
    //support routines
    function FindVirtualFile(FileName: string; var vfile: VirtualFile): Boolean;
    function FindVirtualDirectory(FileName: string; var vfile: VirtualFile): Boolean;
    function GetParentVirtualDirectory(FileName: string; var vfile: VirtualFile): Boolean;
    function CalculateFolderSize(var root: VirtualFile): Int64;
    function ConvertRelativePathToAbsolute(const path: string; acceptMountingPoint: Boolean): string;
    function IsDriveLetter(const path: string): Boolean;
    function CreateReparsePoint(SourcePath, ReparsePath: string): Boolean;
  published

    procedure CBCanFileBeDeleted(
      Sender: TObject;
      const FileName: String;
      const HandleInfo: Int64;
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
      const PendingDeletion: Boolean;
      const HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBCreateFile(
      Sender: TObject;
      const FileName: String;
      const DesiredAccess: Integer;
      const FileAttributes: Integer;
      const ShareMode: Integer;
      const NTCreateDisposition: Integer;
      const NTDesiredAccess: Integer;
      const FileInfo: Int64;
      const HandleInfo: Int64;
      var Reserved: Boolean;
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
      const CaseSensitive: Boolean;
      const Restart: Boolean;
      const RequestedInfo: Integer;
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
      var EaSize: Integer;
      const HandleInfo: Int64;
      var DirectoryContext: Pointer;
      var HandleContext: Pointer;
      var EnumerationContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBReadFile(
      Sender: TObject;
      const FileName: String;
      const Position: Int64;
      Buffer: Pointer;
      const BytesToRead: Int64;
      var BytesRead: Int64;
      const HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBWriteFile(
      Sender: TObject;
      const FileName: String;
      const Position: Int64;
      Buffer: Pointer;
      const BytesToWrite: Int64;
      var BytesWritten: Int64;
      const HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBGetFileInfo(
      Sender: TObject;
      const FileName: String;
      const RequestedInfo: Integer;
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
      var EaSize: Integer;
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
      const DesiredAccess: Integer;
      const FileAttributes: Integer;
      const ShareMode: Integer;
      const NTCreateDisposition: Integer;
      const NTDesiredAccess: Integer;
      const FileInfo: Int64;
      const HandleInfo: Int64;
      var Reserved: Boolean;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBRenameOrMoveFile(
      Sender: TObject;
      const FileName: String;
      const NewFileName: String;
      const HandleInfo: Int64;
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
      const FileAttributes: Integer;
      const EventOrigin: Integer;
      const HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBSetAllocationSize(
      Sender: TObject;
      const FileName: String;
      const AllocationSize: Int64;
      const HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBSetFileSize(
      Sender: TObject;
      const FileName: String;
      const Size: Int64;
      const HandleInfo: Int64;
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
    procedure CBSetReparsePoint(
      Sender: TObject;
      const FileName: String;
      const ReparseTag: Int64;
      ReparseBuffer: Pointer;
      const ReparseBufferLength: Integer;
      const HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBGetReparsePoint(
      Sender: TObject;
      const FileName: String;
      ReparseBuffer: Pointer;
      var ReparseBufferLength: Integer;
      const HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBDeleteReparsePoint(
      Sender: TObject;
      const FileName: String;
      ReparseBuffer: Pointer;
      const ReparseBufferLength: Integer;
      const HandleInfo: Int64;
      var FileContext: Pointer;
      var HandleContext: Pointer;
      var ResultCode: Integer
    );
    procedure CBUnmount(
      Sender: TObject;
      var ResultCode: Integer
    );

    procedure btnMountClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure btnInstallClick(Sender: TObject);
    procedure btnUninstallClick(Sender: TObject);
    procedure btnUnmountClick(Sender: TObject);
    procedure btnAddPointClick(Sender: TObject);
    procedure btnDeletePointClick(Sender: TObject);
  end;

var
  FormMemDrive: TFormMemDrive;

const
  FGuid = '{713CC6CE-B3E2-4fd9-838D-E28F558F6866}';

  FILE_WRITE_ATTRIBUTES     = $0100; // all

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

procedure TFormMemDrive.CBCanFileBeDeleted(
  Sender: TObject;
  const FileName: String;
  const HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var CanBeDeleted: Boolean;
  var ResultCode: Integer);
var
  vfile: VirtualFile;
begin
  CanBeDeleted := FindVirtualFile(FileName, vfile);
  if CanBeDeleted then
  begin
    if ((vfile.FileAttributes and FILE_ATTRIBUTE_DIRECTORY) <> 0) and ((vfile.FileAttributes and FILE_ATTRIBUTE_REPARSE_POINT) = 0) and vfile.Context.GetFile(0, vfile) then
    begin
      CanBeDeleted := false;
      ResultCode := ERROR_DIR_NOT_EMPTY;
    end;
  end
  else
    ResultCode := ERROR_FILE_NOT_FOUND;
end;

procedure TFormMemDrive.CBCloseDirectoryEnumeration(
  Sender: TObject;
  const DirectoryName: String;
  var DirectoryContext: Pointer;
  EnumerationContext: Pointer;
  var ResultCode: Integer);
begin
  try
    if EnumerationContext <> nil then
      FreeMem(Pointer(EnumerationContext));
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBCloseFile(
  Sender: TObject;
  const FileName: String;
  const PendingDeletion: Boolean;
  const HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
begin
  // FileContext := 0;
end;

procedure TFormMemDrive.CBCreateFile(
  Sender: TObject;
  const FileName: String;
  const DesiredAccess: Integer;
  const FileAttributes: Integer;
  const ShareMode: Integer;
  const NTCreateDisposition: Integer;
  const NTDesiredAccess: Integer;
  const FileInfo: Int64;
  const HandleInfo: Int64;
  var Reserved: Boolean;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  vfile, vdir: VirtualFile;
  systime: SYSTEMTIME;
  ft: TDateTime;
begin
  try
    if not GetParentVirtualDirectory(ExtractFilePath(FileName), vdir) then
    begin
      ResultCode := ERROR_PATH_NOT_FOUND;
      Exit;
    end;

    vfile := VirtualFile.Create(ExtractFileName(FileName));
    vfile.FileAttributes := FileAttributes and $FFFF; // FileAttributes contains creation flags as well, which we need to strip
    GetSystemTime(systime);

    ft := SystemTimeToDateTime(systime);
    vfile.CreationTime := ft;
    vfile.LastAccessTime := ft;
    vfile.LastWriteTime := ft;
    vdir.AddFile(vfile);
    FileContext := vfile;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBDeleteFile(
  Sender: TObject;
  const FileName: String;
  var FileContext: Pointer;
  var ResultCode: Integer);
var
  vfile: VirtualFile;
begin
  try
    vfile := nil;
    if FindVirtualFile(FileName, vfile) then
    begin
      vfile.Remove;
      vfile.Destroy;
    end
    else
      ResultCode := ERROR_FILE_NOT_FOUND;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBEnumerateDirectory(
  Sender: TObject;
  const DirectoryName: String;
  const Mask: String;
  const CaseSensitive: Boolean;
  const Restart: Boolean;
  const RequestedInfo: Integer;
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
  var EaSize: Integer;
  const HandleInfo: Int64;
  var DirectoryContext: Pointer;
  var HandleContext: Pointer;
  var EnumerationContext: Pointer;
  var ResultCode: Integer);
var
  vfile, vdir: VirtualFile;
  pinfo: PENUM_INFO;
  ResetEnumeration: Boolean;
  ExactMatch: Boolean;
begin
  try
    FileFound := false;
    ResetEnumeration := false;
    ExactMatch := true;

    ExactMatch := not ((Pos('*', Mask) > 0) or (Pos('?', Mask) > 0));

    if (Restart or (EnumerationContext = nil)) then
      ResetEnumeration := true;

    if Restart and (EnumerationContext <> nil) then
    begin
      FreeMem(Pointer(EnumerationContext));
      EnumerationContext := nil;
    end;

    if EnumerationContext = nil then
    begin
      if not FindVirtualDirectory(DirectoryName, vdir) then
      begin
        ResultCode := ERROR_PATH_NOT_FOUND;
        Exit;
      end;

      pInfo := AllocMem(SizeOf(ENUM_INFO));
      EnumerationContext := pInfo;
      pInfo.vfile := vdir;
      pInfo.ExactMatch := ExactMatch;
    end
    else
    begin
      pInfo := PENUM_INFO(EnumerationContext);
      vdir := pInfo.vfile;
    end;

    if ResetEnumeration then
      pInfo.Index := 0;

    if pInfo.ExactMatch then
      FileFound := (pInfo.Index = 0) and vdir.Context.GetFile(Mask, vfile)
    else
    begin
      while true do
      begin
        FileFound := vdir.Context.GetFile(pInfo.Index, vfile);
      	if (not FileFound) or FCbFs.FileMatchesMask(Mask, vfile.Name, false) then
          break;
        // If the file was picked but its name didn't match the mask, pick the next file
        Inc(pInfo.Index);
      end;
    end;

    if FileFound then
    begin
      FileName        := vfile.Name;
      CreationTime    := vfile.CreationTime;
      LastAccessTime  := vfile.LastAccessTime;
      LastWriteTime   := vfile.LastWriteTime;
      Size            := vfile.Size;
      AllocationSize  := vfile.AllocationSize;
      FileId          := 0;
      Attributes      := vfile.FileAttributes;
      if (vfile.FileAttributes and FILE_ATTRIBUTE_REPARSE_POINT <> 0) and
          FCbFs.UseReparsePoints then
        ReparseTag    := vfile.ReparseTag;
    end;

    Inc(pInfo.Index);
    pInfo.ExactMatch := ExactMatch;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBReadFile(
  Sender: TObject;
  const FileName: String;
  const Position: Int64;
  Buffer: Pointer;
  const BytesToRead: Int64;
  var BytesRead: Int64;
  const HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  vfile: VirtualFile;
  cBytesRead : Cardinal;
  BytesToReadReal: Int64;
begin
  try
    BytesToReadReal := BytesToRead;
    if BytesToRead > $7fffffff then
      BytesToReadReal := $7fffffff;

    vfile := VirtualFile(Pointer(FileContext));
    vfile.Read(Buffer^, Position, BytesToReadReal, cBytesRead);
    BytesRead := cBytesRead;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBWriteFile(
  Sender: TObject;
  const FileName: String;
  const Position: Int64;
  Buffer: Pointer;
  const BytesToWrite: Int64;
  var BytesWritten: Int64;
  const HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  vfile: VirtualFile;
  cBytesWritten : Cardinal;
  BytesToWriteReal: Int64;
begin
  try
    BytesToWriteReal := BytesToWrite;
    if BytesToWrite > $7fffffff then
      BytesToWriteReal := $7fffffff;

    vfile := VirtualFile(Pointer(FileContext));
    BytesWritten := 0;
    vfile.Write(Buffer^, Position, BytesToWriteReal, cBytesWritten);
    BytesWritten := cBytesWritten;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBGetFileInfo(
  Sender: TObject;
  const FileName: String;
  const RequestedInfo: Integer;
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
  var EaSize: Integer;
  var ResultCode: Integer);
var
  vfile: VirtualFile;
begin
  try
    FileExists := false;
    vfile := nil;
    if FindVirtualFile(FileName, vfile) then
    begin
      FileExists      := true;
      CreationTime    := vfile.CreationTime;
      LastAccessTime  := vfile.LastAccessTime;
      LastWriteTime   := vfile.LastWriteTime;
      Size            := vfile.Size;
      AllocationSize  := vfile.AllocationSize;
      FileId          := 0;
      Attributes      := vfile.FileAttributes;
      ReparseTag      := vfile.ReparseTag;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBGetVolumeId(Sender: TObject;
  var VolumeId: Integer; var ResultCode: Integer);
begin
  VolumeID := $12345678;
end;

procedure TFormMemDrive.CBGetVolumeLabel(Sender: TObject;
  var VolumeLabel: String; var ResultCode: Integer);
begin
  VolumeLabel := 'Virtual CbFs Disk';
end;

procedure TFormMemDrive.CBGetVolumeSize(
  Sender: TObject;
  var TotalSectors: Int64;
  var AvailableSectors: Int64;
  var ResultCode: Integer);
var
  status: MEMORYSTATUS;
begin
  try
    GlobalMemoryStatus(status);
    TotalSectors := status.dwTotalVirtual div SECTOR_SIZE;
    AvailableSectors := (status.dwTotalVirtual -
      CalculateFolderSize(FDiskContext) + SECTOR_SIZE div 2)
      div SECTOR_SIZE;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBMount(Sender: TObject; var ResultCode: Integer);
begin
  ;
end;

procedure TFormMemDrive.CBOpenFile(
  Sender: TObject;
  const FileName: String;
  const DesiredAccess: Integer;
  const FileAttributes: Integer;
  const ShareMode: Integer;
  const NTCreateDisposition: Integer;
  const NTDesiredAccess: Integer;
  const FileInfo: Int64;
  const HandleInfo: Int64;
  var Reserved: Boolean;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  vfile: VirtualFile;
begin
  try
    if FileContext = nil then
    begin
      vfile := nil;
      if FindVirtualFile(FileName, vfile) then
        FileContext := vfile
      else
        ResultCode := ERROR_FILE_NOT_FOUND;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBRenameOrMoveFile(
  Sender: TObject;
  const FileName: String;
  const NewFileName: String;
  const HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  vfile, vdir, vnewfile: VirtualFile;
begin
  try
    vfile := nil;
    if not FindVirtualFile(FileName, vfile) then
    begin
      ResultCode := ERROR_FILE_NOT_FOUND;
      Exit;
    end;

    if not FindVirtualDirectory(ExtractFilePath(NewFileName), vdir) then
    begin
      ResultCode := ERROR_PATH_NOT_FOUND;
      Exit;
    end;

    if FindVirtualFile(NewFileName, vnewfile) then
    begin
      if (vnewfile.FileAttributes and FILE_ATTRIBUTE_READONLY) <> 0 then
      begin
        ResultCode := ERROR_ACCESS_DENIED;
        Exit;
      end;

      vnewfile.Remove;
    end;

    vfile.Remove;
    vfile.Rename(ExtractFileName(NewFileName)); // obtain file name from the path;
    vdir.AddFile(vfile);
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBSetFileAttributes(
  Sender: TObject;
  const FileName: String;
  CreationTime: TDateTime;
  LastAccessTime: TDateTime;
  LastWriteTime: TDateTime;
  ChangeTime: TDateTime;
  const FileAttributes: Integer;
  const EventOrigin: Integer;
  const HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  vfile: VirtualFile;
begin
  try
    vfile := VirtualFile(Pointer(FileContext));
    // the case when FileAttributes == 0 indicates that file attributes
    // not changed during this callback
    if FileAttributes <> 0 then
      vfile.FileAttributes := FileAttributes;

    if CreationTime <> ZeroFiletime then
      vfile.CreationTime := CreationTime;

    if LastAccessTime <> ZeroFiletime then
      vfile.LastAccessTime := LastAccessTime;

    if LastWriteTime <> ZeroFiletime then
      vfile.LastWriteTime := LastWriteTime;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBSetAllocationSize(
  Sender: TObject;
  const FileName: String;
  const AllocationSize: Int64;
  const HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  vfile: VirtualFile;
begin
  try
    vfile := VirtualFile(Pointer(FileContext));
    vfile.AllocationSize := AllocationSize;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBSetFileSize(
  Sender: TObject;
  const FileName: String;
  const Size: Int64;
  const HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  vfile: VirtualFile;
begin
  try
    vfile := VirtualFile(Pointer(FileContext));
    vfile.Size := Size;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBIsDirectoryEmpty(
  Sender: TObject;
  const DirectoryName: String;
  var IsEmpty: Boolean;
  var DirectoryContext: Pointer;
  var ResultCode: Integer);
var
  vdir: VirtualFile;
begin
  try
    vdir := nil;
    IsEmpty := false;
    if FindVirtualDirectory(DirectoryName, vdir) then
      IsEmpty := vdir.Context.IsEmpty()
    else
      ResultCode := ERROR_PATH_NOT_FOUND;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBSetReparsePoint(
  Sender: TObject;
  const FileName: String;
  const ReparseTag: Int64;
  ReparseBuffer: Pointer;
  const ReparseBufferLength: Integer;
  const HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
var
  vfile: VirtualFile;
begin
  try
    vfile := VirtualFile(Pointer(FileContext));
    if vfile.ReparsePointBuffer <> nil then
    begin
      if vfile.ReparseTag <>  ReparseTag then
      begin
        ResultCode := ERROR_REPARSE_TAG_MISMATCH;
        exit;
      end;
    end;
    vfile.ReparsePointBuffer := GetMemory(ReparseBufferLength);
    MoveMemory(ReparseBuffer, vfile.ReparsePointBuffer, ReparseBufferLength);
    vfile.ReparseBufferLen := ReparseBufferLength;
    vfile.ReparseTag := ReparseTag;
    vfile.FileAttributes := vfile.FileAttributes or FILE_ATTRIBUTE_REPARSE_POINT;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBGetReparsePoint(
  Sender: TObject;
  const FileName: String;
  ReparseBuffer: Pointer;
  var ReparseBufferLength: Integer;
  const HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
 var
  vfile: VirtualFile;
begin
  try
    vfile := VirtualFile(Pointer(FileContext));
    if vfile = nil then
    begin
        if FindVirtualFile(FileName, vfile) = false then
        begin
          ResultCode := ERROR_FILE_NOT_FOUND;
          exit;
        end;
    end;

    if (vfile.FileAttributes and FILE_ATTRIBUTE_REPARSE_POINT) <>  FILE_ATTRIBUTE_REPARSE_POINT then
    begin
      ResultCode := ERROR_NOT_A_REPARSE_POINT;
      exit;
    end;
    if ReparseBufferLength < vfile.ReparseBufferLen then
    begin
      Move(vfile.ReparsePointBuffer, ReparseBuffer, ReparseBufferLength);
      ResultCode := ERROR_MORE_DATA;
      exit;
    end;
    Move(vfile.ReparsePointBuffer, ReparseBuffer, vfile.ReparseBufferLen);
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBDeleteReparsePoint(
  Sender: TObject;
  const FileName: String;
  ReparseBuffer: Pointer;
  const ReparseBufferLength: Integer;
  const HandleInfo: Int64;
  var FileContext: Pointer;
  var HandleContext: Pointer;
  var ResultCode: Integer);
 var
  vfile: VirtualFile;
begin
  try
    vfile := VirtualFile(Pointer(FileContext));
    if (vfile.FileAttributes and FILE_ATTRIBUTE_REPARSE_POINT) <>  FILE_ATTRIBUTE_REPARSE_POINT then
    begin
      ResultCode := ERROR_NOT_A_REPARSE_POINT;
      exit;
    end;
    if vfile.ReparsePointBuffer <> nil then
    begin
      FreeMem(vfile.ReparsePointBuffer);
      vfile.ReparsePointBuffer := nil;
      vfile.ReparseBufferLen := 0;
      vfile.ReparseTag := 0;
      vfile.FileAttributes := vfile.FileAttributes and not FILE_ATTRIBUTE_REPARSE_POINT;
    end;
  except
    on E : Exception do
      ResultCode := ExceptionToErrorCode(E);
  end;
end;

procedure TFormMemDrive.CBSetVolumeLabel(Sender: TObject;
  const VolumeLabel: String; var ResultCode: Integer);
begin
;
end;

procedure TFormMemDrive.CBUnmount(Sender: TObject; var ResultCode: Integer);
begin
;
end;

procedure TFormMemDrive.FormCreate(Sender: TObject);
begin
  FCbFs := TcbcCBFS.Create(Self);
  FCbFs.Config('SectorSize=' + IntToStr(SECTOR_SIZE));

  FCbFs.OnMount                     := CBMount;
  FCbFs.OnUnmount                   := CBUnmount;
  FCbFs.OnGetVolumeSize             := CBGetVolumeSize;
  FCbFs.OnGetVolumeLabel            := CBGetVolumeLabel;
  FCbFs.OnSetVolumeLabel            := CBSetVolumeLabel;
  FCbFs.OnGetVolumeId               := CBGetVolumeId;
  FCbFs.OnCreateFile                := CBCreateFile;
  FCbFs.OnOpenFile                  := CBOpenFile;
  FCbFs.OnCloseFile                 := CBCloseFile;
  FCbFs.OnGetFileInfo               := CBGetFileInfo;
  FCbFs.OnEnumerateDirectory        := CBEnumerateDirectory;
  FCbFs.OnCloseDirectoryEnumeration := CBCloseDirectoryEnumeration;
  FCbFs.OnSetAllocationSize         := CBSetAllocationSize;
  FCbFs.OnSetFileSize              := CBSetFileSize;
  FCbFs.OnSetFileAttributes         := CBSetFileAttributes;
  FCbFs.OnCanFileBeDeleted          := CBCanFileBeDeleted;
  FCbFs.OnDeleteFile                := CBDeleteFile;
  FCbFs.OnRenameOrMoveFile          := CBRenameOrMoveFile;
  FCbFs.OnReadFile                  := CBReadFile;
  FCbFs.OnWriteFile                 := CBWriteFile;
  FCbFs.OnIsDirectoryEmpty          := CBIsDirectoryEmpty;
  FCbFs.UseReparsePoints            := true;
  FCbFs.OnSetReparsePoint           := CBSetReparsePoint;
  FCbFs.OnGetReparsePoint           := CBGetReparsePoint;
  FCbFs.OnDeleteReparsePoint        := CBDeleteReparsePoint;
  UpdateDriverStatus;
end;

procedure TFormMemDrive.FormDestroy(Sender: TObject);
var
  Index: Integer;
begin
  if FCbFs.Active then
    for Index := FCbFs.MountingPointCount - 1 downto 0 do
      FCbFs.RemoveMountingPoint(Index, '', 0, 0);

  if FCbFs.Active then
    FCbFs.UnmountMedia(true);

  if FCbFs.StoragePresent then
    FCbFs.DeleteStorage(true);

  FCbFs.Free;
end;

procedure TFormMemDrive.AskForReboot(isInstall: Boolean);
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

procedure TFormMemDrive.btnInstallClick(Sender: TObject);
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
        MODULE_DRIVER, INSTALL_REMOVE_OLD_VERSIONS);
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

procedure TFormMemDrive.btnUninstallClick(Sender: TObject);
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

procedure TFormMemDrive.UpdateDriverStatus;
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
    lblDriverStatus.Caption := Format('Driver (ver %d.%d.%d.%d) installed,'#13#10'service %s',
      [Version shr 48, (Version shr 32) and $FFFF, (Version shr 16) and $FFFF, Version and $FFFF, StrStat]);
  end
  else
    lblDriverStatus.Caption := 'Driver not installed';

  btnMount.Enabled := (Status <> 0);
end;

procedure TFormMemDrive.btnMountClick(Sender: TObject);
var
  vDir, vFile: VirtualFile;
  s: string;
  written: cardinal;
begin
  FDiskContext := VirtualFile.Create('\');
  FDiskContext.FileAttributes := FILE_ATTRIBUTE_DIRECTORY;

  vDir := VirtualFile.Create('Sample');
  vDir.FileAttributes := FILE_ATTRIBUTE_DIRECTORY;
  FDiskContext.AddFile(vDir);
  s := 'This is a file header \n---------------------------\n';
  vFile := VirtualFile.Create('TestFile_1.txt');
  written := 0;
  vFile.Write(Pointer(s), 0, Length(s) * SizeOf(Char) , written);
  vdir.AddFile(vFile);

  FCbFs.SerializeAccess := true;
  FCbFs.SerializeEvents := TcbccbfsSerializeEvents.seOnOneWorkerThread;

  FCbFs.Initialize(FGuid);
  FCbFs.CreateStorage;
  FCbFs.MountMedia(0);

  btnMount.Enabled := false;
  btnUnmount.Enabled := true;
  btnAddPoint.Enabled := true;
end;

procedure TFormMemDrive.btnUnmountClick(Sender: TObject);
begin
  FCbFs.UnmountMedia(true);
  FCbFs.DeleteStorage(true);
  FDiskContext.Destroy;
  
  lstPoints.Items.Clear;
  btnMount.Enabled := true;
  btnUnmount.Enabled := false;
  btnAddPoint.Enabled := false;
  btnDeletePoint.Enabled := false;
end;

procedure TFormMemDrive.UpdateMountingPoints;
var
  Index: Integer;
begin
  lstPoints.Items.BeginUpdate;
  try
    lstPoints.Items.Clear;
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

procedure TFormMemDrive.btnAddPointClick(Sender: TObject);
var
  hHandle: Integer;
  S: string;
  w : Integer;
  mp: string;
  root: VirtualFile;
  vFullPathSymLink, vRelativePathSymLink: VirtualFile;
begin
  Screen.Cursor := crHourGlass;
  mp := ConvertRelativePathToAbsolute(edtMountingPoint.Text, True);
  if (mp = '') then
  begin
    MessageDlg('Error: Invalid Mounting Point Path.',
      mtError, [mbOk], 0);
    Exit;
  end;
  FCbFs.AddMountingPoint(mp, STGMP_MOUNT_MANAGER, 0);
  if (Length(mp) <> 2) or (mp[2] <> ':') then
    s := '\\.\' + mp
  else
    s := mp;

  if FindVirtualFile('\', root) then
  begin
    vFullPathSymLink := VirtualFile.Create('full_path_reparse_toTestFile_1.txt');
    root.AddFile(vFullPathSymLink);
    vFullPathSymLink.CreateReparsePoint(s + '\Sample\TestFile_1.txt');
  {
    vRelativePathSymLink := VirtualFile.Create('relative_path_reparse_to_TestFile_1.txt');
    root.AddFile(vRelativePathSymLink);
    vRelativePathSymLink.CreateReparsePoint('.\Sample\TestFile_1.txt');
  }
    //CreateReparsePoint(mp + '\full_path_reparse_to_TestFile_1.txt', mp + '\Sample\TestFile_1.txt');
    CreateReparsePoint(s + '\relative_path_reparse_to_TestFile_1.txt', '.\Sample\TestFile_1.txt');
  end;
  UpdateMountingPoints;
  Screen.Cursor := crDefault;
end;

procedure TFormMemDrive.btnDeletePointClick(Sender: TObject);
var
  Index: Integer;
begin
  Screen.Cursor := crHourGlass;
  for Index := 0 to lstPoints.Items.Count - 1 do
    if lstPoints.Selected[Index] then
      FCbFs.RemoveMountingPoint(Index, '', 0, 0);

  UpdateMountingPoints;
  Screen.Cursor := crDefault;
end;

function TFormMemDrive.FindVirtualFile(FileName: string; var vfile: VirtualFile): Boolean;
var
  root: VirtualFile;
  I, Len: Integer;
  Buffer: array of Char;
begin
  Result := true;
  root := FDiskContext;
  if(FileName = '\') then
  begin
    vfile := FDiskContext;
    Exit;
  end;

  Len := Length(FileName);
  SetLength(Buffer, Len+1);
  for I := 0 to Len-1 do
  begin
    Buffer[I] := FileName[I + 1];
    if Buffer[I] = '\' then
      Buffer[I] := #0;
  end;

  Buffer[Len] := #0;
  vfile := root;
  I := 0;
  while I < Len  do
  begin
    Result := root.Context.GetFile(PChar(@Buffer[I]), vfile);
    if Result then
      root := vfile;

    while(Buffer[I] <> #0) do
      Inc(I);

    Inc(I);
  end;
end;

function TFormMemDrive.GetParentVirtualDirectory(FileName: string; var vfile: VirtualFile): Boolean;
var
  Dir : string;
begin
  Dir := ExtractFilePath(FileName);
  Result := FindVirtualFile(Dir, vfile) and ((vfile.FileAttributes and FILE_ATTRIBUTE_DIRECTORY) <> 0);
end;

function TFormMemDrive.FindVirtualDirectory(FileName: string; var vfile: VirtualFile): Boolean;
begin
  if FindVirtualFile(FileName, vfile) and
     ((vfile.FileAttributes and FILE_ATTRIBUTE_DIRECTORY) <> 0) then
    Result := true
  else
    Result := GetParentVirtualDirectory(FileName, vfile);
end;

function TFormMemDrive.CalculateFolderSize(var root: VirtualFile): Int64;
var
  vfile: VirtualFile;
  DiskSize: Int64;
  Index: Cardinal;
begin
  DiskSize := 0;
  Index := 0;
  vfile := nil;
  while root.Context.GetFile(Index, vfile) do
  begin
    if vfile.FileAttributes and FILE_ATTRIBUTE_DIRECTORY <> 0 then
      DiskSize := DiskSize + CalculateFolderSize(vfile)
    else
      DiskSize := DiskSize + (vfile.AllocationSize + SECTOR_SIZE - 1) and not (SECTOR_SIZE - 1);

    Inc(Index);
  end;

  Result := DiskSize;
end;

function TFormMemDrive.ConvertRelativePathToAbsolute(const path: string; acceptMountingPoint: Boolean): string;
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

function TFormMemDrive.IsDriveLetter(const path: string): Boolean;
begin
  Result := False;

  if (path <> '') and (not path.IsEmpty) then
  begin
    if (path[1] in ['A'..'Z', 'a'..'z']) and (Length(path) = 2) and (path[2] = ':') then
      Result := True;
  end;
end;

procedure TFormMemDrive.lstPointsClick(Sender: TObject);
begin
  btnDeletePoint.Enabled := lstPoints.SelCount > 0;
end;

function TFormMemDrive.CreateReparsePoint(SourcePath, ReparsePath: string): Boolean;
var
  ReparseDataBuffer: PReparseDataBuffer;
  SymLinkTarget: string;
  Flags, Size, dwBytesReturned: DWORD;
  TargetLen, SourceLen: WORD;
  hFile: THandle;
  FileSymLinkTargert: array[0..16384] of Char;
  LinkName: PWideChar;
  resNum: integer;
begin
  if ReparsePath.StartsWith('.') or ReparsePath.StartsWith('..') then
  begin
    SymLinkTarget := ReparsePath;
    Flags := SYMLINK_FLAG_RELATIVE;
  end
  else
  begin
    SymLinkTarget := '\??\' + ReparsePath;
    Flags := 0;
  end;
  TargetLen := SymLinkTarget.Length * 2;
  Size := TargetLen + SizeOf(TReparseDataBuffer);
  GetMem(ReparseDataBuffer, size);
  FillChar(ReparseDataBuffer^, size, 0);
  Move(PChar(SymLinkTarget)^, ReparseDataBuffer.PathBuffer, Length(SymLinkTarget) * SizeOf(Char)); 

  with ReparseDataBuffer^ do
  begin
    ReparseTag := IO_REPARSE_TAG_SYMLINK;
    ReparseDataLength := TargetLen + 12;
    Reserved := 0;
    Flags := Flags;
    SubstituteNameLength := TargetLen;
    PrintNameLength := 0;
    PrintNameOffset := TargetLen;
  end;
 
  hFile := CreateFile(PChar(SourcePath),
    GENERIC_WRITE or GENERIC_READ,
    FILE_SHARE_READ or FILE_SHARE_WRITE,
    nil,
    CREATE_NEW,
    FILE_FLAG_OPEN_REPARSE_POINT or FILE_FLAG_BACKUP_SEMANTICS,
    0);

  dwBytesReturned := 0;
  Result := DeviceIoControl(hFile,
    FSCTL_SET_REPARSE_POINT, ReparseDataBuffer,
    ReparseDataBuffer^.ReparseDataLength + REPARSE_MOUNTPOINT_HEADER_SIZE,
    nil,
    0,
    dwBytesReturned,
    nil);
  CloseHandle(hFile);
end;

initialization

  ZeroFiletime := EncodeDateTime(1601, 1, 1, 0, 0, 0, 0);

end.



