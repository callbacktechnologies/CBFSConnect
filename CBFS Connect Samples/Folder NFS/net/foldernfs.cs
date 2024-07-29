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
        static readonly NFS cbfs_nfs = new FolderNFS();

        internal static string baseFolder = "";

        static void Main(string[] args)
        {
            // Default NFS port
            int port = 2049;

            Banner();

            if (args.Length < 2)
            {
                Usage();
                return;
            }

            string sPort = args[0];
            if (sPort != "-")
                port = int.Parse(args[0]);

            baseFolder = args[1];
            if (baseFolder.EndsWith('\\') || baseFolder.EndsWith('/'))
                baseFolder = baseFolder[..^1];

            if (!Directory.Exists(baseFolder))
            {
                Console.WriteLine(string.Format("Base directory '{0}' specified in program arguments must exist", baseFolder));
                return; 
            }

            bool isWindows = RuntimeInformation.IsOSPlatform(OSPlatform.Windows);
            if (args.Length == 3 && isWindows)
            {
                Console.WriteLine("Mounting points are not supported on Windows, only on Linux and macOS");
                return;
            }

            if (args.Length >= 3)
            {
                cbfs_nfs.Config("MountingPoint=" + args[2]);
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
            Console.WriteLine("Usage: foldernfs [local port or - for default] <local folder> [<mounting point>]\n");
            Console.WriteLine("Example 1 (any OS): foldernfs 2049 c:\\temp\\VFSContents");
            Console.WriteLine("Example 2 (Linux/macOS): sudo foldernfs - /tmp/vfs_contents /mnt/mynfs\n");
            Console.WriteLine("'mount' command should be installed on Linux/macOS to use mounting points!");
            Console.WriteLine("Automatic mounting to mounting points is supported only on Linux and macOS.");
            Console.WriteLine("Also, mounting outside of your home directory may require admin rights.\n");
        }

        static void Banner()
        {
            Console.WriteLine("CBFS Connect Copyright (c) Callback Technologies, Inc.\n");
            Console.WriteLine("This demo shows how to create a virtual drive that represents a local folder using the NFS component.\n");
        }

        static void StopServer()
        {
            Console.WriteLine("Stopping server...");
            cbfs_nfs.StopListening();
            Console.WriteLine("Server stopped");
        }
    }


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
    }

    public class FileStreamContext
    {
        public FileStreamContext()
        {
            OpenCount = 0;
            hStream = null;
        }
        public void IncrementCounter()
        {
            Interlocked.Increment(ref OpenCount);            
        }
        public int DecrementCounter()
        {
            return Interlocked.Decrement(ref OpenCount);
        }
        public int OpenCount;
        public FileStream hStream;
    }
    
    class FolderNFS : NFS
    {
        public const int DIR_MODE = NFSConstants.S_IFDIR | NFSConstants.S_IRWXU | NFSConstants.S_IRGRP
                                | NFSConstants.S_IXGRP | NFSConstants.S_IROTH | NFSConstants.S_IXOTH; // Type -> Directory, Permissions -> 755

        public const int FILE_MODE = NFSConstants.S_IFREG | NFSConstants.S_IWUSR | NFSConstants.S_IRUSR
                                | NFSConstants.S_IRGRP | NFSConstants.S_IROTH; // Type -> File, Permission -> 644



        public FolderNFS() : base()
        {
            OnAccess += new OnAccessHandler(FireAccess);
            OnChmod += new OnChmodHandler(FireChmod);
            OnChown += new OnChownHandler(FireChown);
            OnError += new OnErrorHandler(FireError);
            OnLog += new OnLogHandler(FireLog);
            OnConnectionRequest += new OnConnectionRequestHandler(FireConnectionRequest);
            OnGetAttr += new OnGetAttrHandler(FireGetAttr);
            OnMkDir += new OnMkDirHandler(FireMkDir);
            OnConnected += new OnConnectedHandler(FireConnected);
            OnDisconnected += new OnDisconnectedHandler(FireDisconnected);
            OnLookup += new OnLookupHandler(FireLookup);
            OnOpen += new OnOpenHandler(FireOpen);
            OnRead += new OnReadHandler(FireRead);
            OnReadDir += new OnReadDirHandler(FireReadDir);
            OnClose += new OnCloseHandler(FireClose);
            OnRename += new OnRenameHandler(FireRename);
            OnRmDir += new OnRmDirHandler(FireRmDir);
            OnTruncate += new OnTruncateHandler(FireTruncate);
            OnUnlink += new OnUnlinkHandler(FireUnlink);
            OnUtimens += new OnUtimensHandler(FireUtimens);
            OnWrite += new OnWriteHandler(FireWrite);
        }

        static string GetRealPath(string path)
        {
            if (path.Equals("/"))
                return Program.baseFolder;
            else 
                return Program.baseFolder + path;
        }


        private void FireAccess(object sender, NFSAccessEventArgs e) { return; }
        private void FireChmod(object sender, NFSChmodEventArgs e) { return; }
        private void FireChown(object sender, NFSChownEventArgs e) { return; }

        private void FireClose(object sender, NFSCloseEventArgs e) 
        {
            if (e.FileContext != IntPtr.Zero)
            {
                FileStreamContext Ctx = (FileStreamContext)GCHandle.FromIntPtr(e.FileContext).Target;
                if (Ctx.DecrementCounter() == 0)
                {
                    Ctx.hStream.Close();
                    Ctx.hStream.Dispose();
                    Ctx.hStream = null; 
                    e.FileContext = IntPtr.Zero;
                }
                
                return;
            }
        }

        private void FireError(object sender, NFSErrorEventArgs e)
        {
            Console.WriteLine($"Error: {e.ErrorCode}");
            return;
        }

        private void FireLog(object sender, NFSLogEventArgs e)
        {
            Console.WriteLine($"{e.ConnectionId}: {e.Message}");
            return;
        }

        private void FireConnectionRequest(object sender, NFSConnectionRequestEventArgs e)
        {
            e.Accept = true;
            return;
        }

        private void FireGetAttr(object sender, NFSGetAttrEventArgs e)
        {
            Console.WriteLine($"FireGetAttr: {e.Path}");

            e.Result = Constants.NFS4ERR_NOENT;

            string realPath = GetRealPath(e.Path);

            FileSystemInfo fi;

            if (File.Exists(realPath))
                fi = new FileInfo(realPath);
            else
            if (Directory.Exists(realPath))
                fi = new DirectoryInfo(realPath);
            else
                return;
            
            e.Result = 0;
            e.LinkCount = 1;
            e.Group = "0";
            e.User = "0";

            if (fi.Attributes.HasFlag(FileAttributes.Directory))
            {
                e.Size = 4096; // 0
                e.Mode = DIR_MODE;
            }
            else
            {
                e.Size = ((FileInfo)fi).Length;
                e.Mode = FILE_MODE;
            }
            e.CTime = fi.CreationTimeUtc;
            e.MTime = fi.LastWriteTimeUtc;
            e.ATime = fi.LastAccessTimeUtc;
        }


        private void FireMkDir(object sender, NFSMkDirEventArgs e)
        {
            Console.WriteLine($"FireMkDir: {e.Path}");

            string realPath = GetRealPath(e.Path);

            try
            {
                Directory.CreateDirectory(realPath);
            }
            catch (Exception)
            {
                e.Result = Constants.NFS4ERR_IO;
            }
        }

        private void FireConnected(object sender, NFSConnectedEventArgs e)
        {
            Console.WriteLine($"Client connected: {e.StatusCode}: {e.Description}");
        }

        private void FireDisconnected(object sender, NFSDisconnectedEventArgs e)
        {
            Console.WriteLine($"Client disconnected: {e.StatusCode}: {e.Description}");
        }

        private void FireLookup(object sender, NFSLookupEventArgs e)
        {
            Console.WriteLine($"FireLookup: {e.Path}");

            string realPath = GetRealPath(e.Path);

            if (File.Exists(realPath) || Directory.Exists(realPath))
                return;

            e.Result = Constants.NFS4ERR_NOENT;
        }

        private void FireOpen(object sender, NFSOpenEventArgs e)
        {
            Console.WriteLine($"FireOpen: {e.Path}, open type: {e.OpenType}");

            FileStreamContext Ctx;

            string realPath = GetRealPath(e.Path);

            if (e.OpenType == Constants.OPEN4_CREATE)
            {
                if (File.Exists(realPath) || Directory.Exists(realPath))
                {
                    e.Result = Constants.NFS4ERR_EXIST;
                    return;
                }
            }
            else
            {
                if (!File.Exists(realPath))
                {
                    e.Result = Constants.NFS4ERR_NOENT;
                    return;
                }

                if (e.FileContext != IntPtr.Zero)
                {
                    Ctx = (FileStreamContext)GCHandle.FromIntPtr(e.FileContext).Target;
                    Ctx.IncrementCounter();
                    return;
                }
            }

            try
            {
                Ctx = new();

                Ctx.hStream = new FileStream(realPath, (e.OpenType == Constants.OPEN4_CREATE) ? FileMode.CreateNew : FileMode.Open, FileAccess.ReadWrite);

                e.FileContext = GCHandle.ToIntPtr(GCHandle.Alloc(Ctx));
                Ctx.IncrementCounter();
            }
            catch (Exception)
            {
                e.Result = Constants.NFS4ERR_IO;
            }
        }

        private void FireRead(object sender, NFSReadEventArgs e)
        {
            Console.WriteLine($"FireRead: {e.Path}");

            if (e.Count == 0) return;

            if (e.FileContext.Equals(IntPtr.Zero))
            {
                e.Result = Constants.NFS4ERR_IO;
                return;
            }

            try
            {

                FileStreamContext Ctx = (FileStreamContext)GCHandle.FromIntPtr(e.FileContext).Target;
                if (Ctx == null)
                {
                    e.Result = Constants.NFS4ERR_IO;
                    return;
                }
                FileStream fs = Ctx.hStream;

                if (fs == null || fs.SafeFileHandle.IsInvalid)
                {
                    e.Result = Constants.NFS4ERR_IO;
                    return;
                }

                int bytesRead;
                long streamLen = fs.Length;
                if (e.Offset >= streamLen)
                {
                    e.Count = 0;
                    e.Eof = true;
                    return;
                }
                fs.Position = e.Offset;
                bytesRead = fs.Read(e.BufferB, 0, (int)e.Count);
                if (e.Offset + bytesRead == streamLen)
                    e.Eof = true;
                e.Count = bytesRead;
            }
            catch (Exception ex)
            { 
                Console.WriteLine(ex.ToString());
                e.Result = Constants.NFS4ERR_IO;
            }
        }

        private void FireReadDir(object sender, NFSReadDirEventArgs e)
        {
            Console.WriteLine($"FireReadDir: {e.Path}");

            string realPath = GetRealPath(e.Path);

            try
            {

                int readOffset = 0;
                long cookie = NFSConstants.baseCookie;

                // If Cookie != 0, continue listing entries from a specified cookie. Otherwise, start listing entries from the start.
                if (e.Cookie != 0)
                {
                    readOffset = (int)e.Cookie - NFSConstants.baseCookie + 1;
                    cookie = e.Cookie + 1;
                }

                List<string> fsEntries = null;

                if (!e.FileContext.Equals(IntPtr.Zero))
                {
                    if (GCHandle.FromIntPtr(e.FileContext).Target is List<string> list)
                        fsEntries = list;
                }
                if (fsEntries == null) // new search, we are enumerating a directory here 
                {
                    try
                    {
                        IEnumerable<string> enumEntries = Directory.EnumerateFileSystemEntries(realPath);
                        fsEntries = new List<string>();
                        foreach (string entry in enumEntries)
                            fsEntries.Add(entry);
                        e.FileContext = GCHandle.ToIntPtr(GCHandle.Alloc(fsEntries));
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine(ex.ToString());
                        e.Result = Constants.NFS4ERR_NOENT;
                        return;
                    }
                }
                int fillRes;

                string path;
                bool listingComplete = false; 

                for (int i = readOffset; i < fsEntries.Count; i++)
                {
                    path = fsEntries[i];
                    FileSystemInfo fi;

                    if (File.Exists(path))
                        fi = new FileInfo(path);
                    else
                    if (Directory.Exists(path))
                        fi = new DirectoryInfo(path);
                    else
                        continue;

                    try
                    {
                        Console.WriteLine("Calling FillDir for " + fi.Name);

                        fillRes = FillDir(e.ConnectionId, fi.Name, 0, cookie,
                            (fi.Attributes.HasFlag(FileAttributes.Directory)) ? DIR_MODE : FILE_MODE,
                            "0",
                            "0",
                            1,
                            (fi.Attributes.HasFlag(FileAttributes.Directory)) ? 4096 : ((FileInfo)fi).Length,
                            fi.LastAccessTimeUtc,
                            fi.LastWriteTimeUtc,
                            fi.CreationTimeUtc);
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine(ex.ToString());
                        e.Result = Constants.NFS4ERR_IO;
                        return;
                    }
                    if (fillRes == 0)
                    {
                        if (i == fsEntries.Count - 1)
                        {
                            listingComplete = true;
                            break;
                        }
                        cookie++;
                    }
                    else
                    {
                        e.Result = Constants.NFS4_OK;
                        break;
                    }
                }

                if (listingComplete) // we have sent all the items, and we may remove the context
                {
                    e.FileContext = IntPtr.Zero;
                }
            }
            catch(Exception ex)
            {
                Console.WriteLine(ex.ToString());
                e.Result = Constants.NFS4ERR_IO;
                return;
            }
        }

        private void FireRename(object sender, NFSRenameEventArgs e)
        {
            Console.WriteLine($"FireRename: {e.OldPath} -> {e.NewPath}");

            if (e.OldPath.Equals(e.NewPath))
            {
                return;
            }

            string realOldPath = GetRealPath(e.OldPath);
            string realNewPath = GetRealPath(e.NewPath);

            if (File.Exists(realNewPath))
            {
                File.Delete(realNewPath);
            }
            else
            if (Directory.Exists(realNewPath))
            {
                e.Result = Constants.NFS4ERR_EXIST; 
                return;
            }

            try
            {
                File.Move(realOldPath, realNewPath);
            }
            catch (FileNotFoundException)
            {
                e.Result = Constants.NFS4ERR_NOENT;
                return;
            }
            catch (Exception)// this is a very simple catch-all handling. A real application should analyze the type of exception and return an appropriate error code.
            {
                e.Result = Constants.NFS4ERR_IO; 
                return; 
            }
        }

        private void FireRmDir(object sender, NFSRmDirEventArgs e)
        {
            Console.WriteLine($"FireRmDir: {e.Path}");

            string realPath = GetRealPath(e.Path);

            try
            { 
                Directory.Delete(realPath);
            }
            catch (FileNotFoundException)
            {
                e.Result = Constants.NFS4ERR_NOENT;
                return;
            }
            catch (Exception)// this is a very simple catch-all handling. A real application should analyze the type of exception and return an appropriate error code.
            {
                e.Result = Constants.NFS4ERR_IO;
                return;
            }
        }

        private void FireTruncate(object sender, NFSTruncateEventArgs e)
        {
            Console.WriteLine($"FireTruncate: {e.Path}");

            FileStreamContext Ctx = null; 
            FileStream fs = null;

            if (!e.FileContext.Equals(IntPtr.Zero))
            {
                Ctx = (FileStreamContext)GCHandle.FromIntPtr(e.FileContext).Target;
                if (Ctx != null)
                    fs = Ctx.hStream;
            }

            string realPath = GetRealPath(e.Path);

            if (fs == null)
            {
                try
                {
                    fs = new FileStream(realPath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.Read);
                    Ctx = null; // will indicate that we have created the filestream and have to close it
                }
                catch (FileNotFoundException)
                {
                    e.Result = Constants.NFS4ERR_NOENT;
                    return;
                }
                catch (Exception)// this is a very simple catch-all handling. A real application should analyze the type of exception and return an appropriate error code.
                {
                    e.Result = Constants.NFS4ERR_IO;
                    return;
                }
            }

            try
            {
                fs.SetLength(e.Size);
            }
            catch (Exception)// this is a very simple catch-all handling. A real application should analyze the type of exception and return an appropriate error code.
            {
                e.Result = Constants.NFS4ERR_IO;                
            }

            // If we opened the stream, now it is time to close it
            if (Ctx == null)
            {
                fs.Close();
                fs.Dispose();
            }
        }

        private void FireUnlink(object sender, NFSUnlinkEventArgs e)
        {
            Console.WriteLine($"FireUnlink: {e.Path}");

            string realPath = GetRealPath(e.Path);

            try
            { 
                File.Delete(realPath);
            }
            catch (FileNotFoundException)
            {
                e.Result = Constants.NFS4ERR_NOENT;
                return;
            }
            catch (Exception)// this is a very simple catch-all handling. A real application should analyze the type of exception and return an appropriate error code.
            {
                e.Result = Constants.NFS4ERR_IO;
                return;
            }

        }

        private void FireUtimens(object sender, NFSUtimensEventArgs e)
        {
            Console.WriteLine($"FireUtimens: {e.Path}");
            string realPath = GetRealPath(e.Path);
            try
            { 
                if (e.MTime.Ticks != 0)
                    File.SetLastWriteTimeUtc(realPath, e.MTime);
                if (e.ATime.Ticks != 0)
                    File.SetLastAccessTimeUtc(realPath, e.ATime);
            }
            catch (FileNotFoundException)
            {
                e.Result = Constants.NFS4ERR_NOENT;
                return;
            }
            catch (Exception)// this is a very simple catch-all handling. A real application should analyze the type of exception and return an appropriate error code.
            {
                e.Result = Constants.NFS4ERR_IO;
                return;
            }
        }

        private void FireWrite(object sender, NFSWriteEventArgs e)
        {
            Console.WriteLine($"FireWrite: {e.Path}");

            if (e.Count == 0) return;

            if (e.FileContext.Equals(IntPtr.Zero))
            {
                e.Result = Constants.NFS4ERR_IO;
                return;
            }

            FileStreamContext Ctx = (FileStreamContext)GCHandle.FromIntPtr(e.FileContext).Target;
            FileStream fs = Ctx.hStream;

            if (fs == null || fs.SafeFileHandle.IsInvalid)
            {
                e.Result = Constants.NFS4ERR_IO;
                return;
            }

            fs.Position = e.Offset;
            fs.Write(e.BufferB, 0, (int)e.Count);
            e.Stable = NFSConstants.FILE_SYNC4;
            
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