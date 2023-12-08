object FormFolderDrive: TFormFolderDrive
  Left = 267
  Top = 169
  BorderStyle = bsDialog
  Caption = 'Folder Drive'
  ClientHeight = 509
  ClientWidth = 321
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
    Width = 305
    Height = 65
    AutoSize = False
    Caption = 
      'The Folder Drive demo provides a basic example of how to use CBF' +
      'S Connect to "mount" the contents of a folder as a virtual drive' +
      '. First, install the required drivers. Second, create storage fo' +
      'r the drive. Third, mount "media" in that drive. Finally, create' +
      ' a mounting point for it.'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clDefault
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    ParentFont = False
    WordWrap = True
  end
  object gbInstallDrivers: TGroupBox
    Left = 8
    Top = 79
    Width = 305
    Height = 82
    Caption = 'Install Drivers'
    TabOrder = 0
    object lblDriverStatus: TLabel
      Left = 8
      Top = 64
      Width = 289
      Height = 13
      AutoSize = False
      Caption = 'Driver Status:'
      WordWrap = True
    end
    object lblInstallDrivers: TLabel
      Left = 8
      Top = 16
      Width = 281
      Height = 17
      AutoSize = False
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
  object gbCreateVirtualDrive: TGroupBox
    Left = 8
    Top = 167
    Width = 305
    Height = 82
    Caption = 'Storage'
    TabOrder = 1
    object lblCreateVirtualDrive: TLabel
      Left = 8
      Top = 16
      Width = 289
      Height = 25
      AutoSize = False
      Caption = 
        'Create or delete the virtual drive. Storage is created without m' +
        'ounting points.'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clDefault
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = []
      ParentFont = False
      WordWrap = True
    end
    object btnCreateVirtualDrive: TButton
      Left = 8
      Top = 48
      Width = 105
      Height = 25
      Caption = 'Create Storage'
      TabOrder = 0
      OnClick = btnCreateVirtualDriveClick
    end
    object btnDeleteVirtualDrive: TButton
      Left = 120
      Top = 48
      Width = 105
      Height = 25
      Caption = 'Delete Storage'
      Enabled = False
      TabOrder = 1
      OnClick = btnDeleteVirtualDriveClick
    end
  end
  object gbMountVirtualMedia: TGroupBox
    Left = 8
    Top = 255
    Width = 305
    Height = 106
    Caption = 'Media'
    TabOrder = 2
    object lblMountVirtualMedia: TLabel
      Left = 8
      Top = 15
      Width = 289
      Height = 25
      AutoSize = False
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
    object lblMediaLocation: TLabel
      Left = 8
      Top = 48
      Width = 89
      Height = 13
      AutoSize = False
      Caption = 'Location of Media:'
      WordWrap = True
    end
    object btnMount: TButton
      Left = 8
      Top = 72
      Width = 105
      Height = 25
      Caption = 'Mount'
      Enabled = False
      TabOrder = 1
      OnClick = btnMountClick
    end
    object btnUnmount: TButton
      Left = 120
      Top = 72
      Width = 105
      Height = 25
      Caption = 'Unmount'
      Enabled = False
      TabOrder = 2
      OnClick = btnUnmountClick
    end
    object edtRootPath: TEdit
      Left = 103
      Top = 45
      Width = 194
      Height = 21
      TabOrder = 0
      Text = 'C:\my-storage-media'
    end
  end
  object gbAddMountingPoint: TGroupBox
    Left = 8
    Top = 367
    Width = 305
    Height = 134
    Caption = 'Add Mounting Point'
    TabOrder = 3
    object lblMountingPoints: TLabel
      Left = 8
      Top = 78
      Width = 60
      Height = 13
      Alignment = taCenter
      AutoSize = False
      Caption = 'Drive Letter:'
    end
    object lblAddMountingPoint: TLabel
      Left = 8
      Top = 16
      Width = 289
      Height = 57
      AutoSize = False
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
    object edtMountingPoint: TEdit
      Left = 75
      Top = 74
      Width = 150
      Height = 21
      TabOrder = 0
      Text = 'Z:'
    end
    object btnAddPoint: TButton
      Left = 8
      Top = 100
      Width = 105
      Height = 25
      Caption = 'Add'
      Enabled = False
      TabOrder = 1
      OnClick = btnAddPointClick
    end
    object btnDeletePoint: TButton
      Left = 120
      Top = 100
      Width = 105
      Height = 25
      Caption = 'Delete'
      Enabled = False
      TabOrder = 2
      OnClick = btnDeletePointClick
    end
  end
  object dlgOpenDrv: TOpenDialog
    DefaultExt = '.cab'
    Filter = 'CBFS Connect installation package|cbfs.cab'
    InitialDir = '..\..\..\..\drivers'
    Title = 'Select CBFS Connect installation package'
    Left = 256
    Top = 112
  end
end


