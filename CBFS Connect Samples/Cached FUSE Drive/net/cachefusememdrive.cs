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
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;

using callback.CBFSConnect;
using System.Collections;
using System.Threading;

namespace callback.Demos
{
    class fusememdriveDemo
    {
        private const string mGuid = "{713CC6CE-B3E2-4fd9-838D-E28F558F6866}";
        private static string mOptDrvPath = "";
        private static string mOptMountingPoint = "";

        private static bool isWindows = false;
        private static bool isLinux = false;

        private static FUSE mFUSE = null;
        private static CBCache mCBCache = null;

        private static VirtualFile g_DiskContext = null;

        // g_DiskContextHandle locks the disk context, when its value is passed to the unmanaged code
        private static int g_DiskContextHandle = 0;

        private const int FALLOC_FL_KEEP_SIZE = 0x1;

        private const int BUFFER_SIZE = 1024 * 1024; // 1 MB

        private const int DRIVE_SIZE = 64 * 1024 * 1024;

        private static readonly DateTime ZERO_FILETIME = new DateTime(1601, 1, 1, 0, 0, 0, DateTimeKind.Utc);

        private const int ERROR_PRIVILEGE_NOT_HELD = 1314;

        private const int ESRCH = 3;
        private const int EPERM = 1;
        private const int ENOENT = 2;
        private const int EINTR = 4;
        private const int EIO = 5;
        private const int ENXIO = 6;
        private const int E2BIG = 7;
        private const int ENOEXEC = 8;
        private const int EBADF = 9;
        private const int ECHILD = 10;
        private const int EAGAIN = 11;
        private const int ENOMEM = 12;
        private const int EACCES = 13;
        private const int EFAULT = 14;
        private const int EBUSY = 16;
        private const int EEXIST = 17;
        private const int EXDEV = 18;
        private const int ENODEV = 19;
        private const int ENOTDIR = 20;
        private const int EISDIR = 21;
        private const int ENFILE = 23;
        private const int EMFILE = 24;
        private const int ENOTTY = 25;
        private const int EFBIG = 27;
        private const int ENOSPC = 28;
        private const int ESPIPE = 29;
        private const int EROFS = 30;
        private const int EMLINK = 31;
        private const int EPIPE = 32;
        private const int EDOM = 33;
        private const int EDEADLK = 36;
        private const int ENAMETOOLONG = 38;
        private const int ENOLCK = 39;
        private const int ENOSYS = 40;
        private const int ENOTEMPTY = 41;

        private const int EOPNOTSUPP = 95;

        public enum CBDriverState
        {
            MODULE_STATUS_NOT_PRESENT = 0,
            MODULE_STATUS_STOPPED = 1,
            MODULE_STATUS_RUNNING = 4
        }

        //-----------------------------------------------------------------------------
        static void Usage()
        {
            if (isWindows)
            {
                Console.WriteLine("Usage: fusememdrive [-<switch 1> ... -<switch N>] <mounting point>");
                Console.WriteLine("<Switches>");
                Console.WriteLine("  -drv <cab_file> - Install drivers from CAB file");
            }
            else
            if (isLinux)
            {
                Console.WriteLine("Usage: fusememdrive <mounting point>");
            }
            else
                Console.WriteLine("This fusememdrive sample can't operate - the OS is not detected");
        }

        static int HandleParameters(string[] args)
        {
            int i = 0;
            string curParam;

            while (i < args.Length)
            {
                curParam = args[i];
                if (string.IsNullOrEmpty(curParam))
                    break;

                if (curParam[0] == '-')
                {
                    // driver path
                    if (curParam == "-drv")
                    {
                        if (i + 1 < args.Length)
                        {
                            mOptDrvPath = ConvertRelativePathToAbsolute(args[i + 1]).Trim();
                            if (!string.IsNullOrEmpty(mOptDrvPath) && (Path.GetExtension(mOptDrvPath) == ".cab"))
                            {
                                i += 2;
                                continue;
                            }
                        }
                        if (isWindows) // for non-Windows, we don't care
                        {
                            Console.WriteLine("-drv switch requires the valid path to the .cab file with drivers as the next parameter.");
                            return 1;
                        }
                    }
                }
                else
                {
                    if (curParam.Length == 1)
                    {
                        curParam += ":";
                    }
                    mOptMountingPoint = ConvertRelativePathToAbsolute(curParam, true);
                    if (String.IsNullOrEmpty(mOptMountingPoint))
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

        private static void UpdateDriverStatus()
        {
            if (mFUSE == null)
                mFUSE = new FUSE();

            if (isWindows)
            {
                int status = mFUSE.GetDriverStatus(mGuid);

                if (status != 0)
                {
                    string strStat;

                    switch (status)
                    {
                        case (int)CBDriverState.MODULE_STATUS_NOT_PRESENT:
                            strStat = "not present";
                            break;
                        case (int)CBDriverState.MODULE_STATUS_STOPPED:
                            strStat = "is stopped";
                            break;
                        case (int)CBDriverState.MODULE_STATUS_RUNNING:
                            strStat = "is running";
                            break;
                        default:
                            strStat = "in undefined state";
                            break;
                    }

                    long version = mFUSE.GetDriverVersion(mGuid);
                    Console.WriteLine(string.Format("Driver status: version (ver {0}.{1}.{2}.{3}) installed, service {4}", version >> 48, (version >> 32) & 0xFFFF, (version >> 16) & 0xFFFF, version & 0xFFFF, strStat));
                }
                else
                {
                    Console.WriteLine("Driver not installed");
                }
            }
            else
            if (isLinux)
            {
                long version = mFUSE.GetDriverVersion(mGuid);
                Console.WriteLine(string.Format("Driver status: version (ver {0}.{1}.{2}.{3}) installed", version >> 48, (version >> 32) & 0xFFFF, (version >> 16) & 0xFFFF, version & 0xFFFF));
            }
        }

        static int InstallDriver()
        {
            int drvReboot = 0;
            FUSE disk = new FUSE();

            Console.WriteLine("Installing the drivers from {0}", mOptDrvPath);
            try
            {
                drvReboot = disk.Install(mOptDrvPath, mGuid, null, 0);
            }
            catch (CBFSConnectException err)
            {
                if (err.Code == ERROR_PRIVILEGE_NOT_HELD)
                    Console.WriteLine("Error: Installation requires administrator rights. Run the app as administrator");
                else
                    Console.WriteLine("Error: Driver not installed, error code {1} ({0})", err.Message, err.Code);
                return err.Code;
            }
            Console.WriteLine("Drivers installed successfully");
            if (drvReboot != 0)
                Console.WriteLine("Reboot is required by the driver installation routine");
            return 0;
        }

        static int RunMounting()
        {
            SetupFUSE();

            // attempt to create the storage
            try
            {
                g_DiskContext = new VirtualFile(@"/");
                g_DiskContext.Directory = true;
                g_DiskContextHandle = GCHandle32.Alloc(g_DiskContext);

                mFUSE.Initialize(mGuid);
                mFUSE.Mount(mOptMountingPoint);
            }
            catch (CBFSConnectException e)
            {
                Console.WriteLine("Failed to create storage, error {0} ({1})", e.Code, e.Message);
                return e.Code;
            }

            Console.WriteLine("Now you can use a virtual disk on the mounting point {0}.", mOptMountingPoint);

            while (true)
            {
                Console.WriteLine("To finish, press Enter");
                Console.ReadLine();

                // delete a storage
                try
                {
                    mFUSE.Unmount();
                    GCHandle32.Free(g_DiskContextHandle);
                    Console.Write("Disk unmounted.");
                    if (mCBCache.Active)
                    {
                        mCBCache.CloseCache(Constants.FLUSH_IMMEDIATE, Constants.PURGE_NONE);
                    }
                    return 0;
                }
                catch (CBFSConnectException e)
                {
                    Console.WriteLine("Failed to delete a storage, error {0} ({1})", e.Code, e.Message);
                    continue;
                }
            }
        }

        static void Main(string[] args)
        {
            bool op = false;

            isWindows = System.Runtime.InteropServices.RuntimeInformation.IsOSPlatform(OSPlatform.Windows);

            isLinux = System.Runtime.InteropServices.RuntimeInformation.IsOSPlatform(OSPlatform.Linux);


            if (args.Length < 1)
            {
                Usage();
                UpdateDriverStatus();
                return;
            }

            if (HandleParameters(args) == 0)
            {
                // check the operation
                if (isWindows && (mOptDrvPath != ""))
                {
                    //install the drivers
                    InstallDriver();
                    op = true;
                }
                if (mOptMountingPoint != "")
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

        private static void SetupFUSE()
        {
            UpdateDriverStatus();

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


            if (mFUSE == null)
                mFUSE = new FUSE();

            mFUSE.OnAccess += MFUSE_OnAccess;
            mFUSE.OnChmod += MFUSE_OnChmod;
            mFUSE.OnChown += MFUSE_OnChown;
            mFUSE.OnCopyFileRange += MFUSE_OnCopyFileRange;
            mFUSE.OnCreate += MFUSE_OnCreate;
            mFUSE.OnDestroy += MFUSE_OnDestroy;
            mFUSE.OnError += MFUSE_OnError;
            mFUSE.OnFAllocate += MFUSE_OnFAllocate;
            mFUSE.OnFlush += MFUSE_OnFlush;
            mFUSE.OnFSync += MFUSE_OnFSync;
            mFUSE.OnGetAttr += MFUSE_OnGetAttr;
            mFUSE.OnInit += MFUSE_OnInit;
            mFUSE.OnMkDir += MFUSE_OnMkDir;
            mFUSE.OnOpen += MFUSE_OnOpen;
            mFUSE.OnRead += MFUSE_OnRead;
            mFUSE.OnReadDir += MFUSE_OnReadDir;
            mFUSE.OnRelease += MFUSE_OnRelease;
            mFUSE.OnRename += MFUSE_OnRename;
            mFUSE.OnRmDir += MFUSE_OnRmDir;
            mFUSE.OnStatFS += MFUSE_OnStatFS;
            mFUSE.OnTruncate += MFUSE_OnTruncate;
            mFUSE.OnUnlink += MFUSE_OnUnlink;
            mFUSE.OnUtimens += MFUSE_OnUtimens;
            mFUSE.OnWrite += MFUSE_OnWrite;
            mFUSE.OnLock += MFUSE_OnLock;

        }

        private static int ExceptionToErrorCode(Exception ex)
        {
            if (ex is FileNotFoundException)
                return -ENOENT;
            else if (ex is DirectoryNotFoundException)
                return -ENOENT;
            else if (ex is DriveNotFoundException)
                return -ENOENT;
            else if (ex is OperationCanceledException)
                return -EBADF;
            else if (ex is UnauthorizedAccessException)
                return -EACCES;
            else
                return -EBADF;
        }

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
                VirtualFile vfile = null;
                if (e.FileContext != IntPtr.Zero)
                {
                    VirtualFileHandle handle = GCHandle.FromIntPtr(e.FileContext).Target as VirtualFileHandle;
                    if (handle != null)
                        vfile = handle.File;
                }

                if (vfile == null)
                {
                    e.ResultCode = -ENOENT;
                    return;
                }

                int Count;
                if (e.BytesToRead > int.MaxValue)
                    Count = int.MaxValue;
                else
                    Count = (int)e.BytesToRead;

                byte[] data = new byte[Math.Min(BUFFER_SIZE, Count)];
                int TotalBytesRead = 0;
                while (Count > 0)
                {
                    int BytesRead = 0;
                    vfile.Read(data, e.Position + TotalBytesRead, Math.Min(Count, data.Length), ref BytesRead);
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
            catch (Exception)
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
                VirtualFile vfile = null;
                if (e.FileContext != IntPtr.Zero)
                {
                    VirtualFileHandle handle = GCHandle.FromIntPtr(e.FileContext).Target as VirtualFileHandle;
                    if (handle != null)
                        vfile = handle.File;
                }

                if (vfile == null)
                {
                    e.ResultCode = -ENOENT;
                    return;
                }

                int Count;
                if (e.BytesToWrite > int.MaxValue)
                    Count = int.MaxValue;
                else
                    Count = (int)e.BytesToWrite;

                byte[] data = new byte[Math.Min(BUFFER_SIZE, Count)];
                int TotalBytesWritten = 0;
                while (Count > 0)
                {
                    int BytesToWrite = Math.Min(Count, data.Length);
                    Marshal.Copy(new IntPtr(e.Buffer.ToInt64() + TotalBytesWritten), data, 0, BytesToWrite);

                    int BytesWritten = 0;
                    vfile.Write(data, e.Position + TotalBytesWritten, BytesToWrite, ref BytesWritten);
                    if (BytesWritten <= 0)
                        break;

                    Count -= BytesWritten;
                    TotalBytesWritten += BytesWritten;
                }

                e.BytesWritten = TotalBytesWritten;

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
            catch (Exception)
            {
                e.ResultCode = Constants.RWRESULT_FILE_FAILURE;
            }
        }

        #endregion

        #region FUSE Event handlers
        private static void MFUSE_OnLock(object sender, FUSELockEventArgs e)
        {
            // empty
        }

        private static void MFUSE_OnOpen(object sender, FUSEOpenEventArgs e)
        {
            try
            {
                if (e.FileContext == IntPtr.Zero)
                {
                    VirtualFile vfile = null;
                    if (FindVirtualFile(e.Path, ref vfile))
                    {
                        VirtualFileHandle handle = new VirtualFileHandle(vfile);
                        GCHandle gch = GCHandle.Alloc(handle);
                        e.FileContext = GCHandle.ToIntPtr(gch);

                        mCBCache.FileOpen(e.Path, vfile.Size, Constants.PREFETCH_NOTHING, e.FileContext);
                    }
                    else
                        e.Result = -ENOENT;
                }
                else
                {
                    VirtualFileHandle handle = GCHandle.FromIntPtr(e.FileContext).Target as VirtualFileHandle;
                    handle.AddRef();
                }
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnRelease(object sender, FUSEReleaseEventArgs e)
        {
            try
            {
                if (e.FileContext != IntPtr.Zero)
                {
                    GCHandle gch = GCHandle.FromIntPtr(e.FileContext);
                    VirtualFileHandle handle = gch.Target as VirtualFileHandle;
                    if (handle.DeleteRef())
                    {
                        mCBCache.FileCloseEx(e.Path, Constants.FLUSH_IMMEDIATE, Constants.PURGE_NONE);
                        gch.Free();
                    }
                }
                else
                    e.Result = -EBADF;                
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnRead(object sender, FUSEReadEventArgs e)
        {
            try
            {
                int Count;
                if (e.Size > int.MaxValue)
                    Count = int.MaxValue;
                else
                    Count = (int)e.Size;

                byte[] data = new byte[Math.Min(BUFFER_SIZE, e.Size)];

                long CurrPos = e.Offset;
                int TotalBytesRead = 0;

                while (Count > 0)
                {
                    int BytesToRead = Math.Min(Count, data.Length);
                    if (!mCBCache.FileRead(e.Path, CurrPos, data, 0, BytesToRead))
                        break;

                    Marshal.Copy(data, 0, new IntPtr(e.Buffer.ToInt64() + TotalBytesRead), BytesToRead);
                    Count -= BytesToRead;
                    CurrPos += BytesToRead;
                    TotalBytesRead += BytesToRead;
                }
                e.Result = TotalBytesRead;
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnWrite(object sender, FUSEWriteEventArgs e)
        {
            try
            {
                VirtualFile vfile = null;
                if (e.FileContext != IntPtr.Zero)
                {
                    VirtualFileHandle handle = GCHandle.FromIntPtr(e.FileContext).Target as VirtualFileHandle;
                    if (handle != null)
                        vfile = handle.File;
                }
                if (vfile == null)
                    FindVirtualFile(e.Path, ref vfile);

                if ((vfile == null) || vfile.Directory)
                {
                    e.Result = -ENOENT;
                    return;
                }

                if (e.Offset + e.Size > vfile.AllocationSize)
                    vfile.AllocationSize = e.Offset + e.Size;

                e.Result = 0;

                int Count;
                if (e.Size > int.MaxValue)
                    Count = int.MaxValue;
                else
                    Count = (int)e.Size;

                byte[] data = new byte[Math.Min(BUFFER_SIZE, Count)];
                int TotalBytesWritten = 0;
                long CurrPos = e.Offset;
                while (Count > 0)
                {
                    int BytesToWrite = Math.Min(Count, data.Length);
                    Marshal.Copy(new IntPtr(e.Buffer.ToInt64() + TotalBytesWritten), data, 0, BytesToWrite);

                    if (!mCBCache.FileWrite(e.Path, CurrPos, data, 0, BytesToWrite))
                        break;

                    Count -= BytesToWrite;
                    CurrPos += BytesToWrite;
                    TotalBytesWritten += BytesToWrite;
                }

                e.Result = TotalBytesWritten;
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnTruncate(object sender, FUSETruncateEventArgs e)
        {
            try
            {
                VirtualFile vfile = null;
                if (e.FileContext != IntPtr.Zero)
                {
                    VirtualFileHandle handle = GCHandle.FromIntPtr(e.FileContext).Target as VirtualFileHandle;
                    if (handle != null)
                        vfile = handle.File;
                }
                if (vfile == null)
                    FindVirtualFile(e.Path, ref vfile);

                if ((vfile == null) || vfile.Directory)
                {
                    e.Result = -ENOENT;
                    return;
                }

                vfile.Size = e.Size;
                mCBCache.FileSetSize(e.Path, e.Size, false);
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnUtimens(object sender, FUSEUtimensEventArgs e)
        {
            try
            {
                VirtualFile vfile = null;
                if (e.FileContext != IntPtr.Zero)
                {
                    VirtualFileHandle handle = GCHandle.FromIntPtr(e.FileContext).Target as VirtualFileHandle;
                    if (handle != null)
                        vfile = handle.File;
                }
                if (vfile == null)
                    FindVirtualFile(e.Path, ref vfile);

                if (vfile == null)
                {
                    e.Result = -ENOENT;
                    return;
                }

                if (!ZERO_FILETIME.Equals(e.ATime))
                    vfile.LastAccessTime = e.ATime;

                if (!ZERO_FILETIME.Equals(e.MTime))
                    vfile.LastWriteTime = e.MTime;
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnUnlink(object sender, FUSEUnlinkEventArgs e)
        {
            try
            {
                VirtualFile vfile = null;

                if (FindVirtualFile(e.Path, ref vfile) && !vfile.Directory)
                {
                    vfile.Remove();
                    vfile.Dispose();
                    mCBCache.FileDelete(e.Path);
                }
                else
                    e.Result = -ENOENT;
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnStatFS(object sender, FUSEStatFSEventArgs e)
        {
            e.BlockSize = VirtualFile.SECTOR_SIZE;
            e.FreeBlocks = (DRIVE_SIZE - g_DiskContext.Size + VirtualFile.SECTOR_SIZE / 2) / VirtualFile.SECTOR_SIZE;
            e.FreeBlocksAvail = e.FreeBlocks;
            e.TotalBlocks = DRIVE_SIZE / VirtualFile.SECTOR_SIZE;
            e.MaxFilenameLength = 255;
        }

        private static void MFUSE_OnRmDir(object sender, FUSERmDirEventArgs e)
        {
            try
            {
                VirtualFile vfile = null;

                if (FindVirtualFile(e.Path, ref vfile) && vfile.Directory)
                {
                    vfile.Remove();
                    vfile.Dispose();
                }
                else
                    e.Result = -ENOENT;
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnRename(object sender, FUSERenameEventArgs e)
        {
            try
            {
                VirtualFile vfile = null, vnewfile = null, vdir = null;

                if (!FindVirtualFile(e.OldPath, ref vfile))
                {
                    e.Result = -ENOENT;
                    return;
                }

                if (!GetParentVirtualDirectory(e.NewPath, ref vdir))
                {
                    e.Result = -ENOENT;
                    return;
                }

                if (FindVirtualFile(e.NewPath, ref vnewfile))
                {
                    vnewfile.Remove();
                }

                vfile.Remove();
                vfile.Rename(GetFileName(e.NewPath)); //obtain file name from the path;

                vdir.AddFile(vfile);

                if (mCBCache.FileExists(e.NewPath))
                {
                    mCBCache.FileDelete(e.NewPath);
                }
                mCBCache.FileChangeId(e.OldPath, e.NewPath);

            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnReadDir(object sender, FUSEReadDirEventArgs e)
        {
            try
            {
                VirtualFile vfile = null, child = null;

                if (!FindVirtualFile(e.Path, ref vfile) || !vfile.Directory)
                {
                    e.Result = -ENOENT;
                    return;
                }

                vfile.Context.ResetEnumeration();

                while (vfile.Context.GetNextFile(ref child))
                {
                    long childSize = child.Size;
                    if (!child.Directory)
                    {
                        string FileName = Path.Combine(e.Path, child.Name);
                        if (mCBCache.FileExists(FileName))
                            childSize = mCBCache.FileGetSize(FileName);
                    }
                    e.Result = mFUSE.FillDir(e.FillerContext, child.Name, 0, child.Mode, child.Uid, child.Gid, 1, childSize,
                        child.LastAccessTime, child.LastWriteTime, child.CreationTime);

                    if (e.Result != 0)
                        break;
                }
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnMkDir(object sender, FUSEMkDirEventArgs e)
        {
            try
            {
                VirtualFile vfile = null, vdir = null;

                if (!GetParentVirtualDirectory(e.Path, ref vdir))
                {
                    e.Result = -ENOENT;
                    return;
                }

                if (!vdir.Directory)
                {
                    e.Result = -ENOTDIR;
                    return;
                }

                if (FindVirtualFile(e.Path, ref vfile))
                {
                    e.Result = -EEXIST;
                    return;
                }

                vfile = new VirtualFile(GetFileName(e.Path));

                vfile.Mode = e.Mode;
                vfile.Gid = mFUSE.GetGid();
                vfile.Uid = mFUSE.GetUid();
                vfile.CreationTime = vfile.LastAccessTime = vfile.LastWriteTime = DateTime.UtcNow;

                vdir.AddFile(vfile);
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnInit(object sender, FUSEInitEventArgs e)
        {
            // empty
        }

        private static void MFUSE_OnGetAttr(object sender, FUSEGetAttrEventArgs e)
        {
            try
            {
                VirtualFile vfile = null;
                if (!FindVirtualFile(e.Path, ref vfile))
                {
                    e.Result = -ENOENT;
                    return;
                }

                e.Ino = vfile.Id;
                e.Mode = vfile.Mode;
                e.Gid = vfile.Gid;
                e.Uid = vfile.Uid;
                e.CTime = vfile.CreationTime;
                e.ATime = vfile.LastAccessTime;
                e.MTime = vfile.LastWriteTime;

                if (!vfile.Directory)
                {
                    e.Size = vfile.Size;
                    try
                    {
                        if (mCBCache.FileExists(e.Path))
                            e.Size = mCBCache.FileGetSize(e.Path);
                    }
                    catch
                    {
                    }
                }
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnFSync(object sender, FUSEFSyncEventArgs e)
        {
            // empty
        }

        private static void MFUSE_OnFlush(object sender, FUSEFlushEventArgs e)
        {
            // empty
        }

        private static void MFUSE_OnFAllocate(object sender, FUSEFAllocateEventArgs e)
        {
            try
            {
                VirtualFile vfile = null;
                if (e.FileContext != IntPtr.Zero)
                {
                    VirtualFileHandle handle = GCHandle.FromIntPtr(e.FileContext).Target as VirtualFileHandle;
                    if (handle != null)
                        vfile = handle.File;
                }

                if (vfile == null && !FindVirtualFile(e.Path, ref vfile))
                {
                    e.Result = -ENOENT;
                    return;
                }
                int flags = e.Mode & (~FALLOC_FL_KEEP_SIZE);
                if (flags != 0) // if we detect unsupported flags in Mode, we deny the request
                {
                    e.Result = -EOPNOTSUPP;
                    return;
                }
                if (e.Offset + e.Length >= vfile.Size) // fallocate doesn't shrink files
                {
                    vfile.AllocationSize = e.Offset + e.Length;
                    // fallocate may be used on non-Windows systems to expand file size
                    // Windows component always sets the FALLOC_FL_KEEP_SIZE flag
                    if ((e.Mode & FALLOC_FL_KEEP_SIZE) != FALLOC_FL_KEEP_SIZE)
                    {
                        if (vfile.Size < vfile.AllocationSize)
                            vfile.Size = vfile.AllocationSize;
                    }
                }
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnError(object sender, FUSEErrorEventArgs e)
        {
            string m = String.Format("Error: {0}, Descriptions: {1}", e.ErrorCode, e.Description);
            Console.WriteLine("Error: " + m);
        }

        private static void MFUSE_OnDestroy(object sender, FUSEDestroyEventArgs e)
        {
            // empty
        }

        private static void MFUSE_OnCreate(object sender, FUSECreateEventArgs e)
        {
            try
            {
                VirtualFile vfile = null, vdir = null;

                if (!GetParentVirtualDirectory(e.Path, ref vdir))
                {
                    e.Result = -ENOENT;
                    return;
                }

                vfile = new VirtualFile(GetFileName(e.Path));

                vfile.Mode = e.Mode;
                vfile.CreationTime = vfile.LastAccessTime = vfile.LastWriteTime = DateTime.UtcNow;

                vdir.AddFile(vfile);

                VirtualFileHandle handle = new VirtualFileHandle(vfile);
                GCHandle gch = GCHandle.Alloc(handle);
                e.FileContext = GCHandle.ToIntPtr(gch);
                mCBCache.FileOpenEx(e.Path, 0, false, 0, 0, Constants.PREFETCH_NOTHING, e.FileContext);
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnCopyFileRange(object sender, FUSECopyFileRangeEventArgs e)
        {
            // empty
        }

        private static void MFUSE_OnChown(object sender, FUSEChownEventArgs e)
        {
            try
            {
                VirtualFile vfile = null;
                if (!FindVirtualFile(e.Path, ref vfile))
                {
                    e.Result = -ENOENT;
                    return;
                }

                vfile.Gid = e.Gid;
                vfile.Uid = e.Uid;
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnChmod(object sender, FUSEChmodEventArgs e)
        {
            try
            {
                VirtualFile vfile = null;
                if (!FindVirtualFile(e.Path, ref vfile))
                {
                    e.Result = -ENOENT;
                    return;
                }

                vfile.Mode = e.Mode;
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        private static void MFUSE_OnAccess(object sender, FUSEAccessEventArgs e)
        {
            try
            {
                VirtualFile vfile = null;
                if (!FindVirtualFile(e.Path, ref vfile))
                {
                    e.Result = -ENOENT;
                    return;
                }
            }
            catch (Exception ex)
            {
                e.Result = ExceptionToErrorCode(ex);
            }
        }

        #endregion

        private static bool FindVirtualFile(string FileName, ref VirtualFile vfile)
        {
            bool result = true;
            int i = 0;

            VirtualFile root = g_DiskContext;

            vfile = root;

            if (FileName == "/") return result;

            string[] buffer = FileName.Split(new char[] { '/' });

            while (i < buffer.Length)
            {
                if (result = root.Context.GetFile(buffer[i], ref vfile))
                {
                    root = vfile;
                }
                i++;
            }
            return result;
        }

        private static bool FindVirtualDirectory(string DirName, ref VirtualFile vfile)
        {
            return FindVirtualFile(DirName, ref vfile) && vfile.Directory;
        }

        private static bool GetParentVirtualDirectory(string FileName, ref VirtualFile vfile)
        {
            // string DirName = Path.GetDirectoryName(FileName);
            string DirName;
            int lastpos;
            lastpos = FileName.LastIndexOfAny(new char[] { '\\', '/' });
            if (lastpos == -1)
                return false;
            if (lastpos == 0)
                DirName = "/";
            else
                DirName = FileName.Substring(0, lastpos);
            return FindVirtualDirectory(DirName, ref vfile);
        }

        static string GetFileName(string fullpath)
        {
            string[] buffer = fullpath.Split(new char[] { '/' });
            return buffer[buffer.Length - 1];
        }
    }

    public class DirectoryEnumerationContext : IDisposable
    {
        public DirectoryEnumerationContext()
        {
            mFileList = new ArrayList();
        }

        ~DirectoryEnumerationContext()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            // Check to see if Dispose has already been called. 
            if (this.disposed)
                return;

            if (disposing)
            {
                //
                // Dispose managed resources.
                //
                lock (mFileList.SyncRoot)
                {
                    this.mFileList.Clear();
                }
                this.mFileList = null;
            }
            disposed = true;
        }

        public bool GetNextFile(ref VirtualFile vfile)
        {
            bool Result = false;

            lock (mFileList.SyncRoot)
            {
                if (mIndex < mFileList.Count)
                {
                    vfile = (VirtualFile)mFileList[mIndex];
                    ++mIndex;
                    Result = true;
                }
                else
                {
                    vfile = null;
                }
            }
            return Result;
        }

        public bool GetFile(string FileName, ref VirtualFile vfile)
        {
            lock (mFileList.SyncRoot)
            {
                foreach (VirtualFile vf in mFileList)
                {
                    if (vf.Name.Equals(FileName, StringComparison.CurrentCultureIgnoreCase))
                    {
                        vfile = vf;
                        return true;
                    }
                }
            }
            return false;
        }

        public bool GetFile(int Index, ref VirtualFile vfile)
        {
            bool Result = false;

            lock (mFileList.SyncRoot)
            {
                if (Index < mFileList.Count)
                {
                    vfile = (VirtualFile)mFileList[Index];
                    Result = true;
                }
            }

            return Result;
        }

        public void AddFile(VirtualFile vfile)
        {
            lock (mFileList.SyncRoot)
            {
                if (!mFileList.Contains(vfile))
                {
                    mFileList.Add(vfile);
                    ResetEnumeration();
                }
            }
        }

        public void Remove(VirtualFile vfile)
        {
            lock (mFileList.SyncRoot)
            {
                if (mFileList.Contains(vfile))
                {
                    mFileList.Remove(vfile);
                    ResetEnumeration();
                }
            }
        }

        public void ResetEnumeration()
        {
            mIndex = 0;
        }

        public long Size
        {
            get
            {
                long Total = 0;
                VirtualFile vfile = null;

                for (int i = 0; i < mFileList.Count; i++)
                {
                    if (GetFile(i, ref vfile))
                    {
                        Total += (vfile.AllocationSize + VirtualFile.SECTOR_SIZE - 1) & ~(VirtualFile.SECTOR_SIZE - 1);
                    }
                }

                return Total;
            }
        }

        private ArrayList mFileList;
        private int mIndex = 0;
        private bool disposed = false;
    };

    public class VirtualFile : IDisposable
    {
        public static int MODE_DIR = 0x4000;
        public static int SECTOR_SIZE = 512;

        public VirtualFile(string Name)
        {
            try
            {
                mEnumCtx = new DirectoryEnumerationContext();
                mStream = new MemoryStream();
                Initializer(Name);                
            }
            catch (Exception e)
            {
                Console.WriteLine("Cannot create memory stream. Error {0}", e.Message);
            }
        }

        ~VirtualFile()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            // Check to see if Dispose has already been called. 
            if (this.disposed)
                return;

            if (disposing)
            {
                //
                // Dispose managed resources.
                //
                this.mEnumCtx.Dispose();
                this.mStream.SetLength(0);
                this.mStream.Capacity = 0;
                this.mStream.Close();
                this.mEnumCtx = null;
                this.mName = null;
                this.mStream = null;                

            }
            disposed = true;
        }
        public void AddFile(VirtualFile vfile)
        {
            Context.AddFile(vfile);
            vfile.Parent = this;
        }
                
        public bool Directory
        {
            get
            {
                return (Mode & MODE_DIR) != 0;
            }
            set
            {
                if (value)
                    mMode |= MODE_DIR;
                else
                    mMode &= ~MODE_DIR;
            }
        }
        public long Size
        {
            set
            {
                if (!Directory)
                    lock (this)
                    {
                        mSize = value;
                    }
                else
                    throw new NotSupportedException();
            }
            get
            {
                if (!Directory)
                    return mSize;
                else
                    return mEnumCtx.Size;
            }
        }
        public long AllocationSize
        {
            set
            {
                lock (this)
                {
                    mStream.SetLength(value);
                }
            }
            get
            {
                lock (this)
                {
                    return mStream.Length;
                }
            }
        }
        public string Name
        {
            get
            {
                return (mName);
            }
        }
        public DateTime CreationTime
        {
            get
            {
                return (mCreationTime);
            }
            set
            {
                mCreationTime = value;
            }
        }
        public DateTime LastAccessTime
        {
            get
            {
                return (mLastAccessTime);
            }
            set
            {
                mLastAccessTime = value;
            }
        }
        public DateTime LastWriteTime
        {
            get
            {
                return (mLastWriteTime);
            }
            set
            {
                mLastWriteTime = value;
            }
        }
        public int Mode
        {
            get
            {
                return (mMode);
            }
            set
            {
                mMode = value;
            }
        }
        public int Id
        {
            get
            {
                return (mId);
            }
            set
            {
                mId = value;
            }
        }
        public int Uid
        {
            get
            {
                return (mUid);
            }
            set
            {
                mUid = value;
            }
        }
        public int Gid
        {
            get
            {
                return (mGid);
            }
            set
            {
                mGid = value;
            }
        }
        public void Rename(string NewName)
        {
            mName = NewName;
        }
        public void Remove()
        {
            mParent.Context.Remove(this);
            mParent = null;
        }
        public DirectoryEnumerationContext Context
        {
            get
            {
                return (mEnumCtx);
            }
        }
        public VirtualFile Parent
        {
            get
            {
                return (mParent);
            }
            set
            {
                mParent = value;
            }
        }
        public void Write(byte[] WriteBuf, long Position, int BytesToWrite, ref int BytesWritten)
        {
            lock (this)
            {
                mStream.Seek(Position, SeekOrigin.Begin);

                if (Size - Position < BytesToWrite)
                    Size = Position + BytesToWrite;
             
                long CurrentPosition = mStream.Position;

                mStream.Write(WriteBuf, 0, BytesToWrite);

                BytesWritten = (int)(mStream.Position - CurrentPosition);
            }
        }
        public void Read(byte[] ReadBuf, long Position, int BytesToRead, ref int BytesRead)
        {
            lock (this)
            {
                mStream.Seek(Position, SeekOrigin.Begin);

                BytesRead = mStream.Read(ReadBuf, 0, BytesToRead);
            }
        }
        private void Initializer(string Name)
        {
            mName = Name;
            mCreationTime = DateTime.Now;
            mLastAccessTime = DateTime.Now;
            mLastWriteTime = DateTime.Now;
        }
                
        private DirectoryEnumerationContext mEnumCtx = null;
        private string mName = string.Empty;
        private MemoryStream mStream = null;
        private int mId;
        private int mGid;
        private int mUid;
        private int mMode;
        private DateTime mCreationTime = DateTime.MinValue;
        private DateTime mLastAccessTime = DateTime.MinValue;
        private DateTime mLastWriteTime = DateTime.MinValue;
        private VirtualFile mParent = null;
        private long mSize = 0;
        private bool disposed = false;
    };

    public class VirtualFileHandle
    {
        private VirtualFile mFile = null;
        private int mRefCount = 1;

        public VirtualFileHandle(VirtualFile vfile)
        {
            mFile = vfile;
        }

        public void AddRef()
        {
            Interlocked.Increment(ref mRefCount);
        }

        public bool DeleteRef()
        {
            Interlocked.Decrement(ref mRefCount);
            return mRefCount == 0;
        }

        public VirtualFile File { get { return mFile; } }
    }


    public enum CBDriverState
    {
        MODULE_STATUS_NOT_PRESENT = 0,
        MODULE_STATUS_STOPPED = 1,
        MODULE_STATUS_RUNNING = 4
    }

    class GCHandle32
    {
        static object _lockobj = new object();
        static Dictionary<int, object> _objMap = new Dictionary<int, object>();
        static int _id = 1;

        public static int Alloc(object value)
        {
            int ret;
            lock (_lockobj)
            {
                _objMap.Add(_id, value);
                ret = _id;
                ++_id;
            }
            return ret;
        }

        public static object FromInt(int value)
        {
            object obj;
            lock (_lockobj)
            {
                obj = _objMap[value];
            }
            return obj;
        }

        public static void Free(int value)
        {
            lock (_lockobj)
            {
                _objMap.Remove(value);
            }
        }
    }
}





class ConsoleDemo
{
  /// <summary>
  /// Takes a list of switch arguments or name-value arguments and turns it into a dictionary.
  /// </summary>
  public static System.Collections.Generic.Dictionary<string, string> ParseArgs(string[] args)
  {
    System.Collections.Generic.Dictionary<string, string> dict = new System.Collections.Generic.Dictionary<string, string>();

    for (int i = 0; i < args.Length; i++)
    {
      // Add a key to the dictionary for each argument.
      if (args[i].StartsWith("/"))
      {
        // If the next argument does NOT start with a "/", then it is a value.
        if (i + 1 < args.Length && !args[i + 1].StartsWith("/"))
        {
          // Save the value and skip the next entry in the list of arguments.
          dict.Add(args[i].ToLower().TrimStart('/'), args[i + 1]);
          i++;
        }
        else
        {
          // If the next argument starts with a "/", then we assume the current one is a switch.
          dict.Add(args[i].ToLower().TrimStart('/'), "");
        }
      }
      else
      {
        // If the argument does not start with a "/", store the argument based on the index.
        dict.Add(i.ToString(), args[i].ToLower());
      }
    }
    return dict;
  }
  /// <summary>
  /// Asks for user input interactively and returns the string response.
  /// </summary>
  public static string Prompt(string prompt, string defaultVal)
  {
    Console.Write(prompt + (defaultVal.Length > 0 ? " [" + defaultVal + "]": "") + ": ");
    string val = Console.ReadLine();
    if (val.Length == 0) val = defaultVal;
    return val;
  }
}