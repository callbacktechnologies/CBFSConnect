/*
 * CBFS Connect 2024 .NET Edition - Sample Project
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
 * 
 */

#define EXECUTE_IN_DEFAULT_APP_DOMAIN

using System;
using System.IO;
using System.ComponentModel;
using System.Diagnostics;
using Microsoft.Win32.SafeHandles;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;

using callback.CBFSConnect;

namespace callback.Demos
{



    [Flags]
    public enum CBFileAttributes : uint
    {
        Readonly = 1,
        Hidden = 2,
        System = 4,
        Directory = 0x10,
        Archive = 0x20,
        Device = 0x40,
        Normal = 0x80,
        Temporary = 0x100,
        SparseFile = 0x200,
        ReparsePoint = 0x400,
        Compressed = 0x800,
        Offline = 0x1000,
        NotContentIndexed = 0x2000,
        Encrypted = 0x4000,
        OpenNoRecall = 0x100000,
        OpenReparsePoint = 0x200000,
        FirstPipeInstance = 0x80000,
        PosixSemantics = 0x1000000,
        BackupSemantics = 0x2000000,
        DeleteOnClose = 0x4000000,
        SequentialScan = 0x8000000,
        RandomAccess = 0x10000000,
        NoBuffering = 0x20000000,
        Overlapped = 0x40000000,
        Write_Through = 0x80000000
    }

    [Flags]
    public enum CBFileAccess : uint
    {
        AccessSystemSecurity = 0x1000000,
        Delete = 0x10000,
        file_read_data = 1,
        file_list_directory = 1,
        file_add_file = 2,
        file_write_data = 2,
        file_add_subdirectory = 4,
        file_append_data = 4,
        file_create_pipe_instance = 4,
        file_read_ea = 8,
        file_write_ea = 0x10,
        file_traverse = 0x20,
        file_execute = 0x20,
        file_delete_child = 0x40,
        file_read_attributes = 0x80,
        file_write_attributes = 0x100,
        file_all_access = 0x1f01ff,
        file_generic_execute = 0x1200a0,
        file_generic_read = 0x120089,
        file_generic_write = 0x120116,
        GenericAll = 0x10000000,
        GenericExecute = 0x20000000,
        GenericRead = 0x80000000,
        GenericWrite = 0x40000000,
        MaximumAllowed = 0x2000000,
        ReadControl = 0x20000,
        specific_rightts_all = 0xffff,
        SpecificRightsAll = 0xffff,
        StandardRightsAll = 0x1f0000,
        StandardRightsExecute = 0x20000,
        StandardRightsRead = 0x20000,
        StandardRightsRequired = 0xf0000,
        StandardRightsWrite = 0x20000,
        Synchronize = 0x100000,
        WriteDAC = 0x40000,
        WriteOwner = 0x80000
    }

    public enum CBDriverState
    {
        SERVICE_STOPPED = 1,
        SERVICE_START_PENDING = 2,
        SERVICE_STOP_PENDING = 3,
        SERVICE_RUNNING = 4,
        SERVICE_CONTINUE_PENDING = 5,
        SERVICE_PAUSE_PENDING = 6,
        SERVICE_PAUSED = 7
    }

    class folderdriveDemo
    {

        static void Usage()
        {
            Console.WriteLine("Usage: cachefolderdrive-netcore.exe [-<switch 1> ... -<switch N>] <mapped folder> <mounting point>");
            Console.WriteLine("");
            Console.WriteLine("For example:  cachefolderdrive-netcore.exe C:\\temp Z:");

            Console.WriteLine("<Switches>");
            Console.WriteLine("  -lc - Mount disk for current user only");
            Console.WriteLine("  -n - Mount disk as network volume");
            Console.WriteLine("  -ps (pid|proc_name) - Add process, permitted to access the disk");
            Console.WriteLine("  -drv cab_file - Install drivers from CAB file");
            Console.WriteLine("  -i icon_path - Set overlay icon for the disk");
        }

        #region Variables and Types
        private static CBFS mCBFSConn;
        private static CBCache mCBCache;
        private static string mRootPath = "";
        private static string mOptMountingPoint = "";
        private static string mOptIconPath = "";
        private static string mOptDrvPath = "";
        private static bool mOptLocal = false;
        private static bool mOptNetwork = false;
        private static UInt32 mOptPermittedProcessId = 0;
        private static string mOptPermittedProcessName = "";

        private static bool mEnumerateNamedStreamsEventSet = false;
        private static bool mGetFileNameByFileIdEventSet = false;
        private static bool mEnumerateHardLinksEventSet = false;

        private const int BUFFER_SIZE = 1024 * 1024; // 1 MB

        private const string mGuid = "{713CC6CE-B3E2-4fd9-838D-E28F558F6866}";

        static private IntPtr INVALID_HANDLE_VALUE = new IntPtr(-1);

        private const int ERROR_INVALID_FUNCTION = 1;
        private const int ERROR_FILE_NOT_FOUND = 2;
        private const int ERROR_PATH_NOT_FOUND = 3;
        private const int ERROR_ACCESS_DENIED = 5;
        private const int ERROR_INVALID_DRIVE = 15;
        private const int ERROR_HANDLE_EOF = 38;
        private const int ERROR_INSUFFICIENT_BUFFER = 122;
        private const int ERROR_OPERATION_ABORTED = 0x3e3;
        private const int ERROR_INTERNAL_ERROR = 0x54f;
        private const int ERROR_PRIVILEGE_NOT_HELD = 1314;
        private const int ERROR_DIRECTORY = 0x10b;
        private const int ERROR_DIR_NOT_EMPTY = 0x91;

        private const uint FILE_FLAG_BACKUP_SEMANTICS = 0x02000000;
        private const uint FILE_FLAG_OPEN_REPARSE_POINT = 0x00200000;

        private const uint FILE_ATTRIBUTE_NORMAL = 0x00000080;

        private const uint FILE_READ_DATA = 0x0001;
        private const uint FILE_WRITE_DATA = 0x0002;
        private const uint FILE_APPEND_DATA = 0x0004;

        private const uint FILE_READ_EA = 0x0008;
        private const uint FILE_WRITE_EA = 0x0010;

        private const uint FILE_READ_ATTRIBUTES = 0x0080;
        private const uint FILE_WRITE_ATTRIBUTES = 0x0100;
        private const uint FILE_ATTRIBUTE_ARCHIVE = 0x00000020;

        private const uint SYNCHRONIZE = 0x00100000;

        private const uint READ_CONTROL = 0x00020000;
        private const uint STANDARD_RIGHTS_READ = READ_CONTROL;
        private const uint STANDARD_RIGHTS_WRITE = READ_CONTROL;

        private const uint GENERIC_READ = STANDARD_RIGHTS_READ |
                                            FILE_READ_DATA |
                                            FILE_READ_ATTRIBUTES |
                                            FILE_READ_EA |
                                            SYNCHRONIZE;

        private const uint GENERIC_WRITE = STANDARD_RIGHTS_WRITE |
                                            FILE_WRITE_DATA |
                                            FILE_WRITE_ATTRIBUTES |
                                            FILE_WRITE_EA |
                                            FILE_APPEND_DATA |
                                            SYNCHRONIZE;

        private const uint FSCTL_SET_REPARSE_POINT = 0x900A4;
        private const uint FSCTL_GET_REPARSE_POINT = 0x900A8;
        private const uint FSCTL_DELETE_REPARSE_POINT = 0x900AC;

        private const int SECTOR_SIZE = 512;

        private const uint FILE_OPEN = 0x00000001;
        private const uint FILE_OPEN_IF = 0x00000003;

        private static readonly DateTime ZERO_FILETIME = new DateTime(1601, 1, 1, 0, 0, 0, DateTimeKind.Utc);

        public enum StreamType
        {
            Data = 1,
            ExternalData = 2,
            SecurityData = 3,
            AlternateData = 4,
            Link = 5,
            PropertyData = 6,
            ObjectID = 7,
            ReparseData = 8,
            SparseDock = 9
        }

        public enum FSINFOCLASS
        {
            FileFsVolumeInformation = 1,
            FileFsLabelInformation,      // 2
            FileFsSizeInformation,       // 3
            FileFsDeviceInformation,     // 4
            FileFsAttributeInformation,  // 5
            FileFsControlInformation,    // 6
            FileFsFullSizeInformation,   // 7
            FileFsObjectIdInformation,   // 8
            FileFsDriverPathInformation, // 9
            FileFsMaximumInformation
        }

        [StructLayout(LayoutKind.Sequential, Pack = 8)]
        public struct FILE_FS_CONTROL_INFORMATION
        {
            public Int64 FreeSpaceStartFiltering;
            public Int64 FreeSpaceThreshold;
            public Int64 FreeSpaceStopFiltering;
            public Int64 DefaultQuotaThreshold;
            public Int64 DefaultQuotaLimit;
            public uint FileSystemControlFlags;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 4)]
        public struct Win32StreamID
        {
            public StreamType dwStreamId;
            public int dwStreamAttributes;
            public long Size;
            public int dwStreamNameSize;
            // WCHAR cStreamName[1]; 
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct REPARSE_DATA_BUFFER
        {
            public uint ReparseTag;
            public ushort ReparseDataLength;
            public ushort Reserved;
            public ushort SubstituteNameOffset;
            public ushort SubstituteNameLength;
            public ushort PrintNameOffset;
            public ushort PrintNameLength;
            // uncomment if full length reparse point must be read
            //[MarshalAs(UnmanagedType.ByValArray, SizeConst = 0x3FF0)]
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 0xF)]
            public byte[] PathBuffer;
        }

        public class FileStreamContext
        {
            public FileStreamContext()
            {
                OpenCount = 0;
                QuotaIndexFile = false;
                hStream = null;
            }
            public void IncrementCounter()
            {
                ++OpenCount;
            }
            public void DecrementCounter()
            {
                --OpenCount;
            }
            public bool QuotaIndexFile;
            public int OpenCount;
            public FileStream hStream;
        }

        [DllImport("ole32.dll")]
        static extern int OleInitialize(IntPtr pvReserved);

        [StructLayout(LayoutKind.Sequential, Pack = 4)]
        struct BY_HANDLE_FILE_INFORMATION
        {
            public uint dwFileAttributes;
            public System.Runtime.InteropServices.ComTypes.FILETIME ftCreationTime;
            public System.Runtime.InteropServices.ComTypes.FILETIME ftLastAccessTime;
            public System.Runtime.InteropServices.ComTypes.FILETIME ftLastWriteTime;
            public uint dwVolumeSerialNumber;
            public uint nFileSizeHigh;
            public uint nFileSizeLow;
            public uint nNumberOfLinks;
            public uint nFileIndexHigh;
            public uint nFileIndexLow;
        };

        [DllImport("kernel32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern Boolean BackupRead(SafeFileHandle hFile,
            IntPtr lpBuffer,
            uint nNumberOfBytesToRead,
            out uint lpNumberOfBytesRead,
            [MarshalAs(UnmanagedType.Bool)] Boolean bAbort,
            [MarshalAs(UnmanagedType.Bool)] Boolean bProcessSecurity,
            ref IntPtr lpContext
            );

        [DllImport("kernel32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool BackupSeek(SafeFileHandle hFile,
            uint dwLowBytesToSeek,
            uint dwHighBytesToSeek,
            out uint lpdwLowByteSeeked,
            out uint lpdwHighByteSeeked,
            ref IntPtr lpContext
            );

        // The CharSet must match the CharSet of the corresponding PInvoke signature
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
        struct WIN32_FIND_DATA
        {
            public uint dwFileAttributes;
            public System.Runtime.InteropServices.ComTypes.FILETIME ftCreationTime;
            public System.Runtime.InteropServices.ComTypes.FILETIME ftLastAccessTime;
            public System.Runtime.InteropServices.ComTypes.FILETIME ftLastWriteTime;
            public uint nFileSizeHigh;
            public uint nFileSizeLow;
            public uint dwReserved0;
            public uint dwReserved1;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
            public string cFileName;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 14)]
            public string cAlternateFileName;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        struct WIN32_FIND_STREAM_DATA
        {
            public long qwStreamSize;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 296)]
            public string cStreamName;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool ReadFile(IntPtr hFile, IntPtr lpBuffer, int nNumberOfBytesToRead, out int lpNumberOfBytesRead, [In] ref System.Threading.NativeOverlapped lpOverlapped);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool WriteFile(IntPtr hFile, IntPtr lpBuffer, int nNumberOfBytesToWrite, out int lpNumberOfBytesWritten, [In] ref System.Threading.NativeOverlapped lpOverlapped);

        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern SafeFileHandle CreateFile(string lpFileName,
            uint dwDesiredAccess, FileShare dwShareMode,
            IntPtr lpSecurityAttributes, FileMode dwCreationDisposition,
            uint dwFlagsAndAttributes, IntPtr hTemplateFile);

        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool GetFileInformationByHandle(SafeFileHandle hFile, out BY_HANDLE_FILE_INFORMATION lpFileInformation);

        [DllImport("Advapi32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool SetFileSecurity(string FileName, uint SecurityInformation, IntPtr SecurityDecriptor);

        [DllImport("Advapi32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool GetFileSecurity(string FileName,
            uint SecurityInformation, IntPtr SecurityDecriptor, uint Length, ref uint LengthNeeded);

        [DllImport("advapi32.dll")]
        private static extern UInt32 GetSecurityDescriptorLength(IntPtr pSecurityDescriptor);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr FindFirstFileNameW(string FileName,
            uint dwFlags,
            ref uint StringLength,
            IntPtr LinkName
            );

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool FindNextFileNameW(IntPtr hFindStream,
            ref uint StringLength,
            IntPtr LinkName
            );

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr FindFirstStreamW(string FileName, uint dwLevel, out WIN32_FIND_STREAM_DATA lpFindStreamData, uint dwFlags);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool FindNextStreamW(IntPtr hFindStream, out WIN32_FIND_STREAM_DATA lpFindStreamData);

        [DllImport("kernel32.dll", CharSet = CharSet.Auto)]
        private static extern IntPtr FindFirstFile(string lpFileName, out WIN32_FIND_DATA lpFindFileData);

        [DllImport("kernel32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool FindClose(IntPtr hFindFile);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CreateHardLinkW(String FileName, String ExistingFileName, IntPtr lpSecurityAttributes);

        [StructLayout(LayoutKind.Sequential, Pack = 0)]
        private struct OBJECT_ATTRIBUTES
        {
            internal UInt32 Length;
            internal IntPtr RootDirectory;
            internal IntPtr ObjectName;
            internal uint Attributes;
            internal IntPtr SecurityDescriptor;
            internal IntPtr SecurityQualityOfService;
        };

        [StructLayout(LayoutKind.Sequential, Pack = 0)]
        private struct UNICODE_STRING
        {
            internal ushort Length;
            internal ushort MaximumLength;
            internal IntPtr Buffer;
        };

        [StructLayout(LayoutKind.Sequential, Pack = 0)]
        private struct IO_STATUS_BLOCK
        {
            internal uint Status;
            internal IntPtr information;
        }

        [DllImport("ntdll.dll", ExactSpelling = true, SetLastError = false)]
        private static extern uint NtOpenFile(
            out Microsoft.Win32.SafeHandles.SafeFileHandle FileHandle,
            uint DesiredAccess,
            ref OBJECT_ATTRIBUTES ObjectAttributes,
            ref IO_STATUS_BLOCK IoStatusBlock,
            System.IO.FileShare ShareAccess,
            uint OpenOptions
            );

        [DllImport("Advapi32.dll", ExactSpelling = true, SetLastError = false)]
        private static extern uint LsaNtStatusToWinError(uint Status);

        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool GetFileInformationByHandleEx(
            SafeFileHandle FileHandle,
            uint FileInformationClass,
            IntPtr FileInformation,
            uint BufferSize
            );

        [DllImport("ntdll.dll", CharSet = CharSet.Auto, SetLastError = false)]
        private static extern uint NtSetVolumeInformationFile(
            Microsoft.Win32.SafeHandles.SafeFileHandle FileHandle,
            ref IO_STATUS_BLOCK IoStatusBlock,
            ref FILE_FS_CONTROL_INFORMATION FsInformation,
            int Length,
            FSINFOCLASS FsInformationClass
            );

        [DllImport("ntdll.dll", CharSet = CharSet.Auto, SetLastError = false)]
        private static extern uint NtQueryVolumeInformationFile(
            Microsoft.Win32.SafeHandles.SafeFileHandle FileHandle,
            ref IO_STATUS_BLOCK IoStatusBlock,
            ref FILE_FS_CONTROL_INFORMATION FsInformation,
            int Length,
            FSINFOCLASS FsInformationClass
            );

        [DllImport("Kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        public static extern bool DeviceIoControl(
            IntPtr hDevice,
            uint dwIoControlCode,
            IntPtr InBuffer,
            UInt32 nInBufferSize,
            IntPtr OutBuffer,
            UInt32 nOutBufferSize,
            ref UInt32 pBytesReturned,
            [In] ref System.Threading.NativeOverlapped lpOverlapped);

        [return: MarshalAs(UnmanagedType.Bool)]
        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        public static extern bool GetDiskFreeSpaceEx(
            string lpDirectoryName,
            out ulong lpFreeBytesAvailable,
            out ulong lpTotalNumberOfBytes,
            out ulong lpTotalNumberOfFreeBytes);

        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        private static extern bool DeleteFile(string lpFileName);
        #endregion
        //-----------------------------------------------------------------------------

        static void Main(string[] args)
        {
            bool op = false;

            if (args.Length < 2)
            {
                Usage();
                return;
            }

            if (HandleParameters(args) == 0)
            {
                // check the operation
                if (mOptDrvPath != "")
                {
                    //install the drivers
                    InstallDriver();
                    op = true;
                }
                if (mOptIconPath != "")
                {
                    //register the icon
                    InstallIcon();
                    op = true;
                }
                if (mOptMountingPoint != "" && mRootPath != "")
                {
                    RunMounting();
                    op = true;
                }
            }
            if (!op)
            {
                Console.WriteLine("Unrecognized combination of parameters passed. You need to either install the driver using -drv switch or specify the size of the disk and the mounting point name to create a virtual disk");
            }
        }

        private static void AskForReboot(bool isInstall)
        {
            Console.WriteLine("Warning: System restart is needed in order to " + (isInstall ? "install" : "uninstall") + " the drivers.\rPlease, reboot your computer now.");
        }

        private static bool isWriteRightRequested(CBFileAccess access)
        {
            CBFileAccess writeRight = CBFileAccess.file_write_data |
                                      CBFileAccess.file_append_data |
                                      CBFileAccess.file_write_ea |
                                      CBFileAccess.file_write_attributes;
            return (access & writeRight) != 0;
        }

        private static void SetupCBFS()
        {
            CheckDriver();

            mCBCache = new CBCache();

            string CacheDirectory = @".\cache";

            if (!Directory.Exists(CacheDirectory))
                Directory.CreateDirectory(CacheDirectory);
            mCBCache.CacheDirectory = CacheDirectory;
            mCBCache.CacheFile = @"cache.svlt";
            mCBCache.ReadingCapabilities = Constants.RWCAP_POS_RANDOM | Constants.RWCAP_SIZE_ANY;
            mCBCache.ResizingCapabilities = Constants.RSZCAP_GROW_TO_ANY | Constants.RSZCAP_SHRINK_TO_ANY | Constants.RSZCAP_TRUNCATE_AT_ZERO;
            mCBCache.WritingCapabilities = Constants.RWCAP_POS_RANDOM | Constants.RWCAP_SIZE_ANY | Constants.RWCAP_WRITE_KEEPS_FILESIZE;

            mCBCache.OnReadData += new CBCache.OnReadDataHandler(CBCacheReadData);
            mCBCache.OnWriteData += new CBCache.OnWriteDataHandler(CBCacheWriteData);
            mCBCache.OpenCache("", false);

            mCBFSConn = new CBFS();
            mCBFSConn.Initialize(mGuid);

            mCBFSConn.StorageCharacteristics = 0;

            mCBFSConn.Config("SectorSize=" + SECTOR_SIZE.ToString());

            mCBFSConn.OnMount += new CBFS.OnMountHandler(CBMount);
            mCBFSConn.OnUnmount += new CBFS.OnUnmountHandler(CBUnmount);
            mCBFSConn.OnGetVolumeSize += new CBFS.OnGetVolumeSizeHandler(CBGetVolumeSize);
            mCBFSConn.OnGetVolumeLabel += new CBFS.OnGetVolumeLabelHandler(CBGetVolumeLabel);
            mCBFSConn.OnSetVolumeLabel += new CBFS.OnSetVolumeLabelHandler(CBSetVolumeLabel);
            mCBFSConn.OnGetVolumeId += new CBFS.OnGetVolumeIdHandler(CBGetVolumeId);
            mCBFSConn.OnCreateFile += new CBFS.OnCreateFileHandler(CBCreateFile);
            mCBFSConn.OnOpenFile += new CBFS.OnOpenFileHandler(CBOpenFile);
            mCBFSConn.OnCloseFile += new CBFS.OnCloseFileHandler(CBCloseFile);
            mCBFSConn.OnGetFileInfo += new CBFS.OnGetFileInfoHandler(CBGetFileInfo);
            mCBFSConn.OnEnumerateDirectory += new CBFS.OnEnumerateDirectoryHandler(CBEnumerateDirectory);
            mCBFSConn.OnCloseDirectoryEnumeration += new CBFS.OnCloseDirectoryEnumerationHandler(CBCloseDirectoryEnumeration);
            mCBFSConn.OnSetAllocationSize += new CBFS.OnSetAllocationSizeHandler(CBSetAllocationSize);
            mCBFSConn.OnSetFileSize += new CBFS.OnSetFileSizeHandler(CBSetFileSize);
            mCBFSConn.OnSetFileAttributes += new CBFS.OnSetFileAttributesHandler(CBSetFileAttributes);
            mCBFSConn.OnCanFileBeDeleted += new CBFS.OnCanFileBeDeletedHandler(CBCanFileBeDeleted);
            mCBFSConn.OnDeleteFile += new CBFS.OnDeleteFileHandler(CBDeleteFile);
            mCBFSConn.OnRenameOrMoveFile += new CBFS.OnRenameOrMoveFileHandler(CBRenameOrMoveFile);
            mCBFSConn.OnReadFile += new CBFS.OnReadFileHandler(CBReadFile);
            mCBFSConn.OnWriteFile += new CBFS.OnWriteFileHandler(CBWriteFile);
            mCBFSConn.OnIsDirectoryEmpty += new CBFS.OnIsDirectoryEmptyHandler(CBIsDirectoryEmpty);

            mCBFSConn.UseAlternateDataStreams = false;
            mCBFSConn.OnEjected += new CBFS.OnEjectedHandler(CBEjectStorage);

            mCBFSConn.OnFsctl += new CBFS.OnFsctlHandler(CBFsctl);
        }

        #region Helper Methods
        private static void AddMountingPoint(string mp)
        {
            try
            {
                int flags = (mOptLocal ? Constants.STGMP_LOCAL : 0);

                if (mOptNetwork && mp.Contains(";")) //Assume it's a network mounting point for demo purposes
                {
                    mCBFSConn.AddMountingPoint(mp, Constants.STGMP_NETWORK | Constants.STGMP_NETWORK_ALLOW_MAP_AS_DRIVE | flags, 0);
                    mp = mp.Substring(0, mp.IndexOf(";"));
                }
                else
                    mCBFSConn.AddMountingPoint(mp, Constants.STGMP_MOUNT_MANAGER | flags, 0);

                Process.Start("explorer.exe", mp);
            }
            catch (CBFSConnectException err)
            {
                Console.WriteLine("Error: " + err.Message);
            }
        }

        private static int ExceptionToErrorCode(Exception ex)
        {
            if (ex is FileNotFoundException)
                return ERROR_FILE_NOT_FOUND;
            else if (ex is DirectoryNotFoundException)
                return ERROR_PATH_NOT_FOUND;
            else if (ex is DriveNotFoundException)
                return ERROR_INVALID_DRIVE;
            else if (ex is OperationCanceledException)
                return ERROR_OPERATION_ABORTED;
            else if (ex is UnauthorizedAccessException)
                return ERROR_ACCESS_DENIED;
            else if (ex is Win32Exception)
                return ((Win32Exception)ex).NativeErrorCode;
            else if (ex is IOException && ex.InnerException is Win32Exception)
                return ((Win32Exception)ex.InnerException).NativeErrorCode;
            else
                return ERROR_INTERNAL_ERROR;
        }



        static int HandleParameters(string[] args)
        {
            int i = 0;
            string curParam;
            int nonDashParam = 0;

            while (i < args.Length)
            {
                curParam = args[i];
                if (string.IsNullOrEmpty(curParam))
                    break;

                if (nonDashParam == 0)
                {
                    if (curParam[0] == '-')
                    {
                        // icon path
                        if (curParam == "-i")
                        {
                            if (i + 1 < args.Length)
                            {
                                mOptIconPath = ConvertRelativePathToAbsolute(args[i + 1]);
                                if (!string.IsNullOrEmpty(mOptIconPath))
                                {
                                    i += 2;
                                    continue;
                                }
                            }
                            Console.WriteLine("-i switch requires the valid path to the .ico file with icons as the next parameter.");
                            return 1;
                        }
                        else
                        // user-local disk
                        if (curParam == "-lc")
                        {
                            mOptLocal = true;
                        }
                        else
                        // network disk
                        if (curParam == "-n")
                        {
                            mOptNetwork = true;
                        }
                        else
                        // permitted process
                        if (curParam == "-ps")
                        {
                            if (i + 1 < args.Length)
                            {
                                curParam = args[i + 1];
                                if (!UInt32.TryParse(curParam, out mOptPermittedProcessId))
                                    mOptPermittedProcessName = curParam;
                                i += 2;
                                continue;
                            }
                            Console.WriteLine("Process Id or process name must follow the -ps switch. Process Id must be a positive integer, and a process name may contain an absolute path or just the name of the executable module.");
                            return 1;
                        }
                        else
                        // driver path
                        if (curParam == "-drv")
                        {
                            if (i + 1 < args.Length)
                            {
                                mOptDrvPath = ConvertRelativePathToAbsolute(args[i + 1]);
                                if (!string.IsNullOrEmpty(mOptDrvPath) && (Path.GetExtension(mOptDrvPath) == ".cab"))
                                {
                                    i += 2;
                                    continue;
                                }
                            }
                            Console.WriteLine("-drv switch requires the valid path to the .cab file with drivers as the next parameter.");
                            return 1;
                        }
                    }
                    else
                    {
                        mRootPath = ConvertRelativePathToAbsolute(curParam);
                        if (string.IsNullOrEmpty(mRootPath) || !File.Exists(mRootPath))
                        {
                            Console.WriteLine("The folder '{0}', specified as the one to map, doesn't seem to exist.", curParam);
                            return 1;
                        }
                        i++;
                        nonDashParam += 1;
                        continue;
                    }
                }
                else
                {
                    if (curParam.Length == 1)
                    {
                        curParam = curParam + ":";
                    }
                    mOptMountingPoint = ConvertRelativePathToAbsolute(curParam, true);
                    if (string.IsNullOrEmpty(mOptMountingPoint))
                    {
                        Console.WriteLine("Error: Invalid Mounting Point Path");
                        return 1;
                    }
                    break;
                }
                i++;
            }
            return 0;
        }

        private static string ConvertRelativePathToAbsolute(string path, bool acceptMountingPoint = false)
        {
            string res = null;
            if (!string.IsNullOrEmpty(path))
            {
                res = path;

                // Linux-specific case of using a home directory
                if (path.Equals("~") || path.StartsWith("~/"))
                {
                    string homeDir = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                    if (path.Equals("~") || path.Equals("~/"))
                        return homeDir;
                    else
                        return Path.Combine(homeDir, path.Substring(2));
                }
                int semicolonCount = path.Split(';').Length - 1;
                bool isNetworkMountingPoint = semicolonCount == 2;
                string remainedPath = "";
                if (isNetworkMountingPoint)
                {
                    if (!acceptMountingPoint)
                    {
                        Console.WriteLine($"The path '{path}' format cannot be equal to the Network Mounting Point");
                        return "";
                    }
                    int pos = path.IndexOf(';');
                    if (pos != path.Length - 1)
                    {
                        res = path.Substring(0, pos);
                        if (String.IsNullOrEmpty(res))
                        {
                            return path;
                        }
                        remainedPath = path.Substring(pos);
                    }
                }

                if (IsDriveLetter(res))
                {
                    if (!acceptMountingPoint)
                    {
                        Console.WriteLine($"The path '{res}' format cannot be equal to the Drive Letter");
                        return "";
                    }
                    return path;
                }

                if (!Path.IsPathRooted(res))
                {
                    res = Path.GetFullPath(res);

                    return res + remainedPath;
                }
            }
            return path;
        }

        private static bool IsDriveLetter(string path)
        {
            if (string.IsNullOrEmpty(path))
                return false;

            char c = path[0];
            if ((char.IsLetter(c) && path.Length == 2 && path[1] == ':'))
                return true;
            else
                return false;
        }

        private static void CheckDriver()
        {
            CBFS disk = new CBFS();

            int status = disk.GetDriverStatus(mGuid, Constants.MODULE_DRIVER);

            if (status != 0)
            {
                string strStat;

                switch (status)
                {
                    case (int)CBDriverState.SERVICE_CONTINUE_PENDING:
                        strStat = "continue is pending";
                        break;
                    case (int)CBDriverState.SERVICE_PAUSE_PENDING:
                        strStat = "pause is pending";
                        break;
                    case (int)CBDriverState.SERVICE_PAUSED:
                        strStat = "is paused";
                        break;
                    case (int)CBDriverState.SERVICE_RUNNING:
                        strStat = "is running";
                        break;
                    case (int)CBDriverState.SERVICE_START_PENDING:
                        strStat = "is starting";
                        break;
                    case (int)CBDriverState.SERVICE_STOP_PENDING:
                        strStat = "is stopping";
                        break;
                    case (int)CBDriverState.SERVICE_STOPPED:
                        strStat = "is stopped";
                        break;
                    default:
                        strStat = "in undefined state";
                        break;
                }

                long version = disk.GetModuleVersion(mGuid, Constants.MODULE_DRIVER);
                Console.WriteLine(string.Format("Driver Status: (ver {0}.{1}.{2}.{3}) installed, service {4}", version >> 48, (version >> 32) & 0xFFFF, (version >> 16) & 0xFFFF, version & 0xFFFF, strStat));
            }
            else
            {
                Console.Write("Driver not installed. Would you like to install it now? (y/n): ");
                string input = Console.ReadLine();
                if (input.ToLower().StartsWith("y"))
                {
                    //Install the driver
                    InstallDriver();
                }
            }
        }

        static int InstallDriver()
        {
            int drvReboot = 0;
            CBFS disk = new CBFS();

            if (mOptDrvPath == "")
            {
                mOptDrvPath = "..\\..\\..\\..\\..\\drivers\\cbfs.cab";
            }

            Console.WriteLine("Installing the drivers from {0}", mOptDrvPath);
            try
            {
                drvReboot = disk.Install(mOptDrvPath, mGuid, null, Constants.MODULE_DRIVER | Constants.MODULE_HELPER_DLL, Constants.INSTALL_REMOVE_OLD_VERSIONS);
            }
            catch (CBFSConnectException err)
            {
                if (err.Code == ERROR_PRIVILEGE_NOT_HELD)
                    Console.WriteLine("Error: Installation requires administrator rights. Run the app as administrator");
                else
                    Console.WriteLine("Error: Drivers are not installed, error code {1} ({0})", err.Message, err.Code);
                return err.Code;
            }
            Console.WriteLine("Drivers installed successfully");
            if (drvReboot != 0)
                Console.WriteLine("Reboot is required by the driver installation routine");
            return 0;
        }

        static int InstallIcon()
        {
            bool reboot = false;
            CBFS disk = new CBFS();
            Console.WriteLine("Installing the icon from {0}", mOptIconPath);
            try
            {
                reboot = disk.RegisterIcon(mOptIconPath, mGuid, "sample_icon");
            }
            catch (CBFSConnectException err)
            {
                if (err.Code == ERROR_PRIVILEGE_NOT_HELD)
                    Console.WriteLine("Error: Icon registration requires administrator rights. Run the app as administrator");
                else
                    Console.WriteLine("Error: Icon not registered, error code {1} ({0})", err.Message, err.Code);
                return err.Code;
            }
            Console.WriteLine("Drivers installed successfully");
            if (reboot)
                Console.WriteLine("Reboot is required by the icon registration routine");
            return 0;
        }

        static int RunMounting()
        {
            SetupCBFS();

            // attempt to create the storage
            try
            {
                mCBFSConn.CreateStorage();
            }
            catch (CBFSConnectException e)
            {
                Console.WriteLine("Failed to create storage, error {0} ({1})", e.Code, e.Message);
                return e.Code;
            }

            // Mount a "media"
            try
            {
                mCBFSConn.MountMedia(0);
            }
            catch (CBFSConnectException e)
            {
                Console.WriteLine("Failed to mount media, error {0} ({1})", e.Code, e.Message);
                return e.Code;
            }

            // Add a mounting point
            try
            {
                if (mOptMountingPoint.Length == 1)
                    mOptMountingPoint = mOptMountingPoint + ":";
                AddMountingPoint(mOptMountingPoint);
            }
            catch (CBFSConnectException e)
            {
                Console.WriteLine("Failed to add mounting point, error {0} ({1})", e.Code, e.Message);
                return e.Code;
            }

            // Set the icon
            if (mCBFSConn.IsIconRegistered("sample_icon"))
                mCBFSConn.SetIcon("sample_icon");

            Console.WriteLine("Mounting complete. {0} is now accessible.", mOptMountingPoint);

            while (true)
            {
                Console.WriteLine("To unmount the drive and exit, press Enter");
                Console.ReadLine();

                // remove a mounting point
                try
                {
                    mCBFSConn.RemoveMountingPoint(-1, mOptMountingPoint, 0, 0);
                }
                catch (CBFSConnectException e)
                {
                    Console.WriteLine("Failed to remove mounting point, error {0} ({1})", e.Code, e.Message);
                    continue;
                }

                // delete a storage
                try
                {
                    mCBFSConn.DeleteStorage(true);
                    Console.Write("Disk unmounted.");

                    if (mCBCache.Active)
                    {
                        mCBCache.CloseCache(Constants.FLUSH_IMMEDIATE, Constants.PURGE_NONE);
                    }
                    return 0;
                }
                catch (CBFSConnectException e)
                {
                    Console.WriteLine("Failed to delete storage, error {0} ({1})", e.Code, e.Message);
                    continue;
                }
            }
        }
        #endregion

        #region Cache event handlers
        private static void CBCacheReadData(object sender, CBCacheReadDataEventArgs e)
        {
            e.BytesRead = 0;
            if ((e.Flags & Constants.RWEVENT_CANCELED) == Constants.RWEVENT_CANCELED)
                return;
            if (e.BytesToRead == 0)
                return;

            try
            {
                FileStreamContext Ctx = (FileStreamContext)GCHandle.FromIntPtr(e.FileContext).Target;
                FileStream fs = Ctx.hStream;

                if (fs.SafeFileHandle.IsInvalid)
                {
                    e.ResultCode = Marshal.GetLastWin32Error();
                    return;
                }

                int Count;
                if (e.BytesToRead > int.MaxValue)
                    Count = int.MaxValue;
                else
                    Count = (int)e.BytesToRead;

                fs.Seek(e.Position, SeekOrigin.Begin);
                byte[] data = new byte[Math.Min(BUFFER_SIZE, Count)];
                int TotalBytesRead = 0;
                while (Count > 0)
                {
                    int BytesRead = fs.Read(data, 0, Math.Min(Count, data.Length));
                    if (BytesRead <= 0)
                        break;

                    Marshal.Copy(data, 0, new IntPtr(e.Buffer.ToInt64() + TotalBytesRead), BytesRead);
                    Count -= BytesRead;
                    TotalBytesRead += BytesRead;
                }
                e.BytesRead = TotalBytesRead;

                if (e.BytesRead == 0)
                {
                    e.ResultCode = Constants.RWRESULT_FILE_FAILURE;
                }
                else
                if (e.BytesRead < e.BytesToRead)
                {
                    e.ResultCode = Constants.RWRESULT_PARTIAL;
                }
            }
            catch (Exception ex)
            {
                e.ResultCode = Constants.RWRESULT_FILE_FAILURE;
            }
        }

        private static void CBCacheWriteData(object sender, CBCacheWriteDataEventArgs e)
        {
            e.BytesWritten = 0;
            if ((e.Flags & Constants.RWEVENT_CANCELED) == Constants.RWEVENT_CANCELED)
                return;
            if (e.BytesToWrite == 0)
                return;

            try
            {
                FileStreamContext Ctx = (FileStreamContext)GCHandle.FromIntPtr(e.FileContext).Target;
                FileStream fs = Ctx.hStream;

                if (fs.SafeFileHandle.IsInvalid)
                {
                    e.ResultCode = Marshal.GetLastWin32Error();
                    return;
                }

                int Count;
                if (e.BytesToWrite > int.MaxValue)
                    Count = int.MaxValue;
                else
                    Count = (int)e.BytesToWrite;

                fs.Seek(e.Position, SeekOrigin.Begin);
                long CurrentPosition = fs.Position;

                byte[] data = new byte[Math.Min(BUFFER_SIZE, Count)];
                int TotalBytesWritten = 0;
                while (Count > 0)
                {
                    int BytesToWrite = Math.Min(Count, data.Length);
                    Marshal.Copy(new IntPtr(e.Buffer.ToInt64() + TotalBytesWritten), data, 0, BytesToWrite);

                    fs.Write(data, 0, BytesToWrite);

                    Count -= BytesToWrite;
                    TotalBytesWritten += BytesToWrite;
                }

                e.BytesWritten = (int)(fs.Position - CurrentPosition);

                fs.Flush();
                if (e.BytesWritten == 0)
                {
                    e.ResultCode = Constants.RWRESULT_FILE_FAILURE;
                }
                else
                if (e.BytesWritten < e.BytesToWrite)
                {
                    e.ResultCode = Constants.RWRESULT_PARTIAL;
                }
            }
            catch (Exception ex)
            {
                e.ResultCode = Constants.RWRESULT_FILE_FAILURE;
            }
        }

        #endregion

        #region Event handlers
        private static void CBMount(object sender, CBFSMountEventArgs e)
        {

        }

        private static void CBUnmount(object sender, CBFSUnmountEventArgs e)
        {

        }

        private static void CBGetVolumeSize(object sender, CBFSGetVolumeSizeEventArgs e)
        {
            //CBFS fs = sender as CBFS;
            ulong freeBytesAvailable;
            ulong totalNumberOfBytes;
            ulong totalNumberOfFreeBytes;

            try
            {
                if (GetDiskFreeSpaceEx(mRootPath, out freeBytesAvailable, out totalNumberOfBytes, out totalNumberOfFreeBytes) != true)
                {
                    freeBytesAvailable = 0;
                    totalNumberOfBytes = 0;
                    totalNumberOfFreeBytes = 0;
                }

                e.AvailableSectors = (long)totalNumberOfFreeBytes / SECTOR_SIZE;
                e.TotalSectors = (long)totalNumberOfBytes / SECTOR_SIZE;
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static void CBGetVolumeLabel(object sender, CBFSGetVolumeLabelEventArgs e)
        {
            e.Buffer = "Folder Drive";
        }

        private static void CBSetVolumeLabel(object sender, CBFSSetVolumeLabelEventArgs e)
        {
        }

        private static void CBGetVolumeId(object sender, CBFSGetVolumeIdEventArgs e)
        {
            e.VolumeId = 0x12345678;
        }

        private static void CBCreateFile(object sender, CBFSCreateFileEventArgs e)
        {
            bool isDirectory = false;
            bool IsStream = e.FileName.Contains(":");

            try
            {
                FileStreamContext Ctx = new FileStreamContext();

                isDirectory = (e.Attributes & (uint)CBFileAttributes.Directory) != 0;

                if (isDirectory)
                {
                    if (IsStream)
                    {
                        e.ResultCode = ERROR_DIRECTORY;
                        return;
                    }

                    Directory.CreateDirectory(mRootPath + e.FileName);

                    e.FileContext = GCHandle.ToIntPtr(GCHandle.Alloc(Ctx));
                }
                else
                {
                    Ctx.hStream = new FileStream(mRootPath + e.FileName, FileMode.Create, FileAccess.ReadWrite, FileShare.ReadWrite | FileShare.Delete);

                    if (!IsStream)
                        File.SetAttributes(mRootPath + e.FileName, (FileAttributes)(e.Attributes & 0xFFFF)); // Attributes contains creation flags as well, which we need to strip
                    e.FileContext = GCHandle.ToIntPtr(GCHandle.Alloc(Ctx));
                    mCBCache.FileOpenEx(e.FileName, 0, false, 0, 0, Constants.PREFETCH_NOTHING, e.FileContext);
                }
                Ctx.IncrementCounter();
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static void CBCloseFile(object sender, CBFSCloseFileEventArgs e)
        {
            try
            {
                if (e.FileContext != IntPtr.Zero)
                {
                    FileStreamContext Ctx = (FileStreamContext)GCHandle.FromIntPtr(e.FileContext).Target;

                    if (Ctx.OpenCount == 1)
                    {
                        mCBCache.FileCloseEx(e.FileName, Constants.FLUSH_IMMEDIATE, Constants.PURGE_NONE);
                        // We close the stream in CBCloseFile because this particular call to FileCloseEx is fully synchronous - it flushes and purges the cached data immediately. 
                        // If flushing is postponed, then a handler of the CBCache's WriteData event would have to check for the completion or cancellation of writing, and close the stream there. 
                        // Also, the order of calls is important - first, the file is closed in the cache, then the backend stream is closed.
                        if (Ctx.hStream != null)
                            Ctx.hStream.Close();
                        GCHandle.FromIntPtr(e.FileContext).Free();
                    }
                    else
                    {
                        Ctx.DecrementCounter();
                    }
                }
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static bool isWriteRightForAttrRequested(CBFileAccess access)
        {
            CBFileAccess writeRight = CBFileAccess.file_write_data |
                                      CBFileAccess.file_append_data |
                                      CBFileAccess.file_write_ea;
            return ((access & writeRight) == 0) && ((access & CBFileAccess.file_write_attributes) != 0);
        }
        private static bool isReadRightForAttrOrDeleteRequested(CBFileAccess access)
        {
            CBFileAccess readRight = CBFileAccess.file_read_data | CBFileAccess.file_read_ea;
            CBFileAccess readAttrOrDeleteRight = CBFileAccess.file_read_attributes | CBFileAccess.Delete;

            return ((access & readRight) == 0) && ((access & readAttrOrDeleteRight) != 0);
        }

        private static void CBOpenFile(object sender, CBFSOpenFileEventArgs e)
        {

            FileStreamContext Ctx = null;
            long FileId = -1;

            try
            {

                if (e.FileContext != IntPtr.Zero)
                {
                    Ctx = (FileStreamContext)GCHandle.FromIntPtr(e.FileContext).Target;
                    if (Ctx.hStream != null && isWriteRightRequested((CBFileAccess)e.DesiredAccess))
                    {
                        if (!Ctx.hStream.CanWrite) throw new UnauthorizedAccessException();
                    }
                    Ctx.IncrementCounter();
                    return;
                }

                Ctx = new FileStreamContext();

                FileAttributes attributes = 0;

                if (!e.FileName.Contains(":"))
                    attributes = File.GetAttributes(mRootPath + e.FileName);

                Boolean directoryFile = (attributes & System.IO.FileAttributes.Directory) == System.IO.FileAttributes.Directory;
                Boolean reparsePoint = (attributes & System.IO.FileAttributes.ReparsePoint) == System.IO.FileAttributes.ReparsePoint;
                Boolean openAsReparsePoint = ((e.Attributes & FILE_FLAG_OPEN_REPARSE_POINT) == FILE_FLAG_OPEN_REPARSE_POINT);

                if ((e.NTCreateDisposition == FILE_OPEN) || (e.NTCreateDisposition == FILE_OPEN_IF))
                {
                    if ((directoryFile && !Directory.Exists(mRootPath + e.FileName)) ||
                        (!directoryFile && !File.Exists(mRootPath + e.FileName)))
                    {
                        throw new FileNotFoundException();
                    }
                }

                if (!directoryFile)
                {

                    try
                    {
                        Ctx.hStream = new FileStream(mRootPath + e.FileName, FileMode.Open, FileAccess.ReadWrite, FileShare.ReadWrite | FileShare.Delete);
                    }
                    catch (UnauthorizedAccessException)
                    {
                        CBFileAccess acc = (CBFileAccess)e.DesiredAccess;
                        if (!isReadRightForAttrOrDeleteRequested(acc))
                        {
                            if (isWriteRightForAttrRequested(acc) || !isWriteRightRequested(acc))
                                Ctx.hStream = new FileStream(mRootPath + e.FileName, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                            else
                                throw;
                        }
                    }

                    Ctx.IncrementCounter();
                    e.FileContext = GCHandle.ToIntPtr(GCHandle.Alloc(Ctx));
                    mCBCache.FileOpen(e.FileName, Ctx.hStream.Length, Constants.PREFETCH_NOTHING, e.FileContext);

                    if ((e.NTCreateDisposition != FILE_OPEN) && (e.NTCreateDisposition != FILE_OPEN_IF))
                        File.SetAttributes(mRootPath + e.FileName, (FileAttributes)(e.Attributes & 0xFFFF)); // Attributes contains creation flags as well, which we need to strip

                }
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static void CBGetFileInfo(object sender, CBFSGetFileInfoEventArgs e)
        {
            BY_HANDLE_FILE_INFORMATION fi;
            e.FileExists = false;
            System.IO.FileAttributes attr;
            long FileId = e.FileId;

            try
            {
                string FileName = e.FileName;
                string FullName = mRootPath + FileName;

                try
                {
                    attr = File.GetAttributes(FullName);
                }
                catch (FileNotFoundException)
                {
                    e.FileExists = false;
                    return;
                }

                e.CreationTime = File.GetCreationTimeUtc(FullName);
                e.LastAccessTime = File.GetLastAccessTimeUtc(FullName);
                e.LastWriteTime = File.GetLastWriteTimeUtc(FullName);

                if ((attr & System.IO.FileAttributes.Directory) == System.IO.FileAttributes.Directory)
                    e.Size = 0;
                else
                {
                    FileInfo info = new FileInfo(FullName);
                    e.Size = info.Length;
                    try
                    {
                        if (mCBCache.FileExists(e.FileName))
                            e.Size = mCBCache.FileGetSize(e.FileName);
                    }
                    catch
                    {
                    }
                }

                e.Attributes = (uint)attr;
                e.AllocationSize = e.Size;
                e.HardLinkCount = 1;
                e.FileId = 0;

                e.FileExists = true; // if the file doesn't exist, we have already left the handler
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static void CBEnumerateDirectory(object sender, CBFSEnumerateDirectoryEventArgs e)
        {
            DirectoryEnumerationContext context = null;

            try
            {

                if (e.Restart && (e.EnumerationContext != IntPtr.Zero))
                {
                    if (GCHandle.FromIntPtr(e.EnumerationContext).IsAllocated)
                    {
                        GCHandle.FromIntPtr(e.EnumerationContext).Free();
                    }

                    e.EnumerationContext = IntPtr.Zero;
                }
                if (e.EnumerationContext.Equals(IntPtr.Zero))
                {
                    context = new DirectoryEnumerationContext(mRootPath + e.DirectoryName, e.Mask);

                    GCHandle gch = GCHandle.Alloc(context);

                    e.EnumerationContext = GCHandle.ToIntPtr(gch);
                }
                else
                {
                    context = (DirectoryEnumerationContext)GCHandle.FromIntPtr(e.EnumerationContext).Target;
                }
                e.FileFound = false;

                FileSystemInfo finfo;

                while (e.FileFound = context.GetNextFileInfo(out finfo))
                {
                    if (finfo.Name != "." && finfo.Name != "..") break;
                }
                if (e.FileFound)
                {
                    e.FileName = finfo.Name;

                    e.CreationTime = finfo.CreationTime;
                    e.LastAccessTime = finfo.LastAccessTime;
                    e.LastWriteTime = finfo.LastWriteTime;

                    if ((finfo.Attributes & System.IO.FileAttributes.Directory) != 0)
                    {
                        e.Size = 0;
                    }
                    else
                    {
                        e.Size = ((FileInfo)finfo).Length;
                        try
                        {
                            string FileName = Path.Combine(e.DirectoryName, finfo.Name);
                            if (mCBCache.FileExists(FileName))
                                e.Size = mCBCache.FileGetSize(FileName);
                        }
                        catch
                        {
                        }
                    }

                    e.AllocationSize = e.Size;

                    e.Attributes = Convert.ToUInt32(finfo.Attributes);

                    e.FileId = 0;

                    //
                    // if reparse points supported then get reparse tag for the file
                    //
                    if ((finfo.Attributes & System.IO.FileAttributes.ReparsePoint) != 0)
                    {

                        if (mCBFSConn.UseReparsePoints)
                        {
                            WIN32_FIND_DATA wfd = new WIN32_FIND_DATA();
                            IntPtr hFile = FindFirstFile(mRootPath + e.DirectoryName + '\\' + e.FileName, out wfd);
                            if (hFile != new IntPtr(-1))
                            {
                                e.ReparseTag = wfd.dwReserved0;
                                FindClose(hFile);
                            }
                            //REPARSE_DATA_BUFFER rdb = new REPARSE_DATA_BUFFER();
                            //IntPtr rdbMem = Marshal.AllocHGlobal(Marshal.SizeOf(rdb));
                            //uint rdbLen = (uint)Marshal.SizeOf(rdb);
                            //CBGetReparseData((CBFSConnect)sender, DirectoryInfo.FileName + '\\' + FileName, rdbMem, ref rdbLen);
                            //Marshal.PtrToStructure(rdbMem, rdb);
                            //Marshal.FreeHGlobal(rdbMem);
                            //ReparseTag = rdb.ReparseTag;
                        }
                    }
                    //
                    // If FileId is supported then get FileId for the file.
                    //
                    if (mGetFileNameByFileIdEventSet)
                    {
                        BY_HANDLE_FILE_INFORMATION fi;
                        SafeFileHandle hFile = CreateFile(mRootPath + e.DirectoryName + '\\' + e.FileName, READ_CONTROL, 0, IntPtr.Zero, FileMode.Open, FILE_FLAG_BACKUP_SEMANTICS, IntPtr.Zero);

                        if (!hFile.IsInvalid)
                        {
                            if (GetFileInformationByHandle(hFile, out fi) == true)
                                e.FileId = (Int64)((((UInt64)fi.nFileIndexHigh) << 32) | ((UInt64)fi.nFileIndexLow));

                            hFile.Close();
                        }
                        // else
                        // e.ResultCode = Marshal.GetLastWin32Error();
                    }
                }
                else
                {

                    if (GCHandle.FromIntPtr(e.EnumerationContext).IsAllocated)
                    {
                        GCHandle.FromIntPtr(e.EnumerationContext).Free();
                    }

                    e.EnumerationContext = IntPtr.Zero;
                }
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static void CBCloseDirectoryEnumeration(object sender, CBFSCloseDirectoryEnumerationEventArgs e)
        {
            try
            {
                if (e.EnumerationContext != IntPtr.Zero)
                {
                    if (GCHandle.FromIntPtr(e.EnumerationContext).IsAllocated)
                    {
                        GCHandle.FromIntPtr(e.EnumerationContext).Free();
                    }
                }
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static void CBSetAllocationSize(object sender, CBFSSetAllocationSizeEventArgs e)
        {
            //NOTHING TO DO;
        }

        private static void CBSetFileSize(object sender, CBFSSetFileSizeEventArgs e)
        {
            try
            {
                mCBCache.FileSetSize(e.FileName, e.Size, false);

                int OffsetHigh = Convert.ToInt32(e.Size >> 32 & 0xFFFFFFFF);

                FileStreamContext Ctx = (FileStreamContext)GCHandle.FromIntPtr(e.FileContext).Target;
                FileStream fs = Ctx.hStream;

                if (fs.SafeFileHandle.IsInvalid)
                {
                    e.ResultCode = Marshal.GetLastWin32Error();
                    return;
                }

                fs.Position = e.Size;
                fs.SetLength(e.Size);
                fs.Flush();
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static void CBSetFileAttributes(object sender, CBFSSetFileAttributesEventArgs e)
        {
            try
            {
                string FileName = mRootPath + e.FileName;

                Boolean directoryFile = (File.GetAttributes(FileName) & System.IO.FileAttributes.Directory) == System.IO.FileAttributes.Directory;
                //
                // the case when FileAttributes == 0 indicates that file attributes
                // not changed during this callback
                //
                if (e.Attributes != 0)
                {
                    File.SetAttributes(FileName, (FileAttributes)e.Attributes);
                }

                DateTime CreationTime = e.CreationTime;
                if (CreationTime != ZERO_FILETIME)
                {
                    if (directoryFile)
                        Directory.SetCreationTimeUtc(FileName, CreationTime);
                    else
                        File.SetCreationTimeUtc(FileName, CreationTime);
                }

                FileInfo fi = new FileInfo(FileName);
                bool ro = fi.IsReadOnly;
                FileAttributes attr = File.GetAttributes(FileName);

                if (ro)
                    File.SetAttributes(FileName, attr & ~FileAttributes.ReadOnly);

                DateTime LastAccessTime = e.LastAccessTime;
                if (LastAccessTime != ZERO_FILETIME)
                {
                    if (directoryFile)
                        Directory.SetLastAccessTimeUtc(FileName, LastAccessTime);
                    else
                        File.SetLastAccessTimeUtc(FileName, LastAccessTime);
                }

                DateTime LastWriteTime = e.LastWriteTime;
                if (LastWriteTime != ZERO_FILETIME)
                {
                    if (directoryFile)
                        Directory.SetLastWriteTimeUtc(FileName, LastWriteTime);
                    else
                        File.SetLastWriteTimeUtc(FileName, LastWriteTime);
                }

                if (ro)
                    File.SetAttributes(FileName, attr);
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static void CBCanFileBeDeleted(object sender, CBFSCanFileBeDeletedEventArgs e)
        {
            e.CanBeDeleted = true;
            bool IsStream = e.FileName.Contains(":");
            if (IsStream)
                return;
            if (((File.GetAttributes(mRootPath + e.FileName) & FileAttributes.Directory) != 0) &&
                ((File.GetAttributes(mRootPath + e.FileName) & FileAttributes.ReparsePoint) == 0))
            {
                var info = new DirectoryInfo(mRootPath + e.FileName);

                if (info.GetFileSystemInfos().Length > 0)
                {
                    e.CanBeDeleted = false;
                    e.ResultCode = ERROR_DIR_NOT_EMPTY;
                }
            }
        }

        private static void CBDeleteFile(object sender, CBFSDeleteFileEventArgs e)
        {
            try
            {
                string fullName = mRootPath + e.FileName;
                bool IsStream = e.FileName.Contains(":");

                if (mEnumerateNamedStreamsEventSet && IsStream)
                {
                    if (!DeleteFile(fullName))
                    {
                        if (Marshal.GetLastWin32Error() == ERROR_FILE_NOT_FOUND)
                            throw new FileNotFoundException(fullName);
                        else
                            throw new Exception(fullName);
                    }
                }
                else
                {
                    FileSystemInfo info = null;
                    bool isDir = (File.GetAttributes(fullName) & FileAttributes.Directory) != 0;

                    if (isDir)
                    {
                        info = new DirectoryInfo(fullName);
                    }
                    else
                    {
                        info = new FileInfo(fullName);
                    }

                    info.Delete();
                    mCBCache.FileDelete(e.FileName);
                }
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static void CBRenameOrMoveFile(object sender, CBFSRenameOrMoveFileEventArgs e)
        {
            try
            {
                string oldName = mRootPath + e.FileName;
                string newName = mRootPath + e.NewFileName;
                if ((File.GetAttributes(oldName) & FileAttributes.Directory) != 0)
                {
                    DirectoryInfo dirinfo = new DirectoryInfo(oldName);
                    dirinfo.MoveTo(newName);

                }
                else
                {
                    FileInfo finfo = new FileInfo(oldName);

                    FileInfo finfo1 = new FileInfo(newName);

                    if (finfo1.Exists)
                    {
                        finfo1.Delete();
                    }
                    finfo.MoveTo(newName);

                    if (mCBCache.FileExists(e.NewFileName))
                    {
                        mCBCache.FileDelete(e.NewFileName);
                    }
                    mCBCache.FileChangeId(e.FileName, e.NewFileName);
                }
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static void CBReadFile(object sender, CBFSReadFileEventArgs e)
        {
            e.BytesRead = 0;
            try
            {
                int Count;
                if (e.BytesToRead > int.MaxValue)
                    Count = int.MaxValue;
                else
                    Count = (int)e.BytesToRead;

                byte[] data = new byte[Math.Min(BUFFER_SIZE, Count)];
                long CurrPos = e.Position;
                int TotalBytesRead = 0;
                while (Count > 0)
                {
                    int BytesToRead = Math.Min(Count, data.Length);
                    if (!mCBCache.FileRead(e.FileName, CurrPos, data, 0, BytesToRead))
                        break;

                    Marshal.Copy(data, 0, new IntPtr(e.Buffer.ToInt64() + TotalBytesRead), BytesToRead);
                    Count -= BytesToRead;
                    CurrPos += BytesToRead;
                    TotalBytesRead += BytesToRead;
                }
                e.BytesRead = TotalBytesRead;
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static void CBWriteFile(object sender, CBFSWriteFileEventArgs e)
        {
            e.BytesWritten = 0;
            try
            {
                int Count;
                if (e.BytesToWrite > int.MaxValue)
                    Count = int.MaxValue;
                else
                    Count = (int)e.BytesToWrite;

                byte[] data = new byte[Math.Min(BUFFER_SIZE, Count)];
                long CurrPos = e.Position;
                int TotalBytesWritten = 0;
                while (Count > 0)
                {
                    int BytesToWrite = Math.Min(Count, data.Length);
                    Marshal.Copy(new IntPtr(e.Buffer.ToInt64() + TotalBytesWritten), data, 0, BytesToWrite);

                    if (!mCBCache.FileWrite(e.FileName, CurrPos, data, 0, BytesToWrite))
                        break;

                    Count -= BytesToWrite;
                    CurrPos += BytesToWrite;
                    TotalBytesWritten += BytesToWrite;
                }
                e.BytesWritten = TotalBytesWritten;
                // mCBCache.FileFlush(e.FileName, Constants.FLUSH_MODE_SYNC, 0, e.FileContext);
            }
            catch (Exception ex)
            {
                e.ResultCode = ExceptionToErrorCode(ex);
            }
        }

        private static void CBIsDirectoryEmpty(object sender, CBFSIsDirectoryEmptyEventArgs e)
        {
            // Commented out because the used mechanism is very inefficient 
            /*
            DirectoryInfo dir = new DirectoryInfo(mRootPath + e.DirectoryName);
            FileSystemInfo[] fi = dir.GetFileSystemInfos();
            e.IsEmpty = (fi.Length == 0) ? true : false;
            */
        }

        static void CBFsctl(object sender, CBFSFsctlEventArgs e)
        {
            e.ResultCode = ERROR_INVALID_FUNCTION;
        }

        static void CBEjectStorage(object Sender, CBFSEjectedEventArgs e)
        {
        }

        static Int64 GetFileId(string Path)
        {
            Int64 FileId = 0;
            BY_HANDLE_FILE_INFORMATION fi;
            SafeFileHandle FileHandle = CreateFile(Path, READ_CONTROL, 0, IntPtr.Zero, FileMode.Open, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, IntPtr.Zero);

            if (!FileHandle.IsInvalid)
            {
                if (GetFileInformationByHandle(FileHandle, out fi))
                {
                    FileId = fi.nFileIndexHigh;
                    FileId <<= 32;
                    FileId |= fi.nFileIndexLow;
                }
                FileHandle.Close();
            }
            return FileId;
        }

        #endregion
    }

    public class DirectoryEnumerationContext
    {
        private DirectoryEnumerationContext()
        {

        }
        public bool GetNextFileInfo(out FileSystemInfo info)
        {
            bool Result = false;
            info = null;
            if (mIndex < mFileList.Length)
            {
                info = mFileList[mIndex];
                ++mIndex;
                info.Refresh();
                Result = info.Exists;
            }
            return Result;
        }

        public DirectoryEnumerationContext(string DirName, string Mask)
        {
            DirectoryInfo dirinfo = new DirectoryInfo(DirName);

            mFileList = dirinfo.GetFileSystemInfos(Mask);

            mIndex = 0;
        }
        private FileSystemInfo[] mFileList;
        private int mIndex;
    } // class DirectoryEnumerationContext

}




class ConsoleDemo
{
  public static System.Collections.Generic.Dictionary<string, string> ParseArgs(string[] args)
  {
    System.Collections.Generic.Dictionary<string, string> dict = new System.Collections.Generic.Dictionary<string, string>();

    for (int i = 0; i < args.Length; i++)
    {
      // If it starts with a "/" check the next argument.
      // If the next argument does NOT start with a "/" then this is paired, and the next argument is the value.
      // Otherwise, the next argument starts with a "/" and the current argument is a switch.

      // If it doesn't start with a "/" then it's not paired and we assume it's a standalone argument.

      if (args[i].StartsWith("/"))
      {
        // Either a paired argument or a switch.
        if (i + 1 < args.Length && !args[i + 1].StartsWith("/"))
        {
          // Paired argument.
          dict.Add(args[i].TrimStart('/'), args[i + 1]);
          // Skip the value in the next iteration.
          i++;
        }
        else
        {
          // Switch, no value.
          dict.Add(args[i].TrimStart('/'), "");
        }
      }
      else
      {
        // Standalone argument. The argument is the value, use the index as a key.
        dict.Add(i.ToString(), args[i]);
      }
    }
    return dict;
  }

  public static string Prompt(string prompt, string defaultVal)
  {
    Console.Write(prompt + (defaultVal.Length > 0 ? " [" + defaultVal + "]": "") + ": ");
    string val = Console.ReadLine();
    if (val.Length == 0) val = defaultVal;
    return val;
  }
}