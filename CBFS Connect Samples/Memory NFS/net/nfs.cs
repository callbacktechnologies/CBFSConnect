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

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using callback.CBFSConnect;

namespace callback.Demos
{
    class Program
    {
        static readonly NFS cbfs_nfs = new MemDriveNFS();
        static VirtualFile g_DiskContext = null;

        public static class NFSConstants
        {
            public const int baseCookie = 0x12345678; // Base cookie

            public const int S_IFMT = 61440; // Bit mask for the file type bit field
            public const int S_IFSOCK = 49152; // Socket
            public const int S_IFLNK = 40960; // Symbolic link
            public const int S_IFREG = 32768; // Regular file
            public const int S_IFBLK = 24576; // Block device
            public const int S_IFDIR = 16384; // Directory
            public const int S_IFCHR = 8192; // Character device
            public const int S_IFIFO = 4096; // FIFO

            public const int S_ISUID = 2048; // Set user ID bit
            public const int S_ISGID = 1024; // Set group ID bit
            public const int S_ISVTX = 512; // Sticky bit
            public const int S_IRWXU = 448; // Mask for file owner permissions
            public const int S_IRUSR = 256; // Owner has read permission
            public const int S_IWUSR = 128; // Owner has write permission
            public const int S_IXUSR = 64; // Owner has execute permission
            public const int S_IRWXG = 56; // Mask for group permissions
            public const int S_IRGRP = 32; // Group has read permission
            public const int S_IWGRP = 16; // Group has write permission
            public const int S_IXGRP = 8; // Group has execute permission
            public const int S_IRWXO = 7; // Mask for permissions for others (not in group)
            public const int S_IROTH = 4; // Others have read permission
            public const int S_IWOTH = 2; // Others have write permission
            public const int S_IXOTH = 1; // Others have execute permission

            public const int FILE_SYNC4 = 2;
            public const int NFS4ERR_IO = 5;
            public const int NFS4ERR_NOENT = 2;
            public const int NFS4ERR_EXIST = 17;
        }

        static void Main(string[] args)
        {
            // Default NFS port
            int port = 2049;

            Banner();

            if (args.Length < 1)
            {
                Usage();
                return;
            }

            string sPort = args[0];
            if (sPort != "-")
                port = int.Parse(args[0]);

            bool isWindows = RuntimeInformation.IsOSPlatform(OSPlatform.Windows);
            if (args.Length == 2 && isWindows)
            {
                Console.WriteLine("Mounting points are not supported on Windows, only on Linux and macOS");
                return;
            }

            if (g_DiskContext == null)
            {
                // Create root.
                DateTime now = DateTime.UtcNow;
                g_DiskContext = new("/", MemDriveNFS.DIR_MODE)
                {
                    CreationTime = now,
                    LastAccessTime = now,
                    LastWriteTime = now,
                    Size = 4096
                };

                // Add a subdirectory.
                VirtualFile vfile = new("test", MemDriveNFS.DIR_MODE)
                {
                    CreationTime = now,
                    LastAccessTime = now,
                    LastWriteTime = now,
                    Size = 4096
                };

                g_DiskContext.AddFile(vfile);
            }

            if (args.Length == 2)
            {
                cbfs_nfs.Config("MountingPoint=" + args[1]);
            }

            cbfs_nfs.LocalPort = port;
            cbfs_nfs.StartListening();

            Console.WriteLine($"NFS server started on port {port}");
            Console.WriteLine("Press <Enter> to stop the server");

            while (true)
            {
                cbfs_nfs.DoEvents();
                if (!Console.IsInputRedirected && Console.KeyAvailable && Console.ReadKey(true).Key == ConsoleKey.Enter)
                {
                    StopServer();
                    break;
                }
                Thread.Sleep(100);
            }
        }

        static void Usage()
        {
            Console.WriteLine("Usage: nfs [local port or - for default] <mounting point>\n");
            Console.WriteLine("Example 1 (any OS): nfs 2049");
            Console.WriteLine("Example 2 (Linux/macOS): sudo nfs - /mnt/mynfs\n");
            Console.WriteLine("'mount' command should be installed on Linux/macOS to use mounting points!\n");
            Console.WriteLine("Automatic mounting to mounting points is supported only on Linux and macOS.");
            Console.WriteLine("Also, mounting outside of your home directory may require admin rights.\n");
        }

        static void Banner()
        {
            Console.WriteLine("CBFS Connect Copyright (c) Callback Technologies, Inc.\n");
            Console.WriteLine("This demo shows how to create a virtual drive that is stored in memory using the NFS component.\n");
        }

        static void StopServer()
        {
            Console.WriteLine("Stopping server...");
            cbfs_nfs.StopListening();
            Console.WriteLine("Server stopped");

            g_DiskContext?.Clear();                
        }

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
                DirName = FileName[..lastpos];
            return FindVirtualDirectory(DirName, ref vfile);
        }

        static string GetFileName(string fullpath)
        {
            string[] buffer = fullpath.Split(new char[] { '/' });
            return buffer[^1];
        }

        class MemDriveNFS : NFS
        {
            public const int DIR_MODE = NFSConstants.S_IFDIR | NFSConstants.S_IRWXU | NFSConstants.S_IRGRP
                                    | NFSConstants.S_IXGRP | NFSConstants.S_IROTH | NFSConstants.S_IXOTH; // Type -> Directory, Permissions -> 755

            public const int FILE_MODE = NFSConstants.S_IFREG | NFSConstants.S_IWUSR | NFSConstants.S_IRUSR
                                    | NFSConstants.S_IRGRP | NFSConstants.S_IROTH; // Type -> File, Permission -> 644

            public MemDriveNFS() : base()
            {
                OnAccess += new OnAccessHandler(FireAccess);
                OnChmod += new OnChmodHandler(FireChmod);
                OnChown += new OnChownHandler(FireChown);
                OnCreateLink += new OnCreateLinkHandler(FireCreateLink);
                OnReadLink += new OnReadLinkHandler(FireReadLink);
                OnConnectionRequest += new OnConnectionRequestHandler(FireConnectionRequest);
                OnGetAttr += new OnGetAttrHandler(FireGetAttr);
                OnMkDir += new OnMkDirHandler(FireMkDir);
                OnConnected += new OnConnectedHandler(FireConnected);
                OnDisconnected += new OnDisconnectedHandler(FireDisconnected);
                OnLookup += new OnLookupHandler(FireLookup);
                OnOpen += new OnOpenHandler(FireOpen);
                OnRead += new OnReadHandler(FireRead);
                OnReadDir += new OnReadDirHandler(FireReadDir);
                OnRename += new OnRenameHandler(FireRename);
                OnRmDir += new OnRmDirHandler(FireRmDir);
                OnTruncate += new OnTruncateHandler(FireTruncate);
                OnUnlink += new OnUnlinkHandler(FireUnlink);
                OnUTime += new OnUTimeHandler(FireUTime);
                OnWrite += new OnWriteHandler(FireWrite);

                OnError += new OnErrorHandler(FireError);
                OnLog += new OnLogHandler(FireLog);
            }

            private static void FireAccess(object sender, NFSAccessEventArgs e) { return; }
            private static void FireChmod(object sender, NFSChmodEventArgs e) { return; }
            private static void FireChown(object sender, NFSChownEventArgs e) { return; }

            private static void FireCreateLink(object sender, NFSCreateLinkEventArgs e)
            {
                Console.WriteLine($"FireCreateLink: {e.Path}");
                e.Result = Constants.NFS4ERR_NOTSUPP;
            }

            private static void FireReadLink(object sender, NFSReadLinkEventArgs e)
            {
                Console.WriteLine($"FireReadLink: {e.Path}");
                e.Result = Constants.NFS4ERR_NOTSUPP;
            }

            private static void FireError(object sender, NFSErrorEventArgs e)
            {
                Console.WriteLine($"Error: {e.ErrorCode}");
                return;
            }

            private static void FireLog(object sender, NFSLogEventArgs e)
            {
                Console.WriteLine($"{e.ConnectionId}: {e.Message}");
                return;
            }

            private static void FireConnectionRequest(object sender, NFSConnectionRequestEventArgs e)
            {
                e.Accept = true;
                return;
            }

            private static void FireGetAttr(object sender, NFSGetAttrEventArgs e)
            {
                Console.WriteLine($"FireGetAttr: {e.Path}");

                e.Result = Constants.NFS4ERR_NOENT;

                VirtualFile vfile = null;

                if (FindVirtualFile(e.Path, ref vfile))
                {
                    e.Result = 0;
                    e.FileId = vfile.Id;
                    e.LinkCount = 1;
                    e.Group = "0";
                    e.User = "0";
                    e.Size = vfile.Directory ? 4096 : vfile.Size;
                    e.Mode = vfile.Mode;
                    e.CTime = vfile.CreationTime;
                    e.MTime = vfile.LastWriteTime;
                    e.ATime = vfile.LastAccessTime;
                }
            }

            private static void FireMkDir(object sender, NFSMkDirEventArgs e)
            {
                Console.WriteLine($"FireMkDir: {e.Path}");

                VirtualFile vfile = null, vdir = null;
                DateTime now = DateTime.UtcNow;

                if (FindVirtualFile(e.Path, ref vfile))
                {
                    e.Result = Constants.NFS4ERR_EXIST;
                    return;
                }

                if (!GetParentVirtualDirectory(e.Path, ref vdir))
                {
                    e.Result = Constants.NFS4ERR_NOENT;
                    return;
                }

                vfile = new(GetFileName(e.Path), DIR_MODE)
                {
                    CreationTime = now,
                    LastAccessTime = now,
                    LastWriteTime = now                    
                };

                vdir.AddFile(vfile);
            }

            private static void FireConnected(object sender, NFSConnectedEventArgs e)
            {
                Console.WriteLine($"Client connected: {e.StatusCode}: {e.Description}");
            }

            private static void FireDisconnected(object sender, NFSDisconnectedEventArgs e)
            {
                Console.WriteLine($"Client disconnected: {e.StatusCode}: {e.Description}");
            }

            private static void FireLookup(object sender, NFSLookupEventArgs e)
            {
                Console.WriteLine($"FireLookup: {e.Path}");

                VirtualFile vfile = null;

                if (FindVirtualDirectory(e.Path, ref vfile))
                {
                    return;
                }

                if (FindVirtualFile(e.Path, ref vfile))
                {
                    return;
                }

                e.Result = Constants.NFS4ERR_NOENT;
            }

            private static void FireOpen(object sender, NFSOpenEventArgs e)
            {
                Console.WriteLine($"FireOpen: {e.Path}, open type: {e.OpenType}");

                if (e.OpenType == Constants.OPEN4_CREATE)
                {
                    VirtualFile vfile = null, vdir = null;
                    DateTime now = DateTime.UtcNow;

                    if (FindVirtualFile(e.Path, ref vfile))
                    {
                        e.Result = Constants.NFS4ERR_EXIST;
                        return;
                    }

                    if (!GetParentVirtualDirectory(e.Path, ref vdir))
                    {
                        e.Result = Constants.NFS4ERR_NOENT;
                        return;
                    }

                    vfile = new(GetFileName(e.Path), FILE_MODE)
                    {
                        CreationTime = now,
                        LastAccessTime = now,
                        LastWriteTime = now
                    };

                    vdir.AddFile(vfile);
                }
                else
                {
                    VirtualFile vfile = null;

                    if (!FindVirtualFile(e.Path, ref vfile))
                        e.Result = Constants.NFS4ERR_NOENT;
                    
                     vfile.LastAccessTime = DateTime.UtcNow;
                }
            }

            private static void FireRead(object sender, NFSReadEventArgs e)
            {
                Console.WriteLine($"FireRead: {e.Path}");

                if (e.Count == 0) return;

                int bytesRead = 0;
                VirtualFile vfile = null;

                if (FindVirtualFile(e.Path, ref vfile))
                {
                    if (e.Offset >= vfile.Size)
                    {
                        e.Count = 0;
                        e.Eof = true;
                        return;
                    }
                    vfile.Read(e.BufferB, e.Offset, (int)e.Count, ref bytesRead);    
                    if (e.Offset + bytesRead == vfile.Size) e.Eof = true;
                    e.Count = bytesRead;
                }
                else
                    e.Result = Constants.NFS4ERR_NOENT;
            }

            private static void FireReadDir(object sender, NFSReadDirEventArgs e)
            {
                Console.WriteLine($"FireReadDir: {e.Path}");

                VirtualFile vdir = null, vfile = null;
                int readOffset = 0;
                long cookie = NFSConstants.baseCookie;

                // If Cookie != 0, continue listing entries from a specified cookie. Otherwise, start listing entries from the start.
                if (e.Cookie != 0)
                {
                    readOffset = (int)e.Cookie - NFSConstants.baseCookie + 1;
                    cookie = e.Cookie + 1;
                }

                if (FindVirtualDirectory(e.Path, ref vdir))
                {                    
                    for (int i = readOffset; i < vdir.Context.Count; i++)
                    {
                        vdir.Context.GetFile(i, ref vfile);
                        if (cbfs_nfs.FillDir(e.ConnectionId, vfile.Name, vfile.Id, cookie,
                            vfile.Mode, "0", "0", 1,
                            vfile.Directory ? 4096 : vfile.Size, vfile.LastAccessTime,
                            vfile.LastWriteTime, vfile.CreationTime) == 0)
                        {
                            if (i == vdir.Context.Count - 1)
                                break;
                            cookie++;
                        }
                        else
                        {
                            break;
                        }
                    }                    
                }
                else
                    e.Result = Constants.NFS4ERR_NOENT;
            }

            private static void FireRename(object sender, NFSRenameEventArgs e)
            {
                Console.WriteLine($"FireRename: {e.OldPath} -> {e.NewPath}");

                if (e.OldPath.Equals(e.NewPath))
                {
                    return;
                }

                VirtualFile voldfile = null, vnewfile = null, vnewparent = null;

                if (FindVirtualDirectory(e.OldPath, ref voldfile))
                {
                    // Check for compatibility
                    if (FindVirtualFile(e.NewPath, ref vnewfile) && (vnewfile.Mode & NFSConstants.S_IFREG) != 0)
                    {
                        e.Result = Constants.NFS4ERR_EXIST;
                        return;
                    }

                    // Remove directory if it exists and is empty
                    if (FindVirtualDirectory(e.NewPath, ref vnewfile))
                    {
                        if (vnewfile.Context.Count != 0)
                        {
                            e.Result = Constants.NFS4ERR_EXIST;
                            return;
                        }
                        vnewfile.Remove();
                        vnewfile = null;
                    }

                    // Move directory
                    if (GetParentVirtualDirectory(e.NewPath, ref vnewparent))
                    {
                        voldfile.Remove();
                        voldfile.Rename(GetFileName(e.NewPath));
                        vnewparent.AddFile(voldfile);
                    }
                } 
                else 
                if (FindVirtualFile(e.OldPath, ref voldfile))
                {
                    // Check for compatibility
                    if (FindVirtualDirectory(e.NewPath, ref vnewfile))
                    {
                        e.Result = Constants.NFS4ERR_EXIST;
                        return;
                    }

                    // Remove existing file if it exists
                    if (FindVirtualFile(e.NewPath, ref vnewfile))
                    {
                        vnewfile.Remove();
                        vnewfile = null;
                    }

                    // Move file
                    if (GetParentVirtualDirectory(e.NewPath, ref vnewparent))
                    {
                        voldfile.Remove();
                        voldfile.Rename(GetFileName(e.NewPath));
                        vnewparent.AddFile(voldfile);
                    }
                } else
                {
                    e.Result = Constants.NFS4ERR_NOENT;
                }
            }

            private static void FireRmDir(object sender, NFSRmDirEventArgs e)
            {
                Console.WriteLine($"FireRmDir: {e.Path}");

                VirtualFile vfile = null;

                if (FindVirtualFile(e.Path, ref vfile))
                {
                    vfile.Remove();
                    vfile = null;
                }
                else
                    e.Result = Constants.NFS4ERR_NOENT;
            }

            private static void FireTruncate(object sender, NFSTruncateEventArgs e)
            {
                Console.WriteLine($"FireTruncate: {e.Path}");

                VirtualFile vfile = null;

                if (FindVirtualFile(e.Path, ref vfile))
                    vfile.Size = e.Size;
                else
                    e.Result = Constants.NFS4ERR_NOENT;
            }

            private static void FireUnlink(object sender, NFSUnlinkEventArgs e)
            {
                Console.WriteLine($"FireUnlink: {e.Path}");

                VirtualFile vfile = null;

                if (FindVirtualFile(e.Path, ref vfile))
                {
                    vfile.Remove();
                    vfile = null;
                }
                else
                    e.Result = Constants.NFS4ERR_NOENT;
            }

            private static void FireUTime(object sender, NFSUTimeEventArgs e)
            {
                Console.WriteLine($"FireUTime: {e.Path}");

                VirtualFile vfile = null;

                if (FindVirtualFile(e.Path, ref vfile))
                {
                    if (e.ATime.Ticks != 0)
                        vfile.LastAccessTime = e.ATime;
                    if (e.MTime.Ticks != 0)
                        vfile.LastWriteTime = e.MTime;
                }
                else
                    e.Result = Constants.NFS4ERR_NOENT;
            }

            private static void FireWrite(object sender, NFSWriteEventArgs e)
            {
                Console.WriteLine($"FireWrite: {e.Path}");

                if (e.Count == 0) return;

                int bytesWritten = 0;
                VirtualFile vfile = null;

                if (FindVirtualFile(e.Path, ref vfile))
                {
                    vfile.Write(e.BufferB, e.Offset, (int)e.Count, ref bytesWritten);
                    vfile.LastWriteTime = DateTime.UtcNow;

                    e.Count = bytesWritten;
                    e.Stable = NFSConstants.FILE_SYNC4;
                }
                else
                    e.Result = Constants.NFS4ERR_NOENT;
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

            internal void Clear()
            {
                lock (mFileList.SyncRoot)
                {
                    mFileList.Clear();
                }
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
                            Total += (vfile.Size + VirtualFile.SECTOR_SIZE - 1) & ~(VirtualFile.SECTOR_SIZE - 1);
                        }
                    }

                    return Total;
                }
            }

            public int Count
            {
                get 
                { 
                    return mFileList.Count;
                }
            }

            private ArrayList mFileList;
            private int mIndex = 0;
            private bool disposed = false;
        };

        public class VirtualFile : IDisposable
        {
            public static int MODE_DIR = NFSConstants.S_IFDIR;
            public static int SECTOR_SIZE = 512;

            static int fileIdCounter = 0;

            public VirtualFile(string Name, int mode)
            {
                try
                {
                    mEnumCtx = new DirectoryEnumerationContext();
                    mStream = new MemoryStream();
                    Initializer(Name);
                    mMode = mode;
                    mId = Interlocked.Increment(ref fileIdCounter);
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


            internal void Clear()
            {
                mEnumCtx.Clear();
            }
            
            public void AddFile(VirtualFile vfile)
            {
                Context.AddFile(vfile);
                vfile.Parent = this;
            }

            public int Id => mId;

            public bool Directory
            {
                get
                {
                    return (Mode & MODE_DIR) != 0;
                }
            }
            
            public long Size
            {
                set
                {
                    if (!Directory)
                        lock (this)
                        {
                            mStream.SetLength(value);
                        }
                }
                get
                {
                    if (Directory)
                        return 0;

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
                mCreationTime = DateTime.UtcNow;
                mLastAccessTime = DateTime.UtcNow;
                mLastWriteTime = DateTime.UtcNow;
            }

            private DirectoryEnumerationContext mEnumCtx = null;
            private string mName = string.Empty;
            private MemoryStream mStream = null;
            private int mMode;
            private DateTime mCreationTime = DateTime.MinValue;
            private DateTime mLastAccessTime = DateTime.MinValue;
            private DateTime mLastWriteTime = DateTime.MinValue;
            private VirtualFile mParent = null;
            private long mSize = 0;
            private bool disposed = false;
            private int mId = 0;
        };
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