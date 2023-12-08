object FormEncryptedDrive: TFormEncryptedDrive
  Left = 267
  Top = 169
  BorderStyle = bsDialog
  Caption = 'Encrypted Drive'
  ClientHeight = 616
  ClientWidth = 317
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  Position = poScreenCenter
  OnCreate = FormCreate
  OnDestroy = FormDestroy
  TextHeight = 13
  object lblIntro: TLabel
    Left = 8
    Top = 8
    Width = 296
    Height = 65
    Caption = 
      'The Encrypted Drive demo shows how to create an encrypted virtua' +
      'l drive. Files appear decrypted in the mounted drive, and are en' +
      'crypted on disk using simple XOR encryption. After installing th' +
      'e required drivers, mount "media" for the drive. Finally, create' +
      ' a mounting point.'
    Color = clBtnFace
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clDefault
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    ParentColor = False
    ParentFont = False
    WordWrap = True
  end
  object gbDriver: TGroupBox
    Left = 8
    Top = 80
    Width = 305
    Height = 89
    Caption = 'Driver Management'
    TabOrder = 0
    object lblDriverStatus: TLabel
      Left = 8
      Top = 68
      Width = 289
      Height = 13
      AutoSize = False
      Caption = 'Driver Status:'
    end
    object lblDriver: TLabel
      Left = 8
      Top = 16
      Width = 270
      Height = 13
      Caption = 'Install or uninstall drivers. The system drivers are required.'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clDefault
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = []
      ParentFont = False
      WordWrap = True
    end
    object btnInstall: TButton
      Left = 8
      Top = 36
      Width = 105
      Height = 25
      Caption = 'Install drivers...'
      TabOrder = 0
      OnClick = btnInstallClick
    end
    object btnUninstall: TButton
      Left = 120
      Top = 36
      Width = 105
      Height = 25
      Caption = 'Uninstall drivers'
      TabOrder = 1
      OnClick = btnUninstallClick
    end
  end
  object gbMedia: TGroupBox
    Left = 8
    Top = 176
    Width = 305
    Height = 113
    Caption = 'Media'
    TabOrder = 1
    object lblMedia: TLabel
      Left = 8
      Top = 16
      Width = 286
      Height = 26
      Caption = 
        'Select a folder to be used as the virtual drive'#39's "media". The f' +
        'older will be created if it does not exist.'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clDefault
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = []
      ParentFont = False
      WordWrap = True
    end
    object edtRootPath: TEdit
      Left = 8
      Top = 52
      Width = 217
      Height = 21
      TabOrder = 0
      Text = 'C:\my-storage-media'
    end
    object btnMount: TButton
      Left = 8
      Top = 80
      Width = 105
      Height = 25
      Caption = 'Mount'
      Enabled = False
      TabOrder = 1
      OnClick = btnMountClick
    end
    object btnUnmount: TButton
      Left = 120
      Top = 80
      Width = 105
      Height = 25
      Caption = 'Unmount'
      Enabled = False
      TabOrder = 2
      OnClick = btnUnmountClick
    end
  end
  object gbEncryption: TGroupBox
    Left = 8
    Top = 296
    Width = 305
    Height = 49
    Caption = 'Encryption'
    TabOrder = 2
    DesignSize = (
      305
      49)
    object lblPassword: TLabel
      Left = 8
      Top = 24
      Width = 101
      Height = 13
      Anchors = [akTop, akRight]
      Caption = 'Encryption password:'
    end
    object edtPassword: TEdit
      Left = 120
      Top = 20
      Width = 105
      Height = 21
      Anchors = [akTop, akRight]
      TabOrder = 0
      Text = 'password'
    end
  end
  object gbMountingPoints: TGroupBox
    Left = 8
    Top = 352
    Width = 305
    Height = 257
    Caption = 'Mounting Points'
    TabOrder = 3
    object lblMounting: TLabel
      Left = 8
      Top = 16
      Width = 286
      Height = 52
      Caption = 
        'Add a new mounting point for the virtual drive. Mounting points ' +
        'may be drive letters such as "X:", network mounting points such ' +
        'as "Y:;TestServer;VirtualShare" or UNC paths such as "TestPath".' +
        ' See the documentation for more details.'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clDefault
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = []
      ParentFont = False
      WordWrap = True
    end
    object lblMountingPoints: TLabel
      Left = 8
      Top = 80
      Width = 74
      Height = 13
      Caption = 'Mounting Point:'
    end
    object lblPoints: TLabel
      Left = 8
      Top = 112
      Width = 79
      Height = 13
      Caption = 'Mounting Points:'
    end
    object edtMountingPoint: TEdit
      Left = 120
      Top = 76
      Width = 105
      Height = 21
      TabOrder = 0
      Text = 'Z:'
    end
    object btnAddPoint: TButton
      Left = 8
      Top = 224
      Width = 105
      Height = 25
      Caption = 'Add'
      Enabled = False
      TabOrder = 2
      OnClick = btnAddPointClick
    end
    object btnDeletePoint: TButton
      Left = 120
      Top = 224
      Width = 105
      Height = 25
      Caption = 'Delete'
      Enabled = False
      TabOrder = 3
      OnClick = btnDeletePointClick
    end
    object lstPoints: TListBox
      Left = 120
      Top = 107
      Width = 105
      Height = 110
      ItemHeight = 13
      MultiSelect = True
      TabOrder = 1
      OnClick = lstPointsClick
    end
  end
  object dlgOpenDrv: TOpenDialog
    DefaultExt = '.cab'
    Filter = 'CBFS Connect installation package|cbfs.cab'
    InitialDir = 
      'C:\Program Files\Callback Technologies\CBFS Connect 2020 Delphi ' +
      'Edition\drivers'
    Title = 'Select CBFS Connect installation package'
    Left = 264
    Top = 117
  end
end


