unit uEncrypt;

interface

uses
  Windows, Messages, SysUtils, Variants, Classes, Graphics, Controls, Forms,
  Dialogs,
  cbccbfs;

const
  FILE_OPEN = 1;
  FILE_OPEN_IF = 3;

type
  ByteArray = array of byte;

  TEncryptContext = class
  private
    FFS: TcbcCBFS;
    FBufferSize: Cardinal;
    FBuffer: PAnsiChar;
    FMask : PAnsiChar;
    FCurrentSize: Int64;
    FRefCnt: Integer;
    FInitialized: Boolean;
    FInitialIV: ByteArray;
    FReadOnly: Boolean;

    procedure SetCurrentSize(const Value: Int64);
  public
    FHandle: THandle;

    constructor Create(FS: TcbcCBFS; FileName: string;
      FileAttributes: Cardinal; DesiredAccess: Integer; NTCreateDisposition: Integer;
      New : boolean; var ZeroizeFileContext : Boolean);
    destructor Destroy; override;

    function Read(Position: Int64; var Buffer; BytesToRead: Cardinal; out TotalRead : Cardinal): Integer;
    function Write(Position: Int64; var Buffer; BytesToWrite: Cardinal; out TotalWritten : Cardinal): Integer;
    procedure EncryptBuffer(Position: Int64; Size : integer);
    procedure DecryptBuffer(Position: Int64; Size : integer);
    function IncrementRef: Integer;
    function DecrementRef: Integer;
    procedure CloseFile;

    property CurrentSize: Int64 read FCurrentSize write SetCurrentSize;
    property ReferenceCounter: Integer read FRefCnt write FRefCnt;
    property Initialized: Boolean read FInitialized;
    property ReadOnly: Boolean read FReadOnly;
  end;

function IsWriteRightForAttrRequested(Acc: LongWord): boolean;
function IsReadRightForAttrOrDeleteRequested(Acc: LongWord): boolean;
function IsWriteRightRequested(Acc: LongWord): boolean;

implementation

uses EncryptedDriveF;

function Min(const A, B: integer): integer;
begin
  if A < B then
    Result := A
  else
    Result := B;
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

function IsWriteRightRequested(Acc: LongWord): boolean;
begin
  Result := (Acc and (FILE_WRITE_DATA or FILE_APPEND_DATA or FILE_WRITE_ATTRIBUTES or FILE_WRITE_EA)) <> 0;
end;

{ TEncryptContext }

constructor TEncryptContext.Create(FS: TcbcCBFS; FileName: string;
  FileAttributes: Cardinal; DesiredAccess: Integer; NTCreateDisposition: Integer; New : boolean; var ZeroizeFileContext : Boolean);
var
  SystemInfo: TSystemInfo;
  //SectorsPerCluster, BytesPerSector,
  //NumberOfFreeClusters, TotalNumberOfClusters: Cardinal;
  size_high: LongWord;

  Error : DWORD;
  OSError : EOSError;
  IsStream : boolean;
  Err : integer;
begin
  FRefCnt := 0;
  FBuffer := nil;
  FBufferSize := 0;
  FCurrentSize := 0;
  FInitialized := false;
  ZeroizeFileContext := false;
  FFS := FS;
  FReadOnly := false;
  IsStream := (Pos(':', FileName) >= 1);

  if New then
  begin
    if (FileAttributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
    begin
      if IsStream then
      begin
        OSError := EOSError.Create('Could not open file.');
        OSError.ErrorCode := ERROR_DIRECTORY;
        raise OSError;
      end;

      Win32Check(CreateDirectory(PChar(FormEncryptedDrive.FRootPath + FileName), nil));
      FHandle := CreateFile(PChar(FormEncryptedDrive.FRootPath + FileName),
                             GENERIC_READ or FILE_WRITE_ATTRIBUTES,
                             FILE_SHARE_READ or FILE_SHARE_WRITE or FILE_SHARE_DELETE,
                             nil, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);

    end
    else
    begin
      FHandle := CreateFile(PChar(FormEncryptedDrive.FRootPath + FileName),
        GENERIC_READ or GENERIC_WRITE,
        FILE_SHARE_READ or FILE_SHARE_WRITE or FILE_SHARE_DELETE,
        nil, CREATE_NEW, FileAttributes, 0);
    end;
    
    Win32Check(SetFileAttributes(PChar(FormEncryptedDrive.FRootPath + FileName), FileAttributes and $ffff)); // Attributes contains creation flags as well, which we need to strip
  end
  else
  begin
    if ((FileAttributes and FILE_ATTRIBUTE_DIRECTORY) <> 0) and not IsStream then
    begin
      if (FileName <> '\') and (GetFileAttributes(PChar(FormEncryptedDrive.FRootPath + FileName)) = $FFFFFFFF) then
      begin
        ZeroizeFileContext := true;
        Exit;
      end
      else
      begin
        FHandle := CreateFile(PChar(FormEncryptedDrive.FRootPath + FileName),
                               GENERIC_READ or FILE_WRITE_ATTRIBUTES,
                               FILE_SHARE_READ or FILE_SHARE_WRITE or FILE_SHARE_DELETE,
                               nil, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
        if FHandle = INVALID_HANDLE_VALUE then
        begin
          OSError := EOSError.Create('Could not open file.');
          OSError.ErrorCode := GetLastError;
          raise OSError;
        end;
      end;
    end
    else
    begin
      FHandle := CreateFile(PChar(FormEncryptedDrive.FRootPath + FileName),
                             GENERIC_READ or GENERIC_WRITE, FILE_SHARE_READ or FILE_SHARE_WRITE,
                             nil, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

      if FHandle = INVALID_HANDLE_VALUE then
      begin
        Error := GetLastError;
        if Error = ERROR_ACCESS_DENIED then
        begin
          if not IsReadRightForAttrOrDeleteRequested(DesiredAccess) then
            begin
              if IsWriteRightForAttrRequested(DesiredAccess) or
                ((DesiredAccess and (FILE_WRITE_DATA or FILE_APPEND_DATA or FILE_WRITE_ATTRIBUTES or FILE_WRITE_EA)) <> 0) then
              begin
                FHandle := CreateFile(PChar(FormEncryptedDrive.FRootPath + FileName),
                  GENERIC_READ,
                  FILE_SHARE_READ or FILE_SHARE_WRITE or FILE_SHARE_DELETE,
                  nil,
                  OPEN_EXISTING,
                  FILE_FLAG_OPEN_REPARSE_POINT,
                  0);
                FReadOnly := true;

                if FHandle = INVALID_HANDLE_VALUE then
                begin
                  OSError := EOSError.Create('Could not open file.');
                  OSError.ErrorCode := GetLastError;
                  raise OSError;
                end;
              end;
            end;
        end;
      end;
    end;
  end;
  
  if (not New) and (Pos(':', FileName) <= 0) and (NTCreateDisposition <> FILE_OPEN) and (NTCreateDisposition <> FILE_OPEN_IF) then
    Win32Check(SetFileAttributes(PChar(FormEncryptedDrive.FRootPath + FileName), FileAttributes and $ffff)); // Attributes contains creation flags as well, which we need to strip

  FInitialIV := Copy(FormEncryptedDrive.XORData);

  IncrementRef;

  GetSystemInfo(SystemInfo);
  FBufferSize := SystemInfo.dwPageSize;
  //optional - the more buffer size, the faster read/write callback processing
  FBufferSize := FBufferSize shl 4;

  // allocating buffer for read/write operations
  FBuffer := VirtualAlloc(nil, FBufferSize, MEM_COMMIT, PAGE_READWRITE);
  FMask := VirtualAlloc(nil, FBufferSize + Length(FInitialIV) * 2, MEM_COMMIT, PAGE_READWRITE);

  FCurrentSize := GetFileSize(FHandle, @size_high);
  if FCurrentSize = INVALID_FILE_SIZE then
    Exit;

  FInitialized := true;
end;

destructor TEncryptContext.Destroy;
begin
  if FBuffer <> nil then
    VirtualFree(FBuffer, FBufferSize, MEM_DECOMMIT);

  if FMask <> nil then
    VirtualFree(FMask, FBufferSize + 16, MEM_DECOMMIT);

  if FHandle <> INVALID_HANDLE_VALUE then
    CloseHandle(FHandle);

  FHandle := INVALID_HANDLE_VALUE;
  FRefCnt := 0;
  inherited;
end;


function TEncryptContext.Read(Position: Int64; var Buffer; BytesToRead: Cardinal; out TotalRead : Cardinal): Integer;
var
  CurrPos, StartPosition: Int64;
  SysBuf: PAnsiChar;
  Overlapped: TOverlapped;
  Completed, BufPos, SysBufPos: Cardinal;
begin
  SysBuf := PAnsiChar(@Buffer);
  SysBufPos := 0;
  TotalRead := 0;
  StartPosition := Position;
  Result := NO_ERROR;
  CurrPos := Position;
  BufPos := Position - CurrPos;

  while BytesToRead > 0 do
  begin
    // reading internal buffer
    FillChar(Overlapped, SizeOf(Overlapped), 0);
    Overlapped.Offset := CurrPos and $FFFFFFFF;
    Overlapped.OffsetHigh := (CurrPos shr 32) and $FFFFFFFF;
    if not ReadFile(FHandle, FBuffer^, FBufferSize, Completed,
       @Overlapped) or (Completed = 0) then
    begin
      Result := GetLastError;
      Break;
    end;

    DecryptBuffer(CurrPos, Completed);
    // copying part of internal bufer into system buffer
    if Completed > BufPos then
      Dec(Completed, BufPos);

    Completed := min(BytesToRead, Completed);
    Inc(TotalRead, Completed);
    Move(FBuffer[BufPos], SysBuf[SysBufPos], Completed);
    BufPos := 0;
    // preparing to next part of data
    Inc(SysBufPos, Completed);
    Dec(BytesToRead, min(BytesToRead,Completed));
    Inc(CurrPos, Completed);
  end;

  if Result = ERROR_HANDLE_EOF then
  begin
    if TotalRead = 0 then
      Exit
    else
      Result := NO_ERROR;
  end;

  if (StartPosition + TotalRead) > FCurrentSize then
    FCurrentSize := StartPosition + TotalRead;
end;

function TEncryptContext.Write(Position: Int64; var Buffer; BytesToWrite: Cardinal; out TotalWritten : Cardinal): Integer;
var
  CurrPos, StartPosition: Int64;
  SysBuf: PAnsiChar;
  Overlapped: TOverlapped;
  Completed, SysBufPos: Cardinal;
  ToWrite: LongWord;
begin
  SysBuf := PAnsiChar(@Buffer);
  SysBufPos := 0;
  TotalWritten := 0;
  StartPosition := Position;
  Result := NO_ERROR;
  CurrPos := Position;
  while BytesToWrite > 0 do
  begin
    // copying system buffer into internal bufer
    Completed := FBufferSize;
    Completed := min(BytesToWrite,Completed);
    Move(SysBuf[SysBufPos], FBuffer[0], Completed);
    EncryptBuffer(CurrPos, Completed);
    // writing internal buffer
    FillChar(Overlapped, SizeOf(Overlapped), 0);
    Overlapped.Offset := CurrPos and $FFFFFFFF;
    Overlapped.OffsetHigh := (CurrPos shr 32) and $FFFFFFFF;
    Completed := 0;
    ToWrite := min(BytesToWrite, FBufferSize);
    if not WriteFile(FHandle, FBuffer^, ToWrite, Completed,
      @Overlapped) then
    begin
      Result := GetLastError;
      Inc(TotalWritten, Completed);
      Break;
    end;

    // preparing to next part of data
    Inc(Position, Completed);
    Inc(SysBufPos, Completed);
    Dec(BytesToWrite, min(BytesToWrite,Completed));
    Inc(CurrPos, Completed);
    Inc(TotalWritten, Completed);
    if ToWrite > Completed then
      break;
  end;

  if (TotalWritten + StartPosition) > FCurrentSize then
    FCurrentSize := StartPosition + TotalWritten;
end;

procedure AddInt64(var IV : ByteArray; Num : Int64);
var
  NumArr : array [0..7] of byte;
  i, a, b : integer;
begin
  { big endian increment }
  for i := 7 downto 0 do
  begin
    NumArr[i] := Num and $ff;
    Num := Num shr 8;
  end;

  a := Length(IV) - 8;
  b := 0;
  for i := Length(IV) - 1 downto 0 do
  begin
    if (i > a) then
      b := b + NumArr[i - a];

    b := b + IV[i];
    IV[i] := b and $ff;
    b := b shr 8;
  end;
end;

procedure AddOne(var IV : ByteArray);
var
  i, b : integer;
begin
  b := 1;
  for i := Length(IV) - 1 downto 0 do
  begin
    b := b + IV[i];
    IV[i] := b and $ff;
    b := b shr 8;
    if b = 0 then
      break;
  end;
end;

procedure TEncryptContext.DecryptBuffer(Position: Int64; Size : integer);
var
  BlockNum : Int64;
  I, Offset, BlockCount, BlockLen : integer;
  IV : ByteArray;
begin
  BlockLen := Length(FInitialIV);
  BlockNum := Position div BlockLen;
  BlockCount := (Position - BlockNum * BlockLen + Size + BlockLen - 1) div BlockLen;

  SetLength(IV, Length(FInitialIV));
  Move(FInitialIV[0], IV[0], Length(FInitialIV));
  AddInt64(IV, BlockNum);

  for i := 0 to BlockCount - 1 do
  begin
    Move(IV[0], FMask[i * BlockLen], BlockLen);
    AddOne(IV);
  end;

  { xoring real data with gamma }
  Offset := Position mod BlockLen;

  { xoring data }
  for I := 0 to Size - 1 do
    FBuffer[I] := AnsiChar(Byte(FBuffer[I]) xor Byte(FMask[I + Offset]));
end;

procedure TEncryptContext.EncryptBuffer(Position: Int64; Size : integer);
begin
  DecryptBuffer(Position, Size);
end;

function TEncryptContext.IncrementRef: Integer;
begin
  Inc(FRefCnt);
  Result := FRefCnt;
end;

function TEncryptContext.DecrementRef: Integer;
begin
  if FRefCnt > 0 then
    Dec(FRefCnt);

  Result := FRefCnt;
end;

procedure TEncryptContext.CloseFile;
begin
  if FHandle <> INVALID_HANDLE_VALUE then
    CloseHandle(FHandle);

  FHandle := INVALID_HANDLE_VALUE;
end;

procedure TEncryptContext.SetCurrentSize(const Value: Int64);
var
  HighValue : Integer;
begin
  HighValue := (Value shr 32) and $FFFFFFFF;
  Win32Check(SetFilePointer(FHandle, Value and $FFFFFFFF, @HighValue, FILE_BEGIN) <> $FFFFFFFF {INVALID_SET_FILE_POINTER} );
  Win32Check(SetEndOfFile(FHandle));
  FCurrentSize := Value;
end;

end.
