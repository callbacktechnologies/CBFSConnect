unit VirtFile;

interface

uses
  Windows, Messages, SysUtils, Variants, Classes, Graphics, Controls, Forms,
  Dialogs,
  cbccbfs;

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
end;

constructor VirtualFile.Create(Name: string; InitialSize: Integer);
begin
  System.SetLength(FStream, InitialSize);
  AllocationSize := InitialSize;
  FName := Name;
end;

destructor VirtualFile.Destroy;
begin

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

end.
