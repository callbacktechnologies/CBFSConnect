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
unit nfsc;

interface

uses
  SysUtils, Classes, cbcnfs, Windows, IOUtils;

const
  BaseCookie = $12345678; // Define your base cookie here

  S_IFMT = 61440;  // Bit mask for the file type bit field
  S_IFSOCK = 49152;  // Socket
  S_IFLNK = 40960;  // Symbolic link
  S_IFREG = 32768;  // Regular file
  S_IFBLK = 24576;  // Block device
  S_IFDIR = 16384;  // Directory
  S_IFCHR = 8192;  // Character device
  S_IFIFO = 4096;  // FIFO

  S_ISUID = 2048;  // Set user ID bit
  S_ISGID = 1024;  // Set group ID bit
  S_ISVTX = 512;  // Sticky bit
  S_IRWXU = 448;  // Mask for file owner permissions
  S_IRUSR = 256;  // Owner has read permission
  S_IWUSR = 128;  // Owner has write permission
  S_IXUSR = 64;  // Owner has execute permission
  S_IRWXG = 56;  // Mask for group permissions
  S_IRGRP = 32;  // Group has read permission
  S_IWGRP = 16;  // Group has write permission
  S_IXGRP = 8;  // Group has execute permission
  S_IRWXO = 7;  // Mask for permissions for others (not in group)
  S_IROTH = 4;  // Others have read permission
  S_IWOTH = 2;  // Others have write permission
  S_IXOTH = 1;  // Others have execute permission

  FILE_SYNC4 = 2;
  NFS4ERR_IO = 5;
  NFS4ERR_NOENT = 2;
  NFS4ERR_EXIST = 17;

  DIR_MODE = S_IFDIR or S_IRWXU or S_IRGRP or S_IXGRP or S_IROTH or S_IXOTH; // Type -> Directory, Permissions -> 755
  FILE_MODE = S_IFREG or S_IWUSR or S_IRUSR or S_IRGRP or S_IROTH; // Type -> File, Permission -> 644

type
  VirtualFile = class;

  DirectoryEnumerationContext = class
  private
    FFileList: TList;
    FIndex: Integer;

  public
    constructor Create; overload;
    destructor Destroy; override;

    function GetNextFile(var vfile: VirtualFile): Boolean;
    function GetFile(FileName: string; var vfile: VirtualFile): Boolean; overload;
    function GetFile(Index: integer; var vfile: VirtualFile): Boolean; overload;
    function GetCount: integer;
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
  public
    constructor Create(Name: string; Attrs: Integer); overload;
    destructor Destroy; override;

    procedure Rename(NewName: string);
    procedure Remove;
    procedure SetAllocationSize(Value: Int64);
    procedure Write(WriteBuf: TBytes; Position: Int64; BytesToWrite: Integer; var BytesWritten: Cardinal);
    procedure Read(var ReadBuf: TBytes; Position: Int64; BytesToRead: Integer; var BytesRead: Cardinal);
    procedure AddFile(vfile: VirtualFile);

    //property
    property AllocationSize: Int64 read FAllocationSize write SetAllocationSize;
    property Size: Int64 read FSize write FSize;
    property Name: string read FName write FName;
    property CreationTime: TDateTime read FCreationTime write FCreationTime;
    property LastAccessTime: TDateTime read FLastAccessTime write FLastAccessTime;
    property LastWriteTime: TDateTime read FLastWriteTime write FLastWriteTime;
    property Mode: DWORD read FAttributes write FAttributes;
    property Context: DirectoryEnumerationContext read FEnumCtx;
    property Parent: VirtualFile read FParent write FParent;
  end;

  TMemDriveNFS = class
  private
    FNFS: TcbcNFS;

    procedure OnAccess(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      var Access: Integer;
      var Supported: Integer;
      var Result: Integer
    );

    procedure OnChmod(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      FileContext: Pointer;
      Mode: Integer;
      var Result: Integer
    );

    procedure OnChown(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      FileContext: Pointer;
      const User: String;
      const Group: String;
      var Result: Integer
    );

    procedure OnClose(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      var FileContext: Pointer;
      var Result: Integer
    );

    procedure OnCommit(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      FileContext: Pointer;
      Offset: Int64;
      Count: Integer;
      var Result: Integer
    );

    procedure OnConnected(
      Sender: TObject;
      ConnectionId: Integer;
      StatusCode: Integer;
      const Description: String
    );

    procedure OnConnectionRequest(
      Sender: TObject;
      const Address: String;
      Port: Integer;
      var Accept: Boolean
    );

    procedure OnDisconnected(
      Sender: TObject;
      ConnectionId: Integer;
      StatusCode: Integer;
      const Description: String
    );

    procedure OnError(
      Sender: TObject;
      ConnectionId: Integer;
      ErrorCode: Integer;
      const Description: String
    );

    procedure OnGetAttr(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      FileContext: Pointer;
      var FileID : Int64;
      var Mode: Integer;
      var User: String;
      var Group: String;
      var LinkCount: Integer;
      var Size: Int64;
      var ATime: TDateTime;
      var MTime: TDateTime;
      var CTime: TDateTime;
      var NFSHandleHex: String;
      var Result: Integer
    );

    procedure OnLock(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      FileContext: Pointer;
      LockType: Integer;
      var LockOffset: Int64;
      var LockLen: Int64;
      Test: Boolean;
      var Result: Integer
    );

    procedure OnLog(
      Sender: TObject;
      ConnectionId: Integer;
      LogLevel: Integer;
      const Message: String;
      const LogType: String
    );

    procedure OnLookup(
      Sender: TObject;
      ConnectionId: Integer;
      const Name: String;
      const Path: String;
      var Result: Integer
    );

    procedure OnMkDir(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      var Result: Integer
    );

    procedure OnOpen(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      ShareAccess: Integer;
      ShareDeny: Integer;
      CreateMode: Integer;
      OpenType: Integer;
      var FileContext: Pointer;
      var Result: Integer
    );

    procedure OnRead(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      FileContext: Pointer;
      Buffer: String;
      BufferB: TBytes;
      var Count: Integer;
      Offset: Int64;
      var Eof: Boolean;
      var Result: Integer
    );

    procedure OnReadDir(
      Sender: TObject;
      ConnectionId: Integer;
      var FileContext: Pointer;
      const Path: String;
      Cookie: Int64;
      var Result: Integer
    );

    procedure OnRename(
      Sender: TObject;
      ConnectionId: Integer;
      const OldPath: String;
      const NewPath: String;
      var Result: Integer
    );


     procedure OnRmDir(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      var Result: Integer
    );

    procedure OnTruncate(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      FileContext: Pointer;
      Size: Int64;
      var Result: Integer
    );

    procedure OnUnlink(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      var Result: Integer
    );

    procedure OnUnlock(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      FileContext: Pointer;
      LockType: Integer;
      LockOffset: Int64;
      LockLen: Int64;
      var Result: Integer
    );

    procedure OnUTime(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      FileContext: Pointer;
      ATime: TDateTime;
      MTime: TDateTime;
      var Result: Integer
    );

    procedure OnWrite(
      Sender: TObject;
      ConnectionId: Integer;
      const Path: String;
      FileContext: Pointer;
      Offset: Int64;
      const Buffer: String;
      const BufferB: TBytes;
      var Count: Integer;
      var Stable: Integer;
      var Result: Integer
    );

    function FindVirtualFile(FileName: string; var vfile: VirtualFile): Boolean;
    function FindVirtualDirectory(FileName: string; var vfile: VirtualFile): Boolean;
    function GetParentVirtualDirectory(FileName: string; var vfile: VirtualFile): Boolean;
    function ConvertRelativePathToAbsolute(const path: string; acceptMountingPoint: Boolean): string;
    function IsDriveLetter(const path: string): Boolean;
  public
    constructor Create;
    destructor Destroy;

    property Server: TcbcNFS read FNFS;
  end;

procedure Main;

var
  FDiskContext: VirtualFile; // Declare the global variable

implementation

function NFSExtractFilePath(const FileName: string): string;
var
  i: Integer;
begin
  Result := '';
  for i := Length(FileName) downto 1 do
    if FileName[i] = '/' then
    begin
      Result := Copy(FileName, 1, i);
      Break;
    end;
end;

function NFSExtractFileName(const FileName: string): string;
var
  i: Integer;
begin
  Result := FileName;
  for i := Length(FileName) downto 1 do
    if FileName[i] = '/' then
    begin
      Result := Copy(FileName, i + 1, MaxInt);
      Break;
    end;
end;

{ DirectoryEnumerationContext }

constructor DirectoryEnumerationContext.Create;
begin
  FFileList := TList.Create;
  FIndex := 0;
end;

destructor DirectoryEnumerationContext.Destroy;
var
  i: integer;
begin
  for i := 0 to FFileList.Count - 1 do
  begin
    VirtualFile(FFileList[i]).Free;
  end;

  FFileList.Free;
  inherited;
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

function DirectoryEnumerationContext.GetCount : integer;
begin
  Result := FFileList.Count;
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

constructor VirtualFile.Create(Name: string; Attrs: Integer);
var NowTime : TDateTime;
begin
  AllocationSize := 0;
  Mode := Attrs;
  Size := 0;

  NowTime := Now();

  FName := Name;

  CreationTime := NowTime;
  LastAccessTime := NowTime;
  LastWriteTime := NowTime;

  FEnumCtx := DirectoryEnumerationContext.Create;
end;

destructor VirtualFile.Destroy;
begin
  FEnumCtx.Free;
  inherited;
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
  if Assigned(FEnumCtx) then
  begin
    FEnumCtx.AddFile(vfile);
    vfile.Parent := self;
  end;
end;

procedure VirtualFile.Write(WriteBuf: TBytes; Position: Int64; BytesToWrite: Integer; var BytesWritten: Cardinal);
begin
  if (Size - Position) < BytesToWrite then
    Size := Position + BytesToWrite;

  if FAllocationSize < Size then
    System.SetLength(FStream, Size);

  Move(WriteBuf[0], FStream[Position], BytesToWrite);
  BytesWritten := BytesToWrite;
  FAllocationSize := System.Length(FStream);
end;

procedure VirtualFile.Read(var ReadBuf: TBytes; Position: Int64; BytesToRead: Integer; var BytesRead: Cardinal);
var
  MaxRead: Int64;
begin
  if (System.Length(FStream) - Position) < BytesToRead then
  begin
    MaxRead := System.Length(FStream) - Position;
  end
  else
    MaxRead :=  BytesToRead;

  Move(FStream[Position], ReadBuf[0], MaxRead);
  BytesRead := MaxRead;
end;

{ TMemDriveNFS }

constructor TMemDriveNFS.Create;
begin
  FNFS := TcbcNFS.Create(nil);
  FNFS.Config('LogLevel=4');

  FNFS.OnAccess := OnAccess;
  FNFS.OnChmod := OnChmod;
  FNFS.OnChown := OnChown;
  FNFS.OnClose := OnClose;
  FNFS.OnCommit := OnCommit;
  FNFS.OnConnected := OnConnected;
  FNFS.OnConnectionRequest := OnConnectionRequest;
  FNFS.OnDisconnected := OnDisconnected;
  FNFS.OnError := OnError;
  FNFS.OnGetAttr := OnGetAttr;
  FNFS.OnLock := OnLock;
  FNFS.OnLog := OnLog;
  FNFS.OnLookup := OnLookup;
  FNFS.OnMkDir := OnMkDir;
  FNFS.OnOpen := OnOpen;
  FNFS.OnRead := OnRead;
  FNFS.OnReadDir := OnReadDir;
  FNFS.OnRename := OnRename;
  FNFS.OnRmDir := OnRmDir;
  FNFS.OnTruncate := OnTruncate;
  FNFS.OnUnlink := OnUnlink;
  FNFS.OnUnlock := OnUnlock;
  FNFS.OnUTime := OnUTime;
  FNFS.OnWrite := OnWrite;
end;

function TMemDriveNFS.FindVirtualFile(FileName: string; var vfile: VirtualFile): Boolean;
var
  root: VirtualFile;
  I, Len: Integer;
  Buffer: array of Char;
begin
  Result := true;
  root := FDiskContext;

  if FileName = '/' then
  begin
    vfile := FDiskContext;
    Exit;
  end;

  Len := Length(FileName);
  SetLength(Buffer, Len+1);
  for I := 0 to Len-1 do
  begin
    Buffer[I] := FileName[I + 1];
    if Buffer[I] = '/' then
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

function TMemDriveNFS.GetParentVirtualDirectory(FileName: string; var vfile: VirtualFile): Boolean;
var
  Dir : string;
begin
  Dir := NFSExtractFilePath(FileName);
  Result := FindVirtualFile(Dir, vfile) and ((vfile.Mode and S_IFDIR) <> 0);
end;

function TMemDriveNFS.FindVirtualDirectory(FileName: string; var vfile: VirtualFile): Boolean;
begin
  Result := FindVirtualFile(FileName, vfile) and ((vfile.Mode and S_IFDIR) <> 0);
end;

function TMemDriveNFS.ConvertRelativePathToAbsolute(const path: string; acceptMountingPoint: Boolean): string;
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
        WriteLn('The network folder "' + path + '" format cannot be equal to the Network Mounting Point');
        Exit;
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
        WriteLn('The path "' + res + '" format cannot be equal to the Drive Letter');
        Exit;
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

function TMemDriveNFS.IsDriveLetter(const path: string): Boolean;
begin
  Result := False;

  if (path <> '') and (not path.IsEmpty) then
  begin
    if (path[1] in ['A'..'Z', 'a'..'z']) and (Length(path) = 2) and (path[2] = ':') then
      Result := True;
  end;
end;

destructor TMemDriveNFS.Destroy;
begin
  if FNFS <> nil then
  begin
    FNFS.Free;
    FNFS := nil;
  end;
end;

procedure TMemDriveNFS.OnAccess(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  var Access: Integer;
  var Supported: Integer;
  var Result: Integer);
begin
  Writeln('FireAccess: ' + Path);
end;

procedure TMemDriveNFS.OnChmod(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  FileContext: Pointer;
  Mode: Integer;
  var Result: Integer);
begin
  Writeln('FireChmod ' + Path);
end;

procedure TMemDriveNFS.OnChown(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  FileContext: Pointer;
  const User: String;
  const Group: String;
  var Result: Integer);
begin
  Writeln('FireChown: ' + Path);
end;

procedure TMemDriveNFS.OnClose(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  var FileContext: Pointer;
  var Result: Integer);
begin
  Writeln('FireClose: ' + Path);
end;

procedure TMemDriveNFS.OnCommit(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  FileContext: Pointer;
  Offset: Int64;
  Count: Integer;
  var Result: Integer);
begin
  Writeln('FireCommit: ' + Path);
end;

procedure TMemDriveNFS.OnConnected(
  Sender: TObject;
  ConnectionId: Integer;
  StatusCode: Integer;
  const Description: String);
begin
  Writeln('Client connected: ' + IntToStr(StatusCode) + ' : ' + Description);
end;

procedure TMemDriveNFS.OnConnectionRequest(
  Sender: TObject;
  const Address: String;
  Port: Integer;
  var Accept: Boolean);
begin
  Accept := True;
end;

procedure TMemDriveNFS.OnDisconnected(
  Sender: TObject;
  ConnectionId: Integer;
  StatusCode: Integer;
  const Description: String);
begin
  Writeln('Client disconnected: ' + IntToStr(StatusCode) + ' : ' + Description);
end;

procedure TMemDriveNFS.OnError(
  Sender: TObject;
  ConnectionId: Integer;
  ErrorCode: Integer;
  const Description: String);
begin
  Writeln('Error: ' + IntToStr(ErrorCode) + ' : ' + Description);
end;

procedure TMemDriveNFS.OnGetAttr(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  FileContext: Pointer;
  var FileID : Int64;
  var Mode: Integer;
  var User: String;
  var Group: String;
  var LinkCount: Integer;
  var Size: Int64;
  var ATime: TDateTime;
  var MTime: TDateTime;
  var CTime: TDateTime;
  var NFSHandleHex: String;
  var Result: Integer);
var
  vfile: VirtualFile;
begin
  Writeln('FireGetAttr: ', Path);
  Result := NFS4ERR_NOENT;

  if FindVirtualFile(Path, vfile) then
  begin
    Result := 0;

    LinkCount := 1;
    Group := '0';
    User := '0';
    Mode := vfile.Mode;
    Size := vfile.Size;

    CTime := vfile.CreationTime;
    MTime := vfile.LastWriteTime;
    ATime := vfile.LastAccessTime;
  end;
end;

procedure TMemDriveNFS.OnLock(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  FileContext: Pointer;
  LockType: Integer;
  var LockOffset: Int64;
  var LockLen: Int64;
  Test: Boolean;
  var Result: Integer);
begin
  Writeln('FireLock: ' + Path);
end;

procedure TMemDriveNFS.OnLog(
  Sender: TObject;
  ConnectionId: Integer;
  LogLevel: Integer;
  const Message: String;
  const LogType: String);
begin
  WriteLn('[' + IntToStr(LogLevel) + '] ' + Message);
end;

procedure TMemDriveNFS.OnLookup(
  Sender: TObject;
  ConnectionId: Integer;
  const Name: String;
  const Path: String;
  var Result: Integer);
var
  vfile: VirtualFile;
begin
  Writeln('FireLookup: ' + Path);

  if FindVirtualDirectory(Path, vfile) then
    Exit;

  if FindVirtualFile(Path, vfile) then
    Exit;

  Result := NFS4ERR_NOENT;
end;

procedure TMemDriveNFS.OnMkDir(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  var Result: Integer);
var
  vfile, vdir: VirtualFile;
begin
  Writeln('FireMkDir: ' + Path);

  if FindVirtualFile(Path, vfile) then
  begin
    Result := NFS4ERR_EXIST;
    Exit;
  end;

  if not GetParentVirtualDirectory(Path, vdir) then
  begin
    Result := NFS4ERR_NOENT;
    Exit;
  end;

  vfile := VirtualFile.Create(NFSExtractFileName(Path), DIR_MODE);
  vfile.Size := 4096;

  vdir.AddFile(vfile);
  Result := 0;
end;

procedure TMemDriveNFS.OnOpen(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  ShareAccess: Integer;
  ShareDeny: Integer;
  CreateMode: Integer;
  OpenType: Integer;
  var FileContext: Pointer;
  var Result: Integer);
var
  vfile, vdir: VirtualFile;
begin
  Writeln('FireOpen: ' + Path + ', open type: ' + IntToStr(OpenType));

  if OpenType = 1 then
  begin
    if FindVirtualFile(Path, vfile) then
    begin
      Result := NFS4ERR_EXIST;
      Exit;
    end;
    if not GetParentVirtualDirectory(Path, vdir) then
    begin
      Result := NFS4ERR_NOENT;
      Exit;
    end;

    vfile := VirtualFile.Create(NFSExtractFileName(Path), FILE_MODE);
    vdir.AddFile(vfile);
  end
  else
  begin
    if not FindVirtualFile(Path, vfile) then
      Result := NFS4ERR_NOENT
    else
      vfile.LastAccessTime := Now;
  end;
end;

procedure TMemDriveNFS.OnRead(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  FileContext: Pointer;
  Buffer: String;
  BufferB: TBytes;
  var Count: Integer;
  Offset: Int64;
  var Eof: Boolean;
  var Result: Integer);
var
  vfile: VirtualFile;
  Read: Cardinal;
begin
  Writeln('FireRead: ' + Path);

  if FindVirtualFile(Path, vfile) then
  begin
    if Offset >= vfile.Size then
    begin
      Count := 0;
      Eof := true;
      Exit;
    end;

    vfile.Read(BufferB, Offset, Count, Read);

    if Offset + Read = vfile.Size then
      Eof := true;

    Count := Read;
  end
  else
    Result := NFS4ERR_NOENT;
end;

procedure TMemDriveNFS.OnReadDir(
  Sender: TObject;
  ConnectionId: Integer;
  var FileContext: Pointer;
  const Path: String;
  Cookie: Int64;
  var Result: Integer);
var
  vdir, vfile: VirtualFile;
  currCookie, readOffset: integer;
  count, i: Integer;
begin
  WriteLn('FireReadDir: ' + Path);

  readOffset := 0;
  currCookie := BaseCookie;

  if Cookie <> 0 then
  begin
    readOffset := Cookie - BaseCookie + 1;
    currCookie := Cookie + 1;
  end;

  if FindVirtualDirectory(Path, vdir) then
  begin
    count := vdir.Context.GetCount;

    for i := readOffset to count - 1 do
    begin
      vdir.Context.GetFile(i, vfile);

      FNFS.FillDir(ConnectionId,
        vfile.Name,
        0,
        currCookie,
        vfile.Mode, '0', '0', 1,
        vfile.Size,
        vfile.LastAccessTime,
        vfile.LastWriteTime,
        vfile.CreationTime);

      Inc(currCookie);
    end;
  end
  else
    Result := NFS4ERR_NOENT;
end;

procedure TMemDriveNFS.OnRename(
  Sender: TObject;
  ConnectionId: Integer;
  const OldPath: String;
  const NewPath: String;
  var Result: Integer);
var
  voldfile, vnewfile, vnewparent: VirtualFile;
begin
  Writeln('FireRename: ' + OldPath + ' -> ' + NewPath);

  if FindVirtualFile(OldPath, voldfile) then
  begin
    if FindVirtualFile(NewPath, vnewfile) then
    begin
      vnewfile.Remove;
      vnewfile.Free;
    end;

    if GetParentVirtualDirectory(NewPath, vnewparent) then
    begin
      voldfile.Remove;
      voldfile.Rename(NFSExtractFileName(NewPath));
      vnewparent.AddFile(voldfile);
    end
    else
      Result := NFS4ERR_NOENT;
  end
  else
    Result := NFS4ERR_NOENT;
end;

procedure TMemDriveNFS.OnRmDir(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  var Result: Integer);
var
  vfile: VirtualFile;
begin
  Writeln('FireRmDir: ' + Path);

  if FindVirtualFile(Path, vfile) then
  begin
    vfile.Remove;
    vfile.Free;
  end
  else
    Result := NFS4ERR_NOENT;
end;

procedure TMemDriveNFS.OnTruncate(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  FileContext: Pointer;
  Size: Int64;
  var Result: Integer);
var
  vfile: VirtualFile;
begin
  Writeln('FireTruncate: ' + Path);

  if FindVirtualFile(Path, vfile) then
    vfile.Size := Size
  else
    Result := NFS4ERR_NOENT;
end;

procedure TMemDriveNFS.OnUnlink(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  var Result: Integer);
var
  vfile: VirtualFile;
begin
  Writeln('FireUnlink: ' + Path);

  if FindVirtualFile(Path, vfile) then
  begin
    vfile.Remove;
    vfile.Free;
  end
  else
    Result := NFS4ERR_NOENT;
end;

procedure TMemDriveNFS.OnUnlock(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  FileContext: Pointer;
  LockType: Integer;
  LockOffset: Int64;
  LockLen: Int64;
  var Result: Integer);
begin
  Writeln('FireUnlock: ' + Path);
end;

procedure TMemDriveNFS.OnUTime(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  FileContext: Pointer;
  ATime: TDateTime;
  MTime: TDateTime;
  var Result: Integer);
var
  vfile: VirtualFile;
begin
  Writeln('FireUTime: ' + Path);

  if FindVirtualFile(Path, vfile) then
  begin
    if ATime <> 0 then
      vfile.LastAccessTime := ATime;
    if MTime <> 0 then
      vfile.LastWriteTime := MTime;
  end
  else
    Result := NFS4ERR_NOENT;
end;

procedure TMemDriveNFS.OnWrite(
  Sender: TObject;
  ConnectionId: Integer;
  const Path: String;
  FileContext: Pointer;
  Offset: Int64;
  const Buffer: String;
  const BufferB: TBytes;
  var Count: Integer;
  var Stable: Integer;
  var Result: Integer);
var
  vfile: VirtualFile;
  Written: Cardinal;
begin
  Writeln('FireWrite: ' + Path);

  if FindVirtualFile(Path, vfile) then
  begin
    vfile.AllocationSize := Offset + Count;
    vfile.Write(BufferB, 0, Count, Written);

    Count := Written;
    Stable := FILE_SYNC4;
  end
  else
    Result := NFS4ERR_NOENT;
end;

procedure Banner;
begin
  WriteLn('CBFS Connect Copyright (c) Callback Technologies, Inc.');
  WriteLn;
  WriteLn('This demo shows how to create a virtual drive that is stored in memory using the NFS component.');
  WriteLn;
end;

procedure Usage;
begin
  WriteLn('Usage: nfsc [local port or - for default]');
end;

procedure Main;
var
  NFS: TMemDriveNFS;
  port: Integer;
  sPort: string;
  vfile: VirtualFile;
begin
  // default NFS port
  port := 2049;

  Banner;
  if ParamCount < 1 then
  begin
    Usage;
    Exit;
  end;

  sPort := ParamStr(1);
  if sPort <> '-' then
    port := StrToInt(sPort);

  NFS := TMemDriveNFS.Create;
  try
    if not Assigned(FDiskContext) then
    begin
      FDiskContext := VirtualFile.Create('/', DIR_MODE);

      vfile := VirtualFile.Create('test', DIR_MODE);
      FDiskContext.AddFile(vfile);
    end;

    NFS.Server.LocalPort := port;
    NFS.Server.StartListening;

    WriteLn('NFS server started on port ' + IntToStr(port));
    WriteLn('Press <Enter> to stop the server');

    while True do
    begin
      NFS.Server.DoEvents;
    end;

    NFS.Server.StopListening;
  finally
    NFS.Free;

    if Assigned(FDiskContext) then
      FDiskContext.Free;
  end;
end;

end.


