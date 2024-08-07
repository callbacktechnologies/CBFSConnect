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
import java.io.File;
import java.nio.ByteBuffer;
import java.util.*;
import java.nio.file.Paths;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.regex.Pattern;

import cbfsconnect.*;

class FUSEDriveCache extends CBCache implements CBCacheEventListener
{
    private cachefusememdrive owner;

    public FUSEDriveCache(cachefusememdrive owner)
    {
        this.owner = owner;
    }

    @Override
    public void beforeFlush(CBCacheBeforeFlushEvent cbcacheBeforeFlushEvent)
    {

    }

    @Override
    public void beforePurge(CBCacheBeforePurgeEvent cbcacheBeforePurgeEvent)
    {

    }

    @Override
    public void endCleanup(CBCacheEndCleanupEvent cbcacheEndCleanupEvent)
    {

    }

    @Override
    public void error(CBCacheErrorEvent cbcacheErrorEvent)
    {

    }

    @Override
    public void log(CBCacheLogEvent cbcacheLogEvent)
    {

    }

    @Override
    public void progress(CBCacheProgressEvent cbcacheProgressEvent)
    {

    }

    @Override
    public void readData(CBCacheReadDataEvent e)
    {
        if ((e.flags & Constants.RWEVENT_CANCELED) == Constants.RWEVENT_CANCELED)
            return;
        if (e.bytesToRead == 0)
            return;

        cachefusememdrive.VirtualFile vfile = (cachefusememdrive.VirtualFile) globals.get(e.fileContext);
        if (vfile == null) {
            e.resultCode = Constants.RWRESULT_FILE_FAILURE;
            return;
        }

        if (vfile.isDirectory()) {
            e.resultCode = Constants.RWRESULT_FILE_FAILURE;
            return;
        }

        if (e.position < 0 || e.position >= vfile.getSize())
        {
            e.resultCode = Constants.RWRESULT_RANGE_BEYOND_EOF;
            return;
        }

        e.bytesRead = vfile.read(e.position, e.bytesToRead, e.buffer);

        if (e.bytesRead == 0)
            e.resultCode = Constants.RWRESULT_FILE_FAILURE;
        else
        if (e.bytesRead < e.bytesToRead)
            e.resultCode = Constants.RWRESULT_PARTIAL;
        else
            e.resultCode = Constants.RWRESULT_SUCCESS;
    }

    @Override
    public void startCleanup(CBCacheStartCleanupEvent cbcacheStartCleanupEvent)
    {

    }

    @Override
    public void status(CBCacheStatusEvent cbcacheStatusEvent)
    {

    }

    @Override
    public void writeData(CBCacheWriteDataEvent e)
    {
        if ((e.flags & Constants.RWEVENT_CANCELED) == Constants.RWEVENT_CANCELED)
            return;
        if (e.bytesToWrite == 0)
            return;

        cachefusememdrive.VirtualFile vfile = (cachefusememdrive.VirtualFile) globals.get(e.fileContext);
        if (vfile == null) {
            e.resultCode = Constants.RWRESULT_FILE_FAILURE;
            return;
        }

        if (vfile.isDirectory()) {
            e.resultCode = Constants.RWRESULT_FILE_FAILURE;
            return;
        }

        if (e.position < 0)
        {
            e.resultCode = Constants.RWRESULT_RANGE_BEYOND_EOF;
            return;
        }

        e.bytesWritten = vfile.write(e.buffer, e.position, e.bytesToWrite);
        if (e.bytesWritten == 0)
            e.resultCode = Constants.RWRESULT_FILE_FAILURE;
        else
        if (e.bytesWritten < e.bytesToWrite)
            e.resultCode = Constants.RWRESULT_PARTIAL;
        else
            e.resultCode = Constants.RWRESULT_SUCCESS;
    }
}


/**
 * The storage for global objects (such as contexts) which are returned to a driver
 * and must not be processed by GC until explicitly released.
 */
class globals {
    private static long counter = 0;
    private static final Map<Long, Item> storage = new HashMap<Long, Item>();

    /**
     * Increases the reference counter for a specified global object by its ID.
     * @param id    ID of a stored object to increase the reference counter
     * @return A stored object that corresponds to the specified ID,
     * or {@code null} if no object with the specified ID exists.
     * @see globals#release(long)
     */
    static Object acquire(long id) {
        synchronized (storage) {
            Item item = storage.get(id);
            if (item == null)
                return null;
            item.refs++;
            return item.target;
        }
    }

    /**
     * Adds a new object and initializes its reference counter to 1.
     * @param target An object to add.
     * @return An ID of the stored object.
     * @see globals#free(long)
     */
    static long alloc(Object target) {
        synchronized (storage) {
            long id = ++counter;
            Item item = new Item(target);
            storage.put(id, item);
            return id;
        }
    }

    /**
     * Removes all the global items from the storage.
     */
    static void clear() {
        synchronized (storage) {
            for (long id: storage.keySet()) {
                Item item = storage.get(id);
                item.clear();
            }
            storage.clear();
        }
    }

    /**
     * Removes a stored object by its ID despite the value of its reference counter.
     * @param id ID of an object to remove.
     * @return The removed object, or {@code null} if the specified ID not found.
     * @see globals#alloc(Object)
     */
    static Object free(long id) {
        synchronized (storage) {
            Item item = storage.remove(id);
            if (item == null)
                return null;
            Object target = item.target;
            item.clear();
            return target;
        }
    }

    /**
     * Gets a stored object by its ID. The object's reference counter is not updated.
     * @param id ID of an object to retrieve.
     * @return A stored object that corresponds to the specified ID,
     * or {@code null} if no object with the specified ID exists in the storage.
     * @see globals#set(long, Object)
     */
    static Object get(long id) {
        synchronized (storage) {
            Item item = storage.get(id);
            return (item == null) ? null : item.target;
        }
    }

    /**
     * Decreases the reference counter of an object by its ID in the storage, and removes the object
     * from the storage if its reference counter become 0.
     * @param id An object ID to release.
     * @return The removed object if its reference counter became 0, or {@code null} if either no object
     * with the specified ID exists in the storage or the reference counter for the object is still greater than 0.
     * @see globals#acquire(long)
     */
    static Object release(long id) {
        synchronized (storage) {
            Item item = storage.get(id);
            if (item == null)
                return null;
            if (--item.refs > 0)
                return null;
            storage.remove(id);
            Object target = item.target;
            item.clear();
            return target;
        }
    }

    /**
     * Stores a new object by existing ID. The object's reference counter is not updated.
     * @param id     ID of an object to replace.
     * @param target A new object to store.
     * @return A previously stored object, or {@code null} if either the specified ID not found or
     * the {@code target} is already stored with the specified ID.
     */
    static Object set(long id, Object target) {
        synchronized (storage) {
            Item item = storage.get(id);
            if (item == null)
                return null;
            if (item.target == target)
                return null;
            Object old = item.target;
            item.target = target;
            return old;
        }
    }

    private static class Item {
        int refs;
        Object target;

        Item(Object target) {
            super();
            this.refs = 1;
            this.target = target;
        }

        void clear() {
            refs = 0;
            target = null;
        }
    }
}

public class cachefusememdrive implements FUSEEventListener {

    private static final String PRODUCT_GUID = "{713CC6CE-B3E2-4fd9-838D-E28F558F6866}";
    private static final int SECTOR_SIZE = 512;
    private static final long DRIVE_SIZE = 64 * 1024 * 1024;    // 64MB
    static final int BUFFER_SIZE = 1024 * 1024; // 1MB

    private static final int FALLOC_FL_KEEP_SIZE = 1;


    private static final int GAP = 6;
    private static final int DGAP = GAP * 2;

    // Error codes
    private static final int ESRCH           = 3;
    private static final int EPERM           = 1;
    private static final int ENOENT          = 2;
    private static final int EINTR           = 4;
    private static final int EIO             = 5;
    private static final int ENXIO           = 6;
    private static final int E2BIG           = 7;
    private static final int ENOEXEC         = 8;
    private static final int EBADF           = 9;
    private static final int ECHILD          = 10;
    private static final int EAGAIN          = 11;
    private static final int ENOMEM          = 12;
    private static final int EACCES          = 13;
    private static final int EFAULT          = 14;
    private static final int EBUSY           = 16;
    private static final int EEXIST          = 17;
    private static final int EXDEV           = 18;
    private static final int ENODEV          = 19;
    private static final int ENOTDIR         = 20;
    private static final int EISDIR          = 21;
    private static final int ENFILE          = 23;
    private static final int EMFILE          = 24;
    private static final int ENOTTY          = 25;
    private static final int EFBIG           = 27;
    private static final int ENOSPC          = 28;
    private static final int ESPIPE          = 29;
    private static final int EROFS           = 30;
    private static final int EMLINK          = 31;
    private static final int EPIPE           = 32;
    private static final int EDOM            = 33;
    private static final int EDEADLK         = 36;
    private static final int ENAMETOOLONG    = 38;
    private static final int ENOLCK          = 39;
    private static final int ENOSYS          = 40;
    private static final int ENOTEMPTY       = 41;
    private static final int EOPNOTSUPP      = 95;

    private static final int ERROR_PRIVILEGE_NOT_HELD = 1314;

    private static final int MODE_DIR = 0040000;

    private static final long EMPTY_DATE;

    private static FUSE fuse;
    private static FUSEDriveCache cache;

    private static final Pattern DRIVE_LETTER_PATTERN = Pattern.compile("^[A-Za-z]:$");

    private String cabFileLocation;
    private boolean initialized;
    private String mountingPoint;

    static {
        Calendar utc = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
        utc.set(1601, Calendar.JANUARY, 1, 0, 0, 0);
        utc.clear(Calendar.MILLISECOND);
        EMPTY_DATE = utc.getTimeInMillis();
    }

    private void install() {
        boolean rebootNeeded;

        try {
            rebootNeeded = fuse.install(cabFileLocation, PRODUCT_GUID, null, Constants.INSTALL_REMOVE_OLD_VERSIONS) != 0;
            updateDriverStatus();
        } catch (CBFSConnectException err) {
            if (err.getCode() == ERROR_PRIVILEGE_NOT_HELD)
                System.out.println("Installation requires administrator rights. Please run the app as Administrator");
            else
                System.out.println("Installation failed: " + err.getMessage());

            return;
        }

        if (rebootNeeded)
            System.out.println("System restart is needed for the changes to take effect");
        else
            System.out.println("Driver installed successfully");
    }

    private void uninstall() {
        boolean rebootNeeded;

        try {
            rebootNeeded = fuse.uninstall(cabFileLocation, PRODUCT_GUID, null, Constants.UNINSTALL_VERSION_CURRENT) != 0;
            updateDriverStatus();
        } catch (CBFSConnectException err) {
            if (err.getCode() == ERROR_PRIVILEGE_NOT_HELD)
                System.out.println("Uninstallation requires administrator rights. Please run the app as Administrator");
            else
                System.out.println("Uninstallation failed: "  + err.getMessage());

            return;
        }

        if (rebootNeeded)
            System.out.println("System restart is needed for the changes to take effect");
        else
            System.out.println("Driver uninstalled successfully");
    }

    private void mount() {
        try {
            if (!initialized) {
                fuse.initialize(PRODUCT_GUID);
                fuse.addFUSEEventListener(this);
                initialized = true;
            }

            cache.openCache("", false);

            fuse.mount(mountingPoint);

            System.out.println("Mounted successfully!");
        } catch (Exception err) {
            System.out.println("Failed to mount: " + err.getMessage());
        }
    }

    private void unmount() {
        try {
            fuse.unmount();
            System.out.println("Unmounted successfully!");

            if (cache.isActive())
                cache.closeCache(Constants.FLUSH_IMMEDIATE, Constants.PURGE_NONE);

        } catch (CBFSConnectException err) {
            System.out.println("Failed to unmount media: " + err.getMessage());
        }
    }

    private void updateDriverStatus() {
        try {
            DriverStatus status = DriverStatus.fromInt(fuse.getDriverStatus(PRODUCT_GUID));

            System.out.println("Driver status:");

            if (status == null) {
                System.out.println("Version: ERROR");
                System.out.println("Status: Failed to get driver status");
            } else if (status == DriverStatus.NOT_INSTALLED) {
                System.out.println("Version: not installed");
                System.out.println("Service: not installed");
            } else {
                long version = fuse.getDriverVersion(PRODUCT_GUID);
                System.out.println(String.format("Version: %d.%d.%d.%d", version >> 48, (version >> 32) & 0xffff, (version >> 16) & 0xffff, version & 0xffff));
                System.out.println(String.format("Service: %s", status));
            }
        } catch (CBFSConnectException err) {
            System.out.println("Version: ERROR");
            System.out.println(err.getMessage());
        }
    }

    private static boolean isNullOrEmpty(String s) {
        return (s == null) || (s.length() == 0);
    }

    private static boolean isNullOrEmpty(Date d) {
        return (d == null) || (d.getTime() == EMPTY_DATE);
    }

    private static String[] splitName(String value) {
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

    private static String combineNames(String parent, String name) {
        if (isNullOrEmpty(parent) || isNullOrEmpty(name))
            return null;
        if (parent.endsWith("/"))
            return parent + name;
        return parent + "/" + name;
    }

    private void processParams(String[] args) {
        boolean doInstall = "-drv-inst".equals(args[0]);
        boolean doUninstall = "-drv-uninst".equals(args[0]);

        if (doInstall || doUninstall) {
            if (args.length < 2) {
                usage();
                return;
            }

            cabFileLocation = ConvertRelativePathToAbsolute(args[1]);
            if (isNullOrEmpty(cabFileLocation)) {
                System.out.println("Error: Invalid Cab File Path");
                System.exit(1);
            }

            if (doInstall) {
                install();
            } else {
                uninstall();
            }

            if (doUninstall || args.length == 2)
                return;
        }

        mountingPoint = args[args.length - 1];
        if (mountingPoint.length() == 1){
            mountingPoint += ":";
        }
        mountingPoint = ConvertRelativePathToAbsolute(mountingPoint, true);
        if (isNullOrEmpty(mountingPoint)) {
            System.out.println("Error: Invalid Mountig Point Path");
            System.exit(1);
        }

        mount();
        waitForQuit();
    }

    private boolean isDriveLetter(String path) {
        if (path == null || path.isEmpty() || path.length() != 2)
            return false;

        char c = path.charAt(0);
        if ((((int)c >= (int) 'A' && (int)c <= (int) 'Z') ||
                ((int)c >= (int) 'a' && (int)c <= (int) 'z')) &&
                (path.charAt(1) == ':'))
            return true;
        else
            return false;
    }

    private String ConvertRelativePathToAbsolute(String path) {
        return ConvertRelativePathToAbsolute(path, false);
    }

    private String ConvertRelativePathToAbsolute(String path, boolean acceptMountingPoint) {
        String res = null;
        if (path != null && !path.isEmpty()) {
            res = path;

            // Linux-specific case of using a home directory
            if (path.equals("~") || path.startsWith("~/")) {
                String homeDir = System.getProperty("user.home");
                if (path.equals("~") || path.equals("~/"))
                    return homeDir;
                else
                    return Paths.get(homeDir, path.substring(2)).toString();
            }

            int semicolonCount = path.split(";").length - 1;
            boolean isNetworkMountingPoint = semicolonCount == 2;
            String remainedPath = "";

            if (isNetworkMountingPoint) {
                if (!acceptMountingPoint) {
                    System.out.println("The path '" + path + "' format cannot be equal to the Network Mounting Point");
                    return "";
                }
                int pos = path.indexOf(';');
                if (pos != path.length() - 1) {
                    res = path.substring(0, pos);
                    if (res.isEmpty()) {
                        return path;
                    }
                    remainedPath = path.substring(pos);
                }
            }

            if (isDriveLetter(res)) {
                if (!acceptMountingPoint) {
                    System.out.println("The path '" + res + "' format cannot be equal to the Drive Letter");
                    return "";
                }
                return path;
            }

            if (!Paths.get(res).isAbsolute()) {
                res = Paths.get(res).toAbsolutePath().toString();
                return res + remainedPath;
            }
        }
        return path;
    }

    private void waitForQuit() {
        System.out.println("Enter \"quit\" to exit:");

        Scanner scanner = new Scanner(System.in);
        try {
            do {
                String command = scanner.nextLine();
                if ("quit".equalsIgnoreCase(command)) {
                    unmount();
                    System.exit(0);
                }
            } while (true);
        } finally {
            scanner.close();
        }
    }

    static void usage() {
        if (System.getProperty("os.name").toLowerCase().startsWith("windows"))
        {
            System.out.println("Usage: fusememdrive [-<switch 1> ... -<switch N>] <mounting point>");
            System.out.println("<Switches>");
            System.out.println("  -drv-inst {cab_file} - Install drivers from CAB file");
            System.out.println("  -drv-uninst {cab_file} - Uninstall drivers from CAB file");
            System.out.println("");
            System.out.println("A mounting point can be a drive letter (e.g., Z:) or a path to an empty directory on the NTFS drive");
        }
        else
        {
            System.out.println("Usage: fusememdrive <mounting point>");
            System.out.println("");
            System.out.println("A mounting point should be the absolute path to an empty directory");
        }

    }

    public cachefusememdrive()
    {
        if (System.getProperty("os.name").toLowerCase().startsWith("windows"))
            mountingPoint = "Z:";
        else
            mountingPoint = "~/";

        try {
            updateDriverStatus();
        } catch (Exception err) {
            System.out.println("Error: " + err.getMessage());
        }

        try {
            cache = new FUSEDriveCache(this);

            cache.addCBCacheEventListener(cache); // this sets an event handler. We could have an implementation of the CBCacheEventListener interface in the main class, but a dedicated class provides cleaner code.

            String CacheDirectory = "./cache";
            File dir = new File(CacheDirectory);
            if (dir.exists() && !dir.isDirectory())
                throw new Exception("Cache directory exists and is a file");
            if (!dir.exists())
                dir.mkdir();
            cache.setCacheDirectory(CacheDirectory);

            cache.setCacheFile("cache.svlt");
            cache.setReadingCapabilities(Constants.RWCAP_POS_RANDOM | Constants.RWCAP_SIZE_ANY);
            cache.setResizingCapabilities(Constants.RSZCAP_GROW_TO_ANY | Constants.RSZCAP_SHRINK_TO_ANY | Constants.RSZCAP_TRUNCATE_AT_ZERO);
            cache.setWritingCapabilities(Constants.RWCAP_POS_RANDOM | Constants.RWCAP_SIZE_ANY | Constants.RWCAP_WRITE_KEEPS_FILESIZE);

        }
        catch (TooManyListenersException e)
        {
            throw new RuntimeException(e);
        } catch (CBFSConnectException e)
        {
            throw new RuntimeException(e);
        } catch (Exception e)
        {
            throw new RuntimeException(e);
        }
    }

    public static void main(String[] args) {
        try
        {
            fuse = new FUSE();
        }
        catch (UnsatisfiedLinkError ex)
        {
            System.out.println("The JNI library could not be found or loaded.");
            System.out.println("Please refer to the Deployment\\User Mode Library topic for the instructions related to placing the JNI library in the system.");
            System.out.println("Exception details follow:");
            System.out.println(ex.getMessage());
            return;
        }

        if (args.length < 1 || args.length > 3) {
            usage();
            return;
        }

        cachefusememdrive drive = new cachefusememdrive();
        drive.processParams(args);
    }

    // -----------------------------------
    // Implementation of FUSEEventListener

    public void access(FUSEAccessEvent e) {
        if (isNullOrEmpty(e.path)) {
            e.result = -ENOENT;
            return;
        }

        VirtualFile file = Files.get(e.path);
        if (file == null) {
            e.result = -ENOENT;
            return;
        }
    }

    public void chmod(FUSEChmodEvent e) {
        if (isNullOrEmpty(e.path)) {
            e.result = -ENOENT;
            return;
        }

        VirtualFile file = Files.get(e.path);
        if (file == null) {
            e.result = -ENOENT;
            return;
        }

        file.setMode(e.mode);
    }

    public void chown(FUSEChownEvent e) {
        if (isNullOrEmpty(e.path)) {
            e.result = -ENOENT;
            return;
        }

        VirtualFile file = Files.get(e.path);
        if (file == null) {
            e.result = -ENOENT;
            return;
        }

        file.setGid(e.gid);
        file.setUid(e.uid);
    }

    public void copyFileRange(FUSECopyFileRangeEvent e) {}

    public void create(FUSECreateEvent e) {
        String[] names = splitName(e.path);
        if (names == null) {
            e.result = -ENOENT;
            return;
        }

        VirtualFile parent = Files.get(names[0]);
        if (parent == null) {
            e.result = -ENOENT;
            return;
        }
        if (!parent.isDirectory()) {
            e.result = -ENOTDIR;
            return;
        }
        if (parent.exists(names[1])) {
            e.result = -EEXIST;
            return;
        }

        try {
            VirtualFile file = new VirtualFile(names[1], e.mode, fuse.getGid(), fuse.getUid());
            parent.add(names[1]);
            Files.add(e.path, file);
            file.setId(globals.alloc(file));
            e.fileContext = file.getId();
            file.open();
        }
        catch (CBFSConnectException err)
        {
            e.result = err.getCode();
        }
        if (e.result == 0)
        {
            try
            {
                cache.fileOpenEx(e.path, 0, false, 0, 0, Constants.PREFETCH_NOTHING, e.fileContext);
            }
            catch (CBFSConnectException ex)
            {
                e.result = -EIO;
            }
        }
    }

    public void destroy(FUSEDestroyEvent e) {}

    public void error(FUSEErrorEvent e) {
        System.out.println(String.format("Error: %d, Description: %s", e.errorCode, e.description));
    }

    public void FAllocate(FUSEFAllocateEvent e)
    {
        VirtualFile file = (VirtualFile) globals.get(e.fileContext);
        if (file == null)
        {
            file = Files.get(e.path);
            if (file == null)
            {
                e.result = -ENOENT;
                return;
            }
        }

        int flags = e.mode & (~FALLOC_FL_KEEP_SIZE);
        if (flags != 0)
        {
            // if we detect unsupported flags in Mode, we deny the request
            e.result = -EOPNOTSUPP;
            return;
        }

        long newSize = e.offset + e.length;
        if (newSize > file.size)
        {
            file.setAllocationSize((int) (e.offset + e.length));
            // fallocate may be used on non-Windows systems to expand file size
            // Windows component always sets the FALLOC_FL_KEEP_SIZE flag
            if ((e.mode & FALLOC_FL_KEEP_SIZE) != FALLOC_FL_KEEP_SIZE)
            {
                if (file.size < newSize)
                    file.setSize((int) newSize);
                try
                {
                    if (cache.fileExists(e.path))
                        cache.fileSetSize(e.path, newSize, false);
                } catch (CBFSConnectException ex)
                {
                    // suppress an exception
                }
            }
        }
    }

    public void flush(FUSEFlushEvent e) {}

    public void FSync(FUSEFSyncEvent e) {}

    public void getAttr(FUSEGetAttrEvent e) {
        if (isNullOrEmpty(e.path)) {
            e.result = -ENOENT;
            return;
        }

        VirtualFile file = Files.get(e.path);
        if (file == null) {
            e.result = -ENOENT;
            return;
        }

        e.ino = file.getId();
        e.mode = file.getMode();
        e.gid = file.getGid();
        e.uid = file.getUid();
        e.CTime = file.getCreationTime();
        e.ATime = file.getLastAccessTime();
        e.MTime = file.getLastWriteTime();
        if (!file.isDirectory()) {
            e.size = file.getSize();
            try
            {
                if (cache.fileExists(e.path))
                    e.size = cache.fileGetSize(e.path);
            } catch (CBFSConnectException ex)
            {
                // ignore, as we have received the size in another way
            }
        }
    }

    public void init(FUSEInitEvent e) {}

    public void lock(FUSELockEvent e) {}

    public void mkDir(FUSEMkDirEvent e) {
        String[] names = splitName(e.path);
        if (names == null) {
            e.result = -ENOENT;
            return;
        }

        VirtualFile parent = Files.get(names[0]);
        if (parent == null) {
            e.result = -ENOENT;
            return;
        }
        if (!parent.isDirectory()) {
            e.result = -ENOTDIR;
            return;
        }
        if (parent.exists(names[1])) {
            e.result = -EEXIST;
            return;
        }

        try {
            VirtualFile dir = new VirtualFile(names[1], e.mode, fuse.getGid(), fuse.getUid());
            parent.add(names[1]);
            Files.add(e.path, dir);
            dir.setId(globals.alloc(dir));
        } catch (CBFSConnectException err) {
            e.result = err.getCode();
        }
    }

    public void open(FUSEOpenEvent e) {
        boolean contextFound = false;
        VirtualFile file = null;
        if (e.fileContext != 0) {
            file = (VirtualFile) globals.get(e.fileContext);
            contextFound = true;
        }

        if (!contextFound)
        {
            file = Files.get(e.path);
        }

        if (file == null) {
            e.result = -EBADF;
            return;
        }

        if (!contextFound)
            e.fileContext = globals.alloc(file);

        file.open();

        try
        {
            cache.fileOpen(e.path, file.size, Constants.PREFETCH_NOTHING, e.fileContext);
        }
        catch (CBFSConnectException ex)
        {
            e.result = -EIO;
        }
    }

    public void read(FUSEReadEvent e) {

        try
        {
            if (e.offset < 0)
            {
                e.result = -EFAULT;
                return;
            }

            int count;
            if (e.size > Integer.MAX_VALUE)
                count = Integer.MAX_VALUE;
            else
                count = (int)e.size;

            byte[] data = new byte[Math.min(BUFFER_SIZE, count)];

            int totalBytesRead = 0;
            long currPos = e.offset;

            int bytesToRead = 0;

            while (count > 0)
            {
                bytesToRead = Math.min(count, data.length);
                if (!cache.fileRead(e.path, currPos, data, 0, bytesToRead))
                {
                    e.result = -EFAULT;
                    return;
                }
                e.buffer.put(data, 0, bytesToRead);

                count -= bytesToRead;
                currPos += bytesToRead;
                totalBytesRead += bytesToRead;
            }
            e.result = totalBytesRead;
        }
        catch (CBFSConnectException ex)
        {
            e.result = -EIO;
        }

    }

    public void readDir(FUSEReadDirEvent e) {
        VirtualFile dir = Files.get(e.path);
        if (dir != null) {
            List<String> content = dir.enumerate();
            for (String name : content) {
                VirtualFile file = Files.get(combineNames(e.path, name));
                try {
                    long childSize =  file.getSize();

                    if (!file.isDirectory())
                    {
                        String fileName = e.path + "/" + file.name;
                        if (cache.fileExists(fileName))
                            childSize = cache.fileGetSize(fileName);
                    }
                    fuse.fillDir(e.fillerContext, name, file.getId(), file.getMode(), file.getUid(), file.getGid(),
                            file.isDirectory()? 2 : 1, childSize, file.getLastAccessTime(),
                            file.getLastWriteTime(), file.getCreationTime());
                } catch (CBFSConnectException err) {
                    e.result = err.getCode();
                }
            }
        } else {
            e.result = -ENOENT;
        }
    }

    public void release(FUSEReleaseEvent e) {
        if (e.fileContext != 0) {

            VirtualFile file = (VirtualFile)globals.get(e.fileContext);
            if (file == null)
            {
                e.result = -EBADF;
                return;
            }
            if (file.close()) // open count == 0
            {
                try
                {
                    cache.fileCloseEx(e.path, Constants.FLUSH_IMMEDIATE, Constants.PURGE_NONE);
                } catch (Exception ex)
                {
                    e.result = -EIO;
                    return;
                }
                e.fileContext = 0;
            }
        }
    }

    public void rename(FUSERenameEvent e) {
        VirtualFile file = Files.get(e.oldPath);
        if (file == null) {
            e.result = -ENOENT;
            return;
        }

        String[] oldNames = splitName(e.oldPath);
        if (oldNames == null) {
            e.result = -ENOENT;
            return;
        }

        VirtualFile oldParent = Files.get(oldNames[0]);
        if (oldParent == null) {
            e.result = -ENOENT;
            return;
        }

        String[] newNames = splitName(e.newPath);
        if (newNames == null) {
            e.result = -ENOENT;
            return;
        }

        VirtualFile newParent = Files.get(newNames[0]);
        if (newParent == null) {
            e.result = -ENOENT;
            return;
        }

        if (!newParent.getName().equals(oldParent.getName()) && newParent.exists(file.getName())) {
            if (e.flags != 0) {
                newParent.remove(file.getName());
            } else {
                e.result = -EEXIST;
            }
        }

        oldParent.remove(file.getName());
        file.setName(newNames[1]);
        Files.rename(e.oldPath, e.newPath);
        newParent.add(file.getName());

        try
        {
            if (cache.fileExists(e.newPath))
            {
                cache.fileDelete(e.newPath);
            }
            cache.fileChangeId(e.oldPath, e.newPath);
        }
        catch (CBFSConnectException ex)
        {
            // ignore?
        }
    }

    public void rmDir(FUSERmDirEvent e) {
        VirtualFile file = Files.get(e.path);
        if ((file == null) || (!file.isDirectory())) {
            e.result = -ENOENT;
            return;
        }
        Files.delete(e.path);
    }

    public void statFS(FUSEStatFSEvent e) {
        e.blockSize = SECTOR_SIZE;
        e.freeBlocks = (DRIVE_SIZE - Files.calcSize() + SECTOR_SIZE / 2) / SECTOR_SIZE;
        e.freeBlocksAvail = e.freeBlocks;
        e.totalBlocks = DRIVE_SIZE / SECTOR_SIZE;
        e.maxFilenameLength = 255;
    }

    public void truncate(FUSETruncateEvent e) {
        VirtualFile file = (VirtualFile) globals.get(e.fileContext);
        if (file == null) {
            file = Files.get(e.path);
            if (file == null) {
                e.result = -ENOENT;
                return;
            }
        }
        file.setSize((int)(e.size));
        try
        {
            cache.fileSetSize(e.path, e.size, false);
        } catch (CBFSConnectException ex)
        {
            throw new RuntimeException(ex);
        }
    }

    public void unlink(FUSEUnlinkEvent e) {
        VirtualFile file = Files.get(e.path);
        if (file == null) {
            e.result = -ENOENT;
            return;
        }
        if (file.isDirectory()) {
            e.result = -EISDIR;
            return;
        }
        Files.delete(e.path);
        try
        {
            cache.fileDelete(e.path);
        } catch (CBFSConnectException ex)
        {
            throw new RuntimeException(ex);
        }
    }

    public void utimens(FUSEUtimensEvent e) {
        VirtualFile file = Files.get(e.path);
        if (file == null) {
            e.result = -ENOENT;
            return;
        }
        if (!isNullOrEmpty(e.ATime))
            file.setLastAccessTime(e.ATime);
        if (!isNullOrEmpty(e.MTime))
            file.setLastWriteTime(e.MTime);
    }

    public void write(FUSEWriteEvent e) {
        try
        {
            if (e.offset < 0)
            {
                e.result = -EFAULT;
                return;
            }

            e.result = 0;

            int count;
            if (e.size > Integer.MAX_VALUE)
                count = Integer.MAX_VALUE;
            else
                count = (int)e.size;

            byte[] data = new byte[Math.min(BUFFER_SIZE, count)];
            int totalBytesWritten = 0;
            long currPos = e.offset;
            int bytesToWrite = 0;
            while (count > 0)
            {
                bytesToWrite = Math.min(count, data.length);
                e.buffer.get(data, 0, bytesToWrite);

                if (!cache.fileWrite(e.path, currPos, data, 0, bytesToWrite))
                    break;

                count -= bytesToWrite;
                currPos += bytesToWrite;
                totalBytesWritten += bytesToWrite;
            }

            e.result = totalBytesWritten;
        }
        catch (CBFSConnectException ex)
        {
            e.result = -EIO;
        }
    }

    // End of FUSEEventListener implementation
    // --------------------------------------

    private enum DriverStatus {
        NOT_INSTALLED,
        STOPPED,
        START_PENDING,
        STOP_PENDING,
        RUNNING,
        CONTINUE_PENDING,
        PAUSE_PENDING,
        PAUSED;

        public static DriverStatus fromInt(int value) {
            switch (value) {
                case 0:
                    return NOT_INSTALLED;
                case 1:
                    return STOPPED;
                case 2:
                    return START_PENDING;
                case 3:
                    return STOP_PENDING;
                case 4:
                    return RUNNING;
                case 5:
                    return CONTINUE_PENDING;
                case 6:
                    return PAUSE_PENDING;
                case 7:
                    return PAUSED;
                default:
                    return null;
            }
        }

        @Override
        public String toString() {
            switch (this) {
                case NOT_INSTALLED:
                    return "not installed";
                case STOPPED:
                    return "stopped";
                case START_PENDING:
                    return "start pending";
                case STOP_PENDING:
                    return "stop pending";
                case RUNNING:
                    return "running";
                case CONTINUE_PENDING:
                    return "continue pending";
                case PAUSE_PENDING:
                    return "pause pending";
                case PAUSED:
                    return "paused";
                default:
                    return super.toString();
            }
        }
    }

    static class VirtualFile {
        private long id;
        private int mode;
        private int gid;
        private int uid;
        private String name;
        private Date creationTime;
        private Date lastAccessTime;
        private Date lastWriteTime;

        private final List<String> entries;
        private byte[] data;
        private int size;

        private AtomicInteger openCount;

        VirtualFile(String name, int mode, int gid, int uid) {
            this.name = name;
            this.mode = mode;
            this.gid = gid;
            this.uid = uid;
            creationTime = new Date();
            lastAccessTime = new Date();
            lastWriteTime = new Date();
            if (isDirectory())
                entries = new ArrayList<String>();
            else
                entries = null;
            data = null;
            size = 0;

            openCount = new AtomicInteger(0);
        }

        boolean add(String name) {
            if (entries == null || isNullOrEmpty(name))
                return false;
            synchronized (entries) {
                String lowered = name;
                if (entries.contains(lowered))
                    return false;
                entries.add(lowered);
                return true;
            }
        }

        void clear() {
            name = null;
            if (entries != null) {
                synchronized (entries) {
                    entries.clear();
                }
            }
            if (data != null) {
                Arrays.fill(data, (byte) 0);
                data = null;
            }
            size = 0;
        }

        List<String> enumerate() {
            if (!isDirectory())
                return null;
            else
                return entries;
        }

        boolean exists(String name) {
            if (entries == null || isNullOrEmpty(name))
                return false;

            synchronized (entries) {
                return entries.contains(name);
            }
        }

        int getAllocationSize() {
            synchronized (this)
            {
                return (data == null) ? 0 : data.length;
            }
        }

        long getId() {
            return id;
        }

        int getMode() {
            return mode;
        }

        int getGid() {
            return gid;
        }

        int getUid() {
            return uid;
        }

        Date getCreationTime() {
            return creationTime;
        }

        Date getLastAccessTime() {
            return lastAccessTime;
        }

        Date getLastWriteTime() {
            return lastWriteTime;
        }

        String getName() {
            return name;
        }

        int getSize() {
            synchronized (this)
            {
                return size;
            }
        }

        boolean isDirectory() {
            return (mode & MODE_DIR) != 0;
        }

        boolean isEmpty() {
            if (entries != null)
                synchronized (entries) {
                    return (entries.size() == 0);
                }

            return (size == 0);
        }

        int read(long position, long bytesToRead, ByteBuffer buffer) {
            synchronized (this)
            {
                if (position < 0 || position >= size)
                    return 0;
                int count = (int) Math.min(bytesToRead, size - position);
                if (count != 0)
                    buffer.put(data, (int) position, count);
                return count;
            }
        }

        void remove(String name) {
            if (entries == null || isNullOrEmpty(name))
                return;
            synchronized (entries) {
                String lowered = name;
                if (!entries.contains(lowered))
                    return;
                entries.remove(lowered);
            }
        }

        void setAllocationSize(int newSize)
        {
            synchronized (this)
            {
                if (isDirectory() || newSize == getAllocationSize())
                    return;

                if (data == null || data.length == 0)
                    data = new byte[(int) newSize];
                else
                {
                    byte[] newData = new byte[newSize];
                    System.arraycopy(data, 0, newData, 0, Math.min(data.length, newSize));
                    Arrays.fill(data, (byte) 0);
                    data = newData;
                }
                size = newSize;
            }
        }

        void setId(long newId) {
            this.id = newId;
        }

        void setMode(int newMode) {
            this.mode = newMode;
        }

        void setGid(int newGid) {
            this.gid = newGid;
        }

        void setUid(int newUid) {
            this.uid = newUid;
        }

        void setCreationTime(Date newTime) {
            if (newTime != null)
                this.creationTime = (Date) newTime.clone();
        }

        void setLastAccessTime(Date newTime) {
            if (newTime != null)
                this.lastAccessTime = (Date) newTime.clone();
        }

        void setLastWriteTime(Date newTime) {
            if (newTime != null)
                this.lastWriteTime = (Date) newTime.clone();
        }

        void setSize(int newSize) {
            synchronized (this)
            {
                if (isDirectory() || size == newSize)
                    return;

                if (newSize < size)
                    size = newSize;
                else
                {
                    if (newSize <= getAllocationSize())
                        size = newSize;
                    else
                        setAllocationSize(newSize);
                }
            }
        }

        void setName(String newName) {
            this.name = newName;
        }

        int write(ByteBuffer buffer, long position, long bytesToWrite) {
            synchronized (this)
            {
                if (position < 0 || position >= size)
                    return -1;


                int count = (int) Math.min(bytesToWrite, size - position);
                if (count > 0)
                    buffer.get(data, (int) position, count);
                else
                    count = 0;
                return count;
            }
        }

        public void open()
        {
            openCount.incrementAndGet();
        }

        public boolean close()
        {
            return (openCount.decrementAndGet() == 0);
        }
    }

    private static class Files {
        private static final Map<String, VirtualFile> entries;

        static {
            entries = new HashMap<String, VirtualFile>();
            entries.put("/", new VirtualFile("", MODE_DIR, 0, 0));
        }

        static void add(String name, VirtualFile file) {
            if (isNullOrEmpty(name) || file == null)
                return;
            synchronized (entries) {
                entries.put(name, file);
            }
        }

        static long calcSize() {
            synchronized (entries) {
                long size = 0;
                for (String name: entries.keySet()) {
                    size += entries.get(name).getSize();
                }
                return size;
            }
        }

        static void clear() {
            synchronized (entries) {
                for (String name : entries.keySet()) {
                    VirtualFile file = entries.get(name);
                    if (file != null)
                        file.clear();
                }
                entries.clear();
            }
        }

        static void delete(String name) {
            if (isNullOrEmpty(name))
                return;

            synchronized (entries) {
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

        static VirtualFile get(String name) {
            if (isNullOrEmpty(name))
                return null;
            synchronized (entries) {
                return entries.get(name);
            }
        }

        static void rename(String name, String newName) {
            if (entries == null)
                return;

            synchronized (entries) {
                VirtualFile file = entries.remove(name);
                if (file == null)
                    return;

                entries.put(newName, file);

                if (file.isDirectory() && !file.isEmpty())
                    renameChildItems(name, newName);
            }
        }

        static private void renameChildItems(String name, String newName) {
            List<String> pathsToRename = new ArrayList();
            int pathOffset = name.length();

            for (String path : entries.keySet()) {
                if (path.startsWith(name + "/"))
                    pathsToRename.add(path);
            }

            for (String path : pathsToRename) {
                VirtualFile file = entries.remove(path);
                if (file == null)
                    continue;

                String newPath = newName + path.substring(pathOffset);
                entries.put(newPath, file);
            }
        }
    }
}




