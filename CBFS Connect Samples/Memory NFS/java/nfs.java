/*
 * CBFS Connect 2024 Java Edition - Sample Project
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
 */

import java.io.*;
import java.util.*;

import cbfsconnect.*;

public class nfs implements NFSEventListener
{

    public static final int BASE_COOKIE = 0x12345678; // Base cookie

    public static final int S_IFMT = 61440;
    public static final int S_IFSOCK = 49152;
    public static final int S_IFLNK = 40960;
    public static final int S_IFREG = 32768;
    public static final int S_IFBLK = 24576;
    public static final int S_IFDIR = 16384;
    public static final int S_IFCHR = 8192;
    public static final int S_IFIFO = 4096;
    public static final int S_ISUID = 2048;
    public static final int S_ISGID = 1024;
    public static final int S_ISVTX = 512;
    public static final int S_IRWXU = 448;
    public static final int S_IRUSR = 256;
    public static final int S_IWUSR = 128;
    public static final int S_IXUSR = 64;
    public static final int S_IRWXG = 56;
    public static final int S_IRGRP = 32;
    public static final int S_IWGRP = 16;
    public static final int S_IXGRP = 8;
    public static final int S_IRWXO = 7;
    public static final int S_IROTH = 4;
    public static final int S_IWOTH = 2;
    public static final int S_IXOTH = 1;

    private static final int MODE_DIR = 0040000;

    public static final int FILE_SYNC4 = 2;

    public static final int DIR_MODE = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH; // Type -> Directory, Permissions -> 755
    public static final int FILE_MODE = S_IFREG | S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH; // Type -> File, Permission -> 644

    private static final long EMPTY_DATE;

    private final NFS cbfs_nfs;

    static
    {
        Calendar utc = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
        utc.set(1601, Calendar.JANUARY, 1, 0, 0, 0);
        utc.clear(Calendar.MILLISECOND);
        EMPTY_DATE = utc.getTimeInMillis();
    }

    public nfs()
    {
        cbfs_nfs = new NFS();
    }

    public void start(int port) throws TooManyListenersException, CBFSConnectException 
    {
        cbfs_nfs.addNFSEventListener(this);
        cbfs_nfs.setLocalPort(port);

        cbfs_nfs.startListening();
    }

    public void stop() throws CBFSConnectException
    {
        cbfs_nfs.stopListening();
        cbfs_nfs.removeNFSEventListener(this);
    }

    public void doEvents() throws CBFSConnectException
    {
        cbfs_nfs.doEvents();
    }

    private int createNew(String path, int mode)
    {
        String[] names = splitName(path);
        if (names == null)
            return Constants.NFS4ERR_NOENT;

        VirtualFile parent = Files.get(names[0]);
        if (parent == null || !parent.isDirectory())
            return Constants.NFS4ERR_NOENT;

        if (parent.exists(names[1]))
            return Constants.NFS4ERR_EXIST;

        VirtualFile file = new VirtualFile(names[1], mode);
        parent.add(names[1]);
        Files.add(path, file);


        return 0;
    }

    @Override
    public void access(NFSAccessEvent e)
    {
        System.out.println("access: " + e.path);
    }

    @Override
    public void chmod(NFSChmodEvent e)
    {
        System.out.println("chmod: " + e.path);
    }

    @Override
    public void chown(NFSChownEvent e)
    {
        System.out.println("chown: " + e.path);
    }

    @Override
    public void close(NFSCloseEvent e)
    {
        System.out.println("close: " + e.path);
    }

    @Override
    public void commit(NFSCommitEvent e)
    {
        System.out.println("commit: " + e.path);
    }

    @Override
    public void connectionRequest(NFSConnectionRequestEvent e)
    {
        e.accept = true;
    }

    @Override
    public void createLink(NFSCreateLinkEvent NFSCreateLinkEvent)
    {

    }

    @Override
    public void error(NFSErrorEvent e)
    {
        System.out.println("Error: " + e.errorCode + " / " + e.description);
    }

    @Override
    public void getAttr(NFSGetAttrEvent e)
    {
        VirtualFile file = Files.get(e.path);
        if (file == null)
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }

        e.fileId = file.getId();
        e.mode = file.getMode();
        e.CTime = file.getCreationTime();
        e.ATime = file.getLastAccessTime();
        e.MTime = file.getLastWriteTime();
        e.group = "0";
        e.user = "0";
        e.size = file.getSize();
        e.linkCount = 1;
    }

    @Override
    public void lock(NFSLockEvent e)
    {
        System.out.println("lock: " + e.path);
    }

    @Override
    public void log(NFSLogEvent e)
    {
        System.out.println("[" + e.connectionId + "]: " + e.message);
    }
    
    @Override
    public void lookup(NFSLookupEvent e)
    {
        System.out.println("lookup: " + e.path);

        VirtualFile file = Files.get(e.path);
        if (file == null)
            e.result = Constants.NFS4ERR_NOENT;
    }

    @Override
    public void mkDir(NFSMkDirEvent e)
    {
        System.out.println("mkDir: " + e.path);
        e.result = createNew(e.path, DIR_MODE);
    }

    @Override
    public void open(NFSOpenEvent e)
    {
        System.out.println("open: " + e.path + ", type: " + e.openType);

        if (e.openType == Constants.OPEN4_CREATE)
        {
            e.result = createNew(e.path, FILE_MODE);
        }
        else
        {
            VirtualFile file = Files.get(e.path);
            if (file == null)
                e.result = Constants.NFS4ERR_NOENT;
            else
                file.setLastAccessTime(new Date());

        }
    }

    @Override
    public void read(NFSReadEvent e)
    {
        if (e.count == 0) return;

        VirtualFile file = Files.get(e.path);
        if (file == null || file.isDirectory())
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }

        if (e.offset >= file.size)
        {
            e.count = 0;
            e.eof = true;
            return;
        }
        
        int count = file.read(e.offset, e.count, e.buffer);
        if (e.offset + count == file.size)
            e.eof = true;

        e.count = count;
    }

    @Override
    public void readDir(NFSReadDirEvent e)
    {
        System.out.println("readDir: " + e.path);

        VirtualFile dir = Files.get(e.path);
        if (dir == null)
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }

        int readOffset = 0;
        long cookie = BASE_COOKIE;

        // If Cookie != 0, continue listing entries from a specified cookie. Otherwise, start listing entries from the start.
        if (e.cookie != 0)
        {
            readOffset = (int) e.cookie - BASE_COOKIE + 1;
            cookie = e.cookie + 1;
        }

        List<String> content = dir.enumerate();
        if (content == null)
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }

        for (int i = readOffset; i < content.size(); i++)
        {
            String name = content.get(i);
            VirtualFile file = Files.get(combineNames(e.path, name));

            if (file == null)
            {
                e.result = Constants.NFS4ERR_NOENT;
                return;
            }

            try
            {
                if (cbfs_nfs.fillDir(e.connectionId,
                        name,
                        file.getId(),
                        cookie,
                        file.getMode(),
                        "0", "0", 1,
                        file.getSize(),
                        file.getLastAccessTime(),
                        file.getLastWriteTime(),
                        file.getCreationTime()) != 0)
                    break;

                cookie++;
            }
            catch (CBFSConnectException ex)
            {
                e.result = Constants.NFS4ERR_IO;
                return;
            }
        }
    }

    @Override
    public void readLink(NFSReadLinkEvent NFSReadLinkEvent) {

    }

    @Override
    public void rename(NFSRenameEvent e)
    {
        System.out.println("rename: " + e.oldPath + " -> " + e.newPath);

        VirtualFile file = Files.get(e.oldPath);
        if (file == null)
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }

        String[] oldNames = splitName(e.oldPath);
        if (oldNames == null)
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }

        VirtualFile oldParent = Files.get(oldNames[0]);
        if (oldParent == null)
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }

        String[] newNames = splitName(e.newPath);
        if (newNames == null)
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }

        VirtualFile newParent = Files.get(newNames[0]);
        if (newParent == null)
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }

        if (!newParent.getName().equals(oldParent.getName()) && newParent.exists(file.getName()))
        {
            newParent.remove(file.getName());
        }

        oldParent.remove(file.getName());
        file.setName(newNames[1]);
        Files.rename(e.oldPath, e.newPath);
        newParent.add(file.getName());
    }

    @Override
    public void rmDir(NFSRmDirEvent e)
    {
        System.out.println("rmDir: " + e.path);

        VirtualFile file = Files.get(e.path);
        if ((file == null) || (!file.isDirectory()))
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }
        Files.delete(e.path);
    }

    @Override
    public void truncate(NFSTruncateEvent e)
    {
        System.out.println("truncate: " + e.path);

        VirtualFile file = Files.get(e.path);
        if (file == null)
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }

        file.setSize((int) (e.size));
    }

    @Override
    public void unlink(NFSUnlinkEvent e)
    {
        System.out.println("unlink: " + e.path);

        VirtualFile file = Files.get(e.path);
        if (file == null || file.isDirectory())
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }

        Files.delete(e.path);
    }

    @Override
    public void unlock(NFSUnlockEvent e)
    {
        System.out.println("unlock: " + e.path);
    }

    @Override
    public void UTime(NFSUTimeEvent e)
    {
        System.out.println("utime: " + e.path);

        VirtualFile file = Files.get(e.path);
        if (file == null)
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }
        if (!isNullOrEmpty(e.ATime))
            file.setLastAccessTime(e.ATime);
        if (!isNullOrEmpty(e.MTime))
            file.setLastWriteTime(e.MTime);
    }

    @Override
    public void write(NFSWriteEvent e)
    {
        if (e.count == 0) return;

        VirtualFile file = Files.get(e.path);
        if (file == null || file.isDirectory())
        {
            e.result = Constants.NFS4ERR_NOENT;
            return;
        }
        
        e.count = file.write(e.buffer, e.offset, e.count);
        e.stable = FILE_SYNC4;
        file.setLastWriteTime(new Date());
    }

    @Override
    public void connected(NFSConnectedEvent e)
    {
        System.out.println("Client connected: " + e.statusCode + ": " + e.description);
    }

    @Override
    public void disconnected(NFSDisconnectedEvent e)
    {
        System.out.println("Client disconnected: " + e.statusCode + ": " + e.description);
    }
    
    // main

    static nfs nfs;

    public static void main(String[] args)
    {
        try
        {
            int port = 2049;
            String sPort;

            banner();

            if (args.length < 1)
            {
                usage();
                return;
            }

            sPort = args[0];
            if (!sPort.equals("-"))
                port = Integer.parseInt(args[0]);

            String osName = System.getProperty("os.name");
            if (args.length == 2 && osName.toLowerCase().contains("windows"))
            {
                System.out.println("Mounting points are not supported on Windows, only on Linux and macOS");
                return;
            }

            nfs = new nfs();

            nfs.createNew("/some", 0666);
            VirtualFile aFile = Files.get("/some");
            aFile.setSize(193998846);

            if (args.length == 2)
            {
                nfs.cbfs_nfs.config("MountingPoint=" + args[1]);
            }
            
            nfs.start(port);
            try
            {
                System.out.println("NFS server started on port " + port);
                System.out.println("Press <Enter> to stop the server");

                int key;
                while (true)
                {
                    nfs.doEvents();
                    if (System.in.available() > 0)
                    {
                        key = System.in.read();
                        if (key == 10)
                            break;
                        else
                            System.out.println("The code of the pressed key was " +key);
                    }
                }
            } finally
            {
                stopServer();
            }
        } catch (CBFSConnectException ex)
        {
            System.out.println("CBFSConnectException: " + ex.getMessage() + " Code: " + ex.getCode());
        } catch (Exception ex)
        {
            System.out.println("Exception: " + ex.getMessage());
        }
    }

    private static void banner()
    {
        System.out.println("CBFS NFS Copyright (c) Callback Technologies, Inc.\n");
        System.out.println("This demo shows how to create a virtual drive that is stored in memory using the NFS component.\n");
    }

    private static void usage()
    {
        System.out.println("Usage: nfs [local port or - for default]\n");
        System.out.println("Example: nfs 2049");
    }

    private static void stopServer() throws CBFSConnectException
    {
        System.out.println("Stopping server...");
        nfs.stop();
        System.out.println("Server stopped");
    }

    private static boolean isNullOrEmpty(String s)
    {
        return (s == null) || (s.length() == 0);
    }

    private static boolean isNullOrEmpty(Date d)
    {
        return (d == null) || (d.getTime() == EMPTY_DATE);
    }

    private static String[] splitName(String value)
    {
        if (isNullOrEmpty(value) || value.equals("/"))
            return null;
        int index = value.lastIndexOf('/');
        if (index < 0)
            return null;

        String[] parts = new String[2];
        if (index == 0)
            parts[0] = "/";
        else
            parts[0] = value.substring(0, index);
        parts[1] = value.substring(index + 1);
        return parts;
    }

    private static String combineNames(String parent, String name)
    {
        if (isNullOrEmpty(parent) || isNullOrEmpty(name))
            return null;
        if (parent.endsWith("/"))
            return parent + name;
        return parent + "/" + name;
    }

    public static String getFileName(String fullPath)
    {
        assert fullPath != null;

        int result = fullPath.length();

        while (--result >= 0)
        {
            if (fullPath.charAt(result) == '/')
                break;
        }
        return fullPath.substring(result + 1);
    }

    private static class VirtualFile
    {
        private long id;
        private int mode;
        private String name;
        private Date creationTime;
        private Date lastAccessTime;
        private Date lastWriteTime;

        private final List<String> entries;
        private int size;

        private MemoryStream data = new MemoryStream();

        VirtualFile(String name, int mode)
        {
            this.name = name;
            this.mode = mode;

            creationTime = new Date();
            lastAccessTime = new Date();
            lastWriteTime = new Date();

            if (isDirectory())
                entries = new ArrayList<String>();
            else
                entries = null;
            
            size = 0;
        }

        boolean add(String name)
        {
            if (entries == null || isNullOrEmpty(name))
                return false;
            synchronized (entries)
            {
                String lowered = name;
                if (entries.contains(lowered))
                    return false;
                entries.add(lowered);
                return true;
            }
        }

        void clear()
        {
            name = null;
            if (entries != null)
            {
                synchronized (entries)
                {
                    entries.clear();
                }
            }
            if (data != null)
            {
                data.clear();
            }
            size = 0;
        }

        List<String> enumerate()
        {
            if (!isDirectory())
                return null;
            else
                return entries;
        }

        boolean exists(String name)
        {
            if (entries == null || isNullOrEmpty(name))
                return false;

            synchronized (entries)
            {
                return entries.contains(name);
            }
        }


        long getId() {
            return id;
        }

        int getMode()
        {
            return mode;
        }

        Date getCreationTime()
        {
            return creationTime;
        }

        Date getLastAccessTime()
        {
            return lastAccessTime;
        }

        Date getLastWriteTime()
        {
            return lastWriteTime;
        }

        String getName()
        {
            return name;
        }

        int getSize()
        {
            return (isDirectory() || data == null) ? 0 : data.length;
        }

        boolean isDirectory()
        {
            return (mode & MODE_DIR) != 0;
        }

        boolean isEmpty()
        {
            if (entries != null)
                synchronized (entries)
                {
                    return (entries.size() == 0);
                }

            return (size == 0);
        }

        int read(long position, int bytesToRead, byte[] buffer)
        {
            data.seek((int)position);
            return data.read(buffer, 0, bytesToRead);
        }

        void remove(String name)
        {
            if (entries == null || isNullOrEmpty(name))
                return;
            synchronized (entries)
            {
                String lowered = name;
                if (!entries.contains(lowered))
                    return;
                entries.remove(lowered);
            }
        }

        void setMode(int newMode)
        {
            this.mode = newMode;
        }
        
        void setId(long newId) {
            this.id = newId;
        }

        void setCreationTime(Date newTime)
        {
            if (newTime != null)
                this.creationTime = (Date) newTime.clone();
        }

        void setLastAccessTime(Date newTime)
        {
            if (newTime != null)
                this.lastAccessTime = (Date) newTime.clone();
        }

        void setLastWriteTime(Date newTime)
        {
            if (newTime != null)
                this.lastWriteTime = (Date) newTime.clone();
        }

        void setSize(int newSize)
        {
            if (isDirectory() || newSize == getSize())
                return;

            data.ensureCapacity(newSize);
        }

        void setName(String newName)
        {
            this.name = newName;
        }

        int write(byte[] buffer, long position, int bytesToWrite)
        {
            data.seek((int)position);
            data.write(buffer, 0, bytesToWrite);
            return bytesToWrite;
        }
    }

    private static class MemoryStream {
        private byte[] buffer;
        private int position;
        private int capacity;
        private int length;

        public MemoryStream() {
            clear();
        }

        public void clear() {
            this.capacity = 32;
            this.buffer = new byte[this.capacity];
            this.position = 0;
            this.length = 0;
        }

        public void write(byte[] data, int offset, int size) {
            ensureCapacity(position + data.length);
            System.arraycopy(data, offset, buffer, position, size);
            position += size;
            if (position > length) {
                length = position;
            }
        }

        public int read(byte[] data, int offset, int count) {
            if (position >= length) {
                return -1;
            }
            int bytesToRead = Math.min(count, length - position);
            System.arraycopy(buffer, position, data, offset, bytesToRead);
            position += bytesToRead;
            return bytesToRead;
        }

        public void seek(int offset) {
            if (offset < 0 || offset > length) {
                throw new IllegalArgumentException("Offset is out of bounds");
            }
            position = offset;
        }
        
        private void ensureCapacity(int minCapacity) {
            if (minCapacity > capacity) {
                int newCapacity = Math.max(minCapacity, capacity * 2);
                buffer = Arrays.copyOf(buffer, newCapacity);
                capacity = newCapacity;
            }
        }
    }

    private static class Files
    {
        private static final Map<String, VirtualFile> entries;

        private static long counter = 0;

        static
        {
            entries = new HashMap<String, VirtualFile>();
            entries.put("/", new VirtualFile("", MODE_DIR));
        }

        static void add(String name, VirtualFile file)
        {
            if (isNullOrEmpty(name) || file == null)
                return;
            synchronized (entries)
            {
                entries.put(name, file);
                counter++;
                file.setId(counter);
            }
        }

        static long calcSize()
        {
            synchronized (entries)
            {
                long size = 0;
                for (String name : entries.keySet())
                {
                    size += entries.get(name).getSize();
                }
                return size;
            }
        }

        static void clear()
        {
            synchronized (entries)
            {
                for (String name : entries.keySet())
                {
                    VirtualFile file = entries.get(name);
                    if (file != null)
                        file.clear();
                }
                entries.clear();
            }
        }

        static void delete(String name)
        {
            if (isNullOrEmpty(name))
                return;

            synchronized (entries)
            {
                String[] names = splitName(name);
                if (names == null)
                    return;

                VirtualFile parent = entries.get(names[0]);
                if (parent != null)
                    parent.remove(names[1]);

                VirtualFile file = entries.remove(name);
                if (file != null)
                    file.clear();
            }
        }

        static VirtualFile get(String name)
        {
            if (isNullOrEmpty(name))
                return null;
            synchronized (entries)
            {
                return entries.get(name);
            }
        }

        static void rename(String name, String newName)
        {
            if (entries == null)
                return;

            synchronized (entries)
            {
                VirtualFile file = entries.remove(name);
                if (file == null)
                    return;

                entries.put(newName, file);

                if (file.isDirectory() && !file.isEmpty())
                    renameChildItems(name, newName);
            }
        }

        static private void renameChildItems(String name, String newName)
        {
            List<String> pathsToRename = new ArrayList();
            int pathOffset = name.length();

            for (String path : entries.keySet())
            {
                if (path.startsWith(name + "/"))
                    pathsToRename.add(path);
            }

            for (String path : pathsToRename)
            {
                VirtualFile file = entries.remove(path);
                if (file == null)
                    continue;

                String newPath = newName + path.substring(pathOffset);
                entries.put(newPath, file);
            }
        }
    }
}




