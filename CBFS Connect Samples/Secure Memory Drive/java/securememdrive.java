/*
 * CBFS Connect 2022 Java Edition - Sample Project
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
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.io.File;
import java.nio.ByteBuffer;
import java.util.*;
import java.util.List;
import javax.swing.*;
import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;
import javax.swing.filechooser.FileFilter;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.regex.Pattern;

//
// This project relies on JNA
//
import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.platform.win32.Advapi32;
import com.sun.jna.platform.win32.Kernel32;
import com.sun.jna.platform.win32.Kernel32Util;
import com.sun.jna.platform.win32.WinDef.*;
import static com.sun.jna.platform.win32.WinNT.*;
import static com.sun.jna.platform.win32.WinNT.SECURITY_IMPERSONATION_LEVEL.SecurityImpersonation;
import static com.sun.jna.platform.win32.WinNT.TOKEN_INFORMATION_CLASS.TokenUser;
import com.sun.jna.ptr.IntByReference;
import com.sun.jna.ptr.PointerByReference;
import com.sun.jna.win32.StdCallLibrary;
import com.sun.jna.win32.W32APIOptions;

import cbfsconnect.*;


//
// ------------------------ securememdrive ---------------------------------
//
// Key method:
//  securememdrive::fileSecureCheck,
//     This method checks the permissions of a client token through the security descriptor of file.
//
//  VirtualFile::allocateDefaultSecurityDescriptor,
//     This method assigns default security descriptors to files and directories.
//
// ------------------------------------------------------------------------
//
public class securememdrive extends JFrame implements CbfsEventListener {

    private static final String PRODUCT_GUID = "{713CC6CE-B3E2-4fd9-838D-E28F558F6866}";
    private static final int SECTOR_SIZE = 512;
    private static final long DRIVE_SIZE = 10 * 1024 * 1024;    // 10MB

    private static final int GAP = 6;
    private static final int DGAP = GAP * 2;

    private static final int ERROR_FILE_NOT_FOUND = 2;
    private static final int ERROR_PATH_NOT_FOUND = 3;
    private static final int ERROR_ACCESS_DENIED = 5;
    private static final int ERROR_WRITE_FAULT = 29;
    private static final int ERROR_READ_FAULT = 30;
    private static final int ERROR_FILE_EXISTS = 80;
    private static final int ERROR_INVALID_NAME = 123;
    private static final int ERROR_PRIVILEGE_NOT_HELD = 1314;
    private static final int ERROR_INTERNAL_ERROR = 0x54f;

    private static final Pattern DRIVE_LETTER_PATTERN = Pattern.compile("^[A-Za-z]:$");

    private static final long EMPTY_DATE;

    private Cbfs cbfs;
    private String cabFileLocation;
    private boolean initialized;
    private boolean driverRunning;
    private boolean storageCreated;
    private boolean mediaMounted;

    private JPanel panelDriver;
    private JLabel labelVersion;
    private JLabel labelStatus;
    private JButton buttonInstall;
    private JButton buttonUinstall;

    private JPanel panelOperations;
    private JButton buttonCreateStorage;
    private JButton buttonMountMedia;
    private JButton buttonUnmountMedia;
    private JButton buttonDeleteStorage;

    private JPanel panelMountingPoints;
    private JTextField textMountingPoint;
    private JButton buttonAddMountingPoint;
    private JButton buttonRemoveMountingPoint;
    private JScrollPane scrollMountingPoints;
    private JList listMountingPoints;
    private DefaultListModel modelMountingPoints;

    static {
        Calendar utc = Calendar.getInstance(TimeZone.getTimeZone("UTC"));
        utc.set(1601, Calendar.JANUARY, 1, 0, 0, 0);
        utc.clear(Calendar.MILLISECOND);
        EMPTY_DATE = utc.getTimeInMillis();
    }

    private securememdrive() {
        super("Secure Memory Drive");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLayout(null);
        setLocationByPlatform(true);
        setResizable(false);
        setSize(400, 320);
        setVisible(true);

        initializeControls();

        cbfs = new Cbfs();
        updateDriverStatus();
        updateControls();
        repaint();
    }

    private boolean fileSecureCheck(Pointer securityDescriptor, HANDLE impersonatedToken, DWORD accessRights) {
        boolean allow = false;

        DWORDByReference accessRightsRef = new DWORDByReference(accessRights);
        DWORDByReference grantedAccess = new DWORDByReference();
        PRIVILEGE_SET privileges = new PRIVILEGE_SET(1);
        DWORDByReference privilegesLength = new DWORDByReference(new DWORD(privileges.size()));
        GENERIC_MAPPING mapping = new GENERIC_MAPPING();
        BOOLByReference result = new BOOLByReference();

        mapping.genericRead    = new DWORD(FILE_GENERIC_READ);
        mapping.genericWrite   = new DWORD(FILE_GENERIC_WRITE);
        mapping.genericExecute = new DWORD(FILE_GENERIC_EXECUTE);
        mapping.genericAll     = new DWORD(FILE_ALL_ACCESS);

        Advapi32.INSTANCE.MapGenericMask(accessRightsRef, mapping);
        if (!Advapi32.INSTANCE.AccessCheck(securityDescriptor, impersonatedToken, accessRightsRef.getValue(),
                mapping, privileges, privilegesLength, grantedAccess, result)) {

            int dwError = Kernel32.INSTANCE.GetLastError();
        }
        else
            allow = result.getValue().booleanValue();

        return allow;
    }

    private void buttonInstallClick() {
        JFileChooser chooser = createOpenCabChooser();
        if (chooser.showOpenDialog(this) != JFileChooser.APPROVE_OPTION)
            return;

        cabFileLocation = chooser.getSelectedFile().getParent();
        boolean rebootNeeded;

        try {
            rebootNeeded = cbfs.install(chooser.getSelectedFile().getName(), PRODUCT_GUID, null,
                    Constants.MODULE_DRIVER, Constants.INSTALL_REMOVE_OLD_VERSIONS) != 0;
            updateDriverStatus();
            updateControls();
        }
        catch (CBFSConnectException err) {
            int errCode = err.getCode();
            if (errCode == ERROR_PRIVILEGE_NOT_HELD)
                JOptionPane.showMessageDialog(this,
                        "Installation requires administrator rights. Please run the app as Administrator",
                        getTitle(), JOptionPane.WARNING_MESSAGE);
            else
                JOptionPane.showMessageDialog(this,
                        "Installation failed.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
            return;
        }

        if (rebootNeeded)
            JOptionPane.showMessageDialog(this,
                    "System restart is needed for the changes to take effect", getTitle(), JOptionPane.WARNING_MESSAGE);
        else
            JOptionPane.showMessageDialog(this,
                    "Driver installed successfully", getTitle(), JOptionPane.INFORMATION_MESSAGE);
    }

    private void buttonUinstallClick() {
        JFileChooser chooser = createOpenCabChooser();
        if (chooser.showOpenDialog(this) != JFileChooser.APPROVE_OPTION)
            return;

        cabFileLocation = chooser.getSelectedFile().getParent();
        boolean rebootNeeded;

        try {
            rebootNeeded = cbfs.uninstall(chooser.getSelectedFile().getPath(), PRODUCT_GUID, null,
                    Constants.UNINSTALL_VERSION_CURRENT) != 0;
            updateDriverStatus();
            updateControls();
        }
        catch (CBFSConnectException err) {
            int errCode = err.getCode();
            if (errCode == ERROR_PRIVILEGE_NOT_HELD)
                JOptionPane.showMessageDialog(this,
                        "Uninstallation requires administrator rights. Please run the app as Administrator",
                        getTitle(), JOptionPane.WARNING_MESSAGE);
            else
                JOptionPane.showMessageDialog(this,
                        "Uninstallation failed.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
            return;
        }

        if (rebootNeeded)
            JOptionPane.showMessageDialog(this,
                    "System restart is needed for the changes to take effect", getTitle(), JOptionPane.WARNING_MESSAGE);
        else
            JOptionPane.showMessageDialog(this,
                    "Driver uninstalled successfully", getTitle(), JOptionPane.INFORMATION_MESSAGE);
    }

    private void buttonCreateStorageClick() {
        try {
            if (!initialized) {
                cbfs.initialize(PRODUCT_GUID);

                //
                // Enable Windows Security
                //
                cbfs.setFileSystemName("NTFS");
                cbfs.setUseWindowsSecurity(true);
                cbfs.setFireAllOpenCloseEvents(true);

                cbfs.setSerializeAccess(true);
                cbfs.setSerializeEvents(1);
                cbfs.addCbfsEventListener(this);
                initialized = true;
            }
            cbfs.createStorage();
            storageCreated = true;
            updateControls();
        }
        catch (Exception err) {
            JOptionPane.showMessageDialog(this,
                    "Failed to create a storage.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
        }
    }

    private void buttonMountMediaClick() {
        try {
            cbfs.mountMedia(0);
            mediaMounted = true;
            updateControls();
            updateMountingPoints();
        }
        catch (CBFSConnectException err) {
            JOptionPane.showMessageDialog(this,
                    "Failed to mount media.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
        }
    }

    private void buttonUnmountMediaClick() {
        try {
            cbfs.unmountMedia(true);
            mediaMounted = false;
            updateControls();
        }
        catch (CBFSConnectException err) {
            JOptionPane.showMessageDialog(this,
                    "Failed to unmount media.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
        }
    }

    private void buttonDeleteStorageClick() {
        try {
            cbfs.deleteStorage(true);
            modelMountingPoints.clear();
            storageCreated = false;
            updateControls();
        }
        catch (CBFSConnectException err) {
            JOptionPane.showMessageDialog(this,
                    "Failed to delete a storage.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
        }
    }

    private void buttonAddMountingPointClick() {
        try {
            String mountingPoint = textMountingPoint.getText();
            if (mountingPoint.length() == 1) {
                mountingPoint += ":";
            }
            mountingPoint = ConvertRelativePathToAbsolute(mountingPoint, true);

            if (isNullOrEmpty(mountingPoint)) {
                JOptionPane.showMessageDialog(this, "Mounting point not specified.", getTitle(), JOptionPane.ERROR_MESSAGE);
                return;
            }
            cbfs.addMountingPoint(mountingPoint, Constants.STGMP_MOUNT_MANAGER, 0);
            updateMountingPoints();
        }
        catch (CBFSConnectException err) {
            JOptionPane.showMessageDialog(this,
                    "Failed to add a mounting point.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
        }
        updateControls();
    }

    private void buttonRemoveMountingPointClick() {
        try {
            int index = listMountingPoints.getSelectedIndex();
            cbfs.removeMountingPoint(index, "", 0, 0);
            updateMountingPoints();
        }
        catch (CBFSConnectException err) {
            JOptionPane.showMessageDialog(this,
                    "Failed to remove a mounting point.\n" + err.getMessage(), getTitle(), JOptionPane.ERROR_MESSAGE);
        }
        updateControls();
    }

    private void updateControls() {
        buttonInstall.setEnabled(!driverRunning);
        buttonUinstall.setEnabled(driverRunning && !storageCreated);

        buttonCreateStorage.setEnabled(driverRunning && !storageCreated);
        buttonMountMedia.setEnabled(storageCreated && !mediaMounted);
        buttonUnmountMedia.setEnabled(mediaMounted);
        buttonDeleteStorage.setEnabled(storageCreated && !mediaMounted);

        buttonAddMountingPoint.setEnabled(mediaMounted);
        buttonRemoveMountingPoint.setEnabled(listMountingPoints.getSelectedIndex() >= 0);
    }

    private void updateDriverStatus() {
        try {
            DriverStatus status = DriverStatus.fromInt(cbfs.getDriverStatus(PRODUCT_GUID, Constants.MODULE_DRIVER));
            driverRunning = (status == DriverStatus.RUNNING);

            if (status == null) {
                labelVersion.setText("Version: ERROR");
                labelStatus.setText("Status: Failed to get driver status");
            }
            else
            if (status == DriverStatus.NOT_INSTALLED) {
                labelVersion.setText("Version: not installed");
                labelStatus.setText("Service: not installed");
            }
            else {
                long version = cbfs.getModuleVersion(PRODUCT_GUID, Constants.MODULE_DRIVER);
                labelVersion.setText(String.format("Version: %d.%d.%d.%d",
                        version >> 48, (version >> 32) & 0xffff, (version >> 16) & 0xffff, version & 0xffff));
                labelStatus.setText(String.format("Service: %s", status));
            }
        }
        catch (CBFSConnectException err) {
            labelVersion.setText("Version: ERROR");
            labelStatus.setText(err.getMessage());
            driverRunning = false;
        }
    }

    private void updateMountingPoints() {
        modelMountingPoints.clear();
        MountingPointList mountingPoints = cbfs.getMountingPoints();
        for (int i = 0; i < mountingPoints.size(); i++)
            modelMountingPoints.addElement(mountingPoints.item(i).getName());
    }

    private void initializeControls() {
        Insets frameArea = getClientArea(this);

        panelDriver = new JPanel(null);
        panelDriver.setBorder(BorderFactory.createTitledBorder(" Driver "));
        panelDriver.setLocation(GAP, GAP);
        panelDriver.setSize(frameArea.right - frameArea.left - DGAP, 95);
        add(panelDriver);
        Insets panelArea = getClientArea(panelDriver);

        buttonInstall = new JButton("Install...");
        buttonInstall.setBounds(panelArea.right - GAP - 95, panelArea.top + GAP, 95, 25);
        buttonInstall.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonInstallClick();
            }
        });
        panelDriver.add(buttonInstall);

        buttonUinstall = new JButton("Uninstall...");
        buttonUinstall.setBounds(panelArea.right - GAP - 95, getBottom(buttonInstall) + GAP, 95, 25);
        buttonUinstall.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonUinstallClick();
            }
        });
        panelDriver.add(buttonUinstall);

        labelVersion = new JLabel("Version: not installed");
        labelVersion.setLocation(panelArea.left + GAP, panelArea.top + GAP);
        labelVersion.setSize(buttonInstall.getX() - labelVersion.getX() - GAP, 18);
        panelDriver.add(labelVersion);

        labelStatus = new JLabel("Service: not installed");
        labelStatus.setLocation(panelArea.left + GAP, labelVersion.getY() + labelVersion.getHeight() + GAP);
        labelStatus.setSize(buttonInstall.getX() - labelVersion.getX() - GAP, 18);
        panelDriver.add(labelStatus);

        panelOperations = new JPanel(null);
        panelOperations.setBorder(BorderFactory.createTitledBorder(" Operations "));
        panelOperations.setLocation(GAP, getBottom(panelDriver) + GAP);
        panelOperations.setSize(160,
                frameArea.bottom - panelOperations.getY() - frameArea.top - GAP);
        add(panelOperations);
        panelArea = getClientArea(panelOperations);

        buttonCreateStorage = new JButton("1. Create Storage");
        buttonCreateStorage.setBounds(panelArea.left + GAP, panelArea.top + GAP,
                panelArea.right - panelArea.left - DGAP, 25);
        buttonCreateStorage.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonCreateStorageClick();
            }
        });
        panelOperations.add(buttonCreateStorage);

        buttonMountMedia = new JButton("2. Mount Media");
        buttonMountMedia.setBounds(panelArea.left + GAP, getBottom(buttonCreateStorage) + GAP,
                buttonCreateStorage.getWidth(), buttonCreateStorage.getHeight());
        buttonMountMedia.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonMountMediaClick();
            }
        });
        panelOperations.add(buttonMountMedia);

        buttonUnmountMedia = new JButton("5. Unmount Media");
        buttonUnmountMedia.setBounds(panelArea.left + GAP, getBottom(buttonMountMedia) + GAP * 4,
                buttonCreateStorage.getWidth(), buttonCreateStorage.getHeight());
        buttonUnmountMedia.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonUnmountMediaClick();
            }
        });
        panelOperations.add(buttonUnmountMedia);

        buttonDeleteStorage = new JButton("6. Delete Storage");
        buttonDeleteStorage.setBounds(panelArea.left + GAP, getBottom(buttonUnmountMedia) + GAP,
                buttonCreateStorage.getWidth(), buttonCreateStorage.getHeight());
        buttonDeleteStorage.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonDeleteStorageClick();
            }
        });
        panelOperations.add(buttonDeleteStorage);

        panelMountingPoints = new JPanel(null);
        panelMountingPoints.setBorder(BorderFactory.createTitledBorder(" Mounting Points "));
        panelMountingPoints.setLocation(getRight(panelOperations) + GAP, getBottom(panelDriver) + GAP);
        panelMountingPoints.setSize(getRight(panelDriver) - getRight(panelOperations) - GAP, panelOperations.getHeight());
        add(panelMountingPoints);
        panelArea = getClientArea(panelMountingPoints);

        textMountingPoint = new JTextField();
        textMountingPoint.setBounds(panelArea.left + GAP, panelArea.top + GAP,
                panelArea.right - panelArea.left - DGAP, 25);
        textMountingPoint.setText("Z:");
        panelMountingPoints.add(textMountingPoint);

        buttonAddMountingPoint = new JButton("3. Add");
        buttonAddMountingPoint.setBounds(panelArea.left + GAP, getBottom(textMountingPoint) + GAP,
                (textMountingPoint.getWidth() - GAP) / 2, 25);
        buttonAddMountingPoint.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonAddMountingPointClick();
            }
        });
        panelMountingPoints.add(buttonAddMountingPoint);

        buttonRemoveMountingPoint = new JButton("4. Remove");
        buttonRemoveMountingPoint.setBounds(getRight(buttonAddMountingPoint) + GAP, buttonAddMountingPoint.getY(),
                getRight(textMountingPoint) - getRight(buttonAddMountingPoint) - GAP, 25);
        buttonRemoveMountingPoint.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent actionEvent) {
                buttonRemoveMountingPointClick();
            }
        });
        panelMountingPoints.add(buttonRemoveMountingPoint);

        modelMountingPoints = new DefaultListModel();
        listMountingPoints = new JList(modelMountingPoints);
        listMountingPoints.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
        listMountingPoints.addListSelectionListener(new ListSelectionListener() {
            public void valueChanged(ListSelectionEvent e) {
                buttonRemoveMountingPoint.setEnabled(listMountingPoints.getSelectedIndex() >= 0);
            }
        });
        JScrollPane scrollMountingPoints = new JScrollPane(listMountingPoints);
        scrollMountingPoints.setLocation(panelArea.left + GAP, getBottom(buttonAddMountingPoint) + GAP);
        scrollMountingPoints.setSize(textMountingPoint.getWidth(), panelArea.bottom - getBottom(buttonAddMountingPoint) - DGAP);
        panelMountingPoints.add(scrollMountingPoints);
    }

    private Insets getClientArea(JComponent c) {
        Rectangle b = c.getBounds();
        Insets i = c.getInsets();
        return new Insets(i.top, i.left,b.height - i.bottom,b.width - i.right);
    }

    private Insets getClientArea(JFrame f) {
        Rectangle b = f.getBounds();
        Insets i = f.getInsets();
        return new Insets(i.top, i.left,b.height - i.bottom,b.width - i.right);
    }

    private int getBottom(JComponent c) {
        return c.getY() + c.getHeight();
    }

    private int getRight(JComponent c) {
        return c.getX() + c.getWidth();
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
                    JOptionPane.showMessageDialog(this,
                            "The path '" + path + "' format cannot be equal to the Network Mounting Point", getTitle(), JOptionPane.ERROR_MESSAGE);
                    return path;
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
                    JOptionPane.showMessageDialog(this,
                            "The path '" + res + "' format cannot be equal to the Drive Letter", getTitle(), JOptionPane.ERROR_MESSAGE);
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

    private static boolean isNullOrEmpty(String s) {
        return (s == null) || (s.length() == 0);
    }

    private static boolean isNullOrEmpty(Date d) {
        return (d == null) || (d.getTime() == EMPTY_DATE);
    }

    private static String[] splitName(String value) {
        if (isNullOrEmpty(value) || value.equals("\\"))
            return null;
        int index = value.lastIndexOf('\\');
        if (index < 0)
            return null;

        String[] parts = new String[2];
        if (index == 0)
            parts[0] = "\\";
        else
            parts[0] = value.substring(0, index);
        parts[1] = value.substring(index + 1);
        return parts;
    }

    private static String combineNames(String parent, String name) {
        if (isNullOrEmpty(parent) || isNullOrEmpty(name))
            return null;
        if (parent.endsWith("\\"))
            return parent + name;
        return parent + "\\" + name;
    }

    private JFileChooser createOpenCabChooser() {
        JFileChooser chooser = new JFileChooser();
        chooser.setDialogTitle("Select CBFS driver package");
        chooser.setMultiSelectionEnabled(false);
        chooser.setFileFilter(new CabFileFilter());
        chooser.setAcceptAllFileFilterUsed(false);
        if (!isNullOrEmpty(cabFileLocation))
            chooser.setCurrentDirectory(new File(cabFileLocation));
        return chooser;
    }

    private static void log(Object o) {
        String info = (o == null) ? "null" : o.toString();
        System.err.println(info);
    }

    public static void main(String[] args) {
        new securememdrive();
    }

    // -----------------------------------
    // Implementation of CbfsEventListener

    public void canFileBeDeleted(CbfsCanFileBeDeletedEvent e) {
        e.canBeDeleted = true;
    }

    public void cleanupFile(CbfsCleanupFileEvent e) {
        // not needed in this demo
    }

    public void closeDirectoryEnumeration(CbfsCloseDirectoryEnumerationEvent e) {
        if (e.enumerationContext != 0) {
            globals.free(e.enumerationContext);
            e.enumerationContext = 0;
        }
    }

    public void closeFile(CbfsCloseFileEvent e) {

        if (e.fileContext != 0) {
            VirtualFile file = (VirtualFile) globals.get(e.fileContext);
            if (file.derOpen() == 0)
                globals.free(e.fileContext);
        }
    }

    public void closeHardLinksEnumeration(CbfsCloseHardLinksEnumerationEvent e) {
        // out of the demo's scope
    }

    public void closeNamedStreamsEnumeration(CbfsCloseNamedStreamsEnumerationEvent e) {
        // out of the demo's scope
    }

    public void closeQuotasEnumeration(CbfsCloseQuotasEnumerationEvent e) {
        // out of the demo's scope
    }

    public void createFile(CbfsCreateFileEvent e) {
        String[] names = splitName(e.fileName);
        if (names == null) {
            e.resultCode = ERROR_INVALID_NAME;
            return;
        }

        VirtualFile parent = Files.get(names[0]);
        if (parent == null) {
            e.resultCode = ERROR_PATH_NOT_FOUND;
            return;
        }
        if (parent.exists(names[1])) {
            e.resultCode = ERROR_FILE_EXISTS;
            return;
        }

        //
        // Access Check
        //
        try
        {
            boolean allow;

            int AccessRights = ((e.attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ? FILE_ADD_SUBDIRECTORY : FILE_ADD_FILE;

            HANDLE hToken;
            HANDLEByReference hImpersonatedTokenRef = new HANDLEByReference();
            hToken = new HANDLE(new Pointer(cbfs.getOriginatorToken()));
            Advapi32.INSTANCE.DuplicateToken(hToken, SecurityImpersonation, hImpersonatedTokenRef);

            allow = fileSecureCheck(parent.getSecurityDescriptor(), hImpersonatedTokenRef.getValue(), new DWORD(AccessRights));
            Kernel32.INSTANCE.CloseHandle(hImpersonatedTokenRef.getValue());

            if (!allow) {
                e.resultCode = ERROR_ACCESS_DENIED;
                return;
            }
        }
        catch (Exception ex)
        {
            e.resultCode = ERROR_INTERNAL_ERROR;
            return;
        }

        VirtualFile file = new VirtualFile(names[1], e.attributes & 0xFFFF);  // Attributes contains creation flags as well, which we need to strip
        parent.add(names[1]);
        Files.add(e.fileName, file);
        file.refOpen();
        e.fileContext = globals.alloc(file);
    }

    public void createHardLink(CbfsCreateHardLinkEvent e) {
        // out of the demo's scope
    }

    public void deleteFile(CbfsDeleteFileEvent e) {
        if (Files.get(e.fileName) == null) {
            e.resultCode = ERROR_FILE_NOT_FOUND;
            return;
        }

        Files.delete(e.fileName);
    }

    public void deleteReparsePoint(CbfsDeleteReparsePointEvent e) {
        // out of the demo's scope
    }

    public void ejected(CbfsEjectedEvent e) {
        // not needed in this demo
    }

    public void enumerateDirectory(CbfsEnumerateDirectoryEvent e) {
        EnumContext context;

        if (e.restart && e.enumerationContext != 0) {
            globals.free(e.enumerationContext);
            e.enumerationContext = 0;
        }

        if (e.enumerationContext == 0) {
            VirtualFile dir = Files.get(e.directoryName);
            if (dir == null) {
                e.resultCode = ERROR_PATH_NOT_FOUND;
                return;
            }
            context = new EnumContext(e.directoryName, dir.enumerate(e.mask));
            e.enumerationContext = globals.alloc(context);
        }
        else {
            context = (EnumContext)globals.get(e.enumerationContext);
            if (context == null) {
                e.resultCode = ERROR_FILE_NOT_FOUND;
                e.enumerationContext = 0;
                e.fileFound = false;
                return;
            }
        }

        String name = context.getNext();
        if (name == null) {
            e.fileFound = false;
            return;
        }

        VirtualFile file = Files.get(name);
        if (file == null) {
            e.fileFound = false;
            return;
        }

        e.fileFound = true;
        e.fileName = file.getName();
        e.attributes = file.getAttributes();
        e.creationTime = file.getCreationTime();
        e.lastAccessTime = file.getLastAccessTime();
        e.lastWriteTime = file.getLastWriteTime();
        if (!file.isDirectory()) {
            e.allocationSize = file.getAllocationSize();
            e.size = file.getSize();
        }
    }

    public void enumerateHardLinks(CbfsEnumerateHardLinksEvent e) {
        // out of the demo's scope
    }

    public void enumerateNamedStreams(CbfsEnumerateNamedStreamsEvent e) {
        // out of the demo's scope
    }

    public void error(CbfsErrorEvent e) {
        log("ERROR: " + e.toString());
    }

    public void flushFile(CbfsFlushFileEvent e) {
        // not needed in this demo
    }

    public void fsctl(CbfsFsctlEvent e) {
        // not needed in this demo
    }

    public void getDefaultQuotaInfo(CbfsGetDefaultQuotaInfoEvent e) {
        // out of the demo's scope
    }

    public void getFileInfo(CbfsGetFileInfoEvent e) {
        if (isNullOrEmpty(e.fileName)) {
            e.resultCode = ERROR_INVALID_NAME;
            return;
        }

        VirtualFile file = Files.get(e.fileName);
        if (file == null) {
            e.fileExists = false;
            return;
        }

        e.fileExists = true;
        e.attributes = file.getAttributes();
        e.creationTime = file.getCreationTime();
        e.lastAccessTime = file.getLastAccessTime();
        e.lastWriteTime = file.getLastWriteTime();
        if (!file.isDirectory()) {
            e.allocationSize = file.getAllocationSize();
            e.size = file.getSize();
        }
    }

    public void getFileNameByFileId(CbfsGetFileNameByFileIdEvent e) {
        // out of the demo's scope
    }

    public void getFileSecurity(CbfsGetFileSecurityEvent e) {
        VirtualFile file = (VirtualFile) globals.get(e.fileContext);
        IntByReference descLenRef = new IntByReference(e.descriptorLength);
        e.resultCode = file.getSecurityDescriptor(e.securityInformation, e.securityDescriptor, e.bufferLength, descLenRef);
        e.descriptorLength = descLenRef.getValue();
    }

    public void getReparsePoint(CbfsGetReparsePointEvent e) {
        // out of the demo's scope
    }

    public void getVolumeId(CbfsGetVolumeIdEvent e) {
        e.volumeId = 0x12345678;
    }

    public void getVolumeLabel(CbfsGetVolumeLabelEvent e) {
        e.buffer = "Secure Memory Drive";
    }

    public void getVolumeObjectId(CbfsGetVolumeObjectIdEvent e) {
        // out of the demo's scope
    }

    public void getVolumeSize(CbfsGetVolumeSizeEvent e) {
        e.totalSectors = DRIVE_SIZE / SECTOR_SIZE;
        e.availableSectors = (DRIVE_SIZE - Files.calcSize() + SECTOR_SIZE / 2) / SECTOR_SIZE;
    }

    public void ioctl(CbfsIoctlEvent e) {
        // not needed in this demo
    }

    public void isDirectoryEmpty(CbfsIsDirectoryEmptyEvent e) {
        VirtualFile dir = Files.get(e.directoryName);
        if (dir == null)
            e.resultCode = ERROR_FILE_NOT_FOUND;
        else
            e.isEmpty = dir.isEmpty();
    }

    public void lockFile(CbfsLockFileEvent e) {
        // not needed in this demo
    }

    public void mount(CbfsMountEvent e) {
        // not needed in this demo
    }

    public void offloadReadFile(CbfsOffloadReadFileEvent e) {
        // not needed in this demo
    }

    public void offloadWriteFile(CbfsOffloadWriteFileEvent e) {
        // not needed in this demo
    }

    public void openFile(CbfsOpenFileEvent e) {
        VirtualFile file;
        if (e.fileContext != 0) {
            file = (VirtualFile) globals.get(e.fileContext);
        }else {
            file = Files.get(e.fileName);
        }
        if (file == null) {
            e.resultCode = ERROR_FILE_NOT_FOUND;
            return;
        }

        //
        // Access Check
        //
        try
        {
            boolean allow;

            int AccessRights = e.desiredAccess;

            HANDLE hToken;
            HANDLEByReference hImpersonatedTokenRef = new HANDLEByReference();
            hToken = new HANDLE(new Pointer(cbfs.getOriginatorToken()));
            Advapi32.INSTANCE.DuplicateToken(hToken, SecurityImpersonation, hImpersonatedTokenRef);

            allow = fileSecureCheck(file.getSecurityDescriptor(), hImpersonatedTokenRef.getValue(), new DWORD(AccessRights));
            Kernel32.INSTANCE.CloseHandle(hImpersonatedTokenRef.getValue());

            if (!allow) {
                e.resultCode = ERROR_ACCESS_DENIED;
                return;
            }
        }
        catch (Exception ex)
        {
            e.resultCode = ERROR_INTERNAL_ERROR;
            return;
        }

        file.refOpen();
        if (e.fileContext == 0)
            e.fileContext = globals.alloc(file);
    }

    public void queryQuotas(CbfsQueryQuotasEvent e) {
        // out of the demo's scope
    }

    public void readFile(CbfsReadFileEvent e) {
        VirtualFile file = (VirtualFile) globals.get(e.fileContext);
        if (file == null || file.isDirectory()) {
            e.resultCode = ERROR_FILE_NOT_FOUND;
            return;
        }

        e.bytesRead = file.read(e.position, e.bytesToRead, e.buffer);
        if (e.bytesRead < 0) {
            e.resultCode = ERROR_READ_FAULT;
            e.bytesRead = 0;
        }
    }

    public void renameOrMoveFile(CbfsRenameOrMoveFileEvent e) {
        VirtualFile file = Files.get(e.fileName);
        if (file == null) {
            e.resultCode = ERROR_FILE_NOT_FOUND;
            return;
        }

        String[] oldNames = splitName(e.fileName);
        if (oldNames == null) {
            e.resultCode = ERROR_INVALID_NAME;
            return;
        }

        VirtualFile oldParent = Files.get(oldNames[0]);
        if (oldParent == null) {
            e.resultCode = ERROR_PATH_NOT_FOUND;
            return;
        }

        String[] newNames = splitName(e.newFileName);
        if (newNames == null) {
            e.resultCode = ERROR_INVALID_NAME;
            return;
        }

        VirtualFile newParent = Files.get(newNames[0]);
        if (newParent == null) {
            e.resultCode = ERROR_PATH_NOT_FOUND;
            return;
        }

        oldParent.remove(file.getName());
        file.setName(newNames[1]);
        Files.rename(e.fileName, e.newFileName);
        newParent.add(file.getName());
    }

    public void setAllocationSize(CbfsSetAllocationSizeEvent e) {
        VirtualFile file = (VirtualFile) globals.get(e.fileContext);
        if (file == null) {
            file = Files.get(e.fileName);
            if (file == null) {
                e.resultCode = ERROR_FILE_NOT_FOUND;
                return;
            }
        }

        file.setAllocationSize((int) e.allocationSize);
    }

    public void setDefaultQuotaInfo(CbfsSetDefaultQuotaInfoEvent e) {
        // out of the demo's scope
    }

    public void setFileSize(CbfsSetFileSizeEvent e) {
        VirtualFile file = (VirtualFile) globals.get(e.fileContext);
        if (file == null) {
            file = Files.get(e.fileName);
            if (file == null) {
                e.resultCode = ERROR_FILE_NOT_FOUND;
                return;
            }
        }

        file.setSize((int) e.size);
    }

    public void setFileAttributes(CbfsSetFileAttributesEvent e) {
        VirtualFile file = (VirtualFile) globals.get(e.fileContext);
        if (file == null) {
            file = Files.get(e.fileName);
            if (file == null) {
                e.resultCode = ERROR_FILE_NOT_FOUND;
                return;
            }
        }

        if (e.attributes != 0)
            file.setAttributes(e.attributes);

        if (!isNullOrEmpty(e.creationTime))
            file.setCreationTime(e.creationTime);

        if (!isNullOrEmpty(e.lastAccessTime))
            file.setLastAccessTime(e.lastAccessTime);

        if (!isNullOrEmpty(e.lastWriteTime))
            file.setLastWriteTime(e.lastWriteTime);
    }

    public void setFileSecurity(CbfsSetFileSecurityEvent e) {
        VirtualFile file = (VirtualFile) globals.get(e.fileContext);
        file.setSecurityDescriptor(e.securityInformation, e.securityDescriptor, e.length);
    }

    public void setQuotas(CbfsSetQuotasEvent e) {
        // out of the demo's scope
    }

    public void setReparsePoint(CbfsSetReparsePointEvent e) {
        // out of the demo's scope
    }

    public void setValidDataLength(CbfsSetValidDataLengthEvent e) {
        // out of the demo's scope
    }

    public void setVolumeLabel(CbfsSetVolumeLabelEvent e) {
        // not needed in this demo
    }

    public void setVolumeObjectId(CbfsSetVolumeObjectIdEvent e) {
        // out of the demo's scope
    }

    public void unlockFile(CbfsUnlockFileEvent e) {
        // not needed in this demo
    }

    public void unmount(CbfsUnmountEvent e) {
        globals.clear();
        Files.clear();
    }

    public void workerThreadCreation(CbfsWorkerThreadCreationEvent e) {
        // not needed in this demo
    }

    public void workerThreadTermination(CbfsWorkerThreadTerminationEvent e) {
        // not needed in this demo
    }

    public void writeFile(CbfsWriteFileEvent e) {
        VirtualFile file = (VirtualFile) globals.get(e.fileContext);
        if (file == null || file.isDirectory()) {
            e.resultCode = ERROR_FILE_NOT_FOUND;
            return;
        }

        e.bytesWritten = file.write(e.buffer, e.position, e.bytesToWrite);
        if (e.bytesWritten < 0) {
            e.resultCode = ERROR_WRITE_FAULT;
            e.bytesWritten = 0;
        }
    }

    // End of CbfsEventListener implementation
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

    private static class FileAttribute {
        static final int READONLY = 0x01;
        static final int HIDDEN = 0x02;
        static final int SYSTEM = 0x04;
        static final int DIRECTORY = 0x10;
        static final int ARCHIVE = 0x20;
        static final int NORMAL = 0x80;
    }

    private static class CabFileFilter extends FileFilter {

        @Override
        public boolean accept(File file) {
            if (file.isDirectory())
                return true;

            return file.getName().equalsIgnoreCase("cbfs.cab");
        }

        @Override
        public String getDescription() {
            return "CBFS Connect driver package";
        }
    }

    private static class VirtualFile {
        private int attributes;
        private String name;
        private Date creationTime;
        private Date lastAccessTime;
        private Date lastWriteTime;

        private final List<String> entries;
        private byte[] data;
        private int size;

        private Pointer securityDescriptor;
        private long securityDescriptorLength;
        private int openCount;

        VirtualFile(String name, int attributes) {
            this.name = name;
            this.attributes = attributes;
            creationTime = new Date();
            lastAccessTime = new Date();
            lastWriteTime = new Date();
            if (isDirectory())
                entries = new ArrayList<String>();
            else
                entries = null;
            data = null;
            size = 0;
            securityDescriptor = null;
            securityDescriptorLength = 0;
            openCount = 0;
        }

        boolean add(String name) {
            if (entries == null || isNullOrEmpty(name))
                return false;
            synchronized (entries) {
                String lowered = name.toLowerCase();
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

            if (securityDescriptor != null) {
                Kernel32.INSTANCE.LocalFree(securityDescriptor);
                securityDescriptor = null;
                securityDescriptorLength = 0;
            }
        }

        List<String> enumerate(String mask) {
            if (!isDirectory() || isNullOrEmpty(mask))
                return null;

            String regex = wildcardToRegex(mask);

            synchronized (entries) {
                List<String> filtered = new ArrayList<String>(entries.size());
                for (String name : entries) {
                    if (name.matches(regex))
                        filtered.add(name);
                }
                return filtered;
            }
        }

        boolean exists(String name) {
            if (entries == null || isNullOrEmpty(name))
                return false;

            synchronized (entries) {
                return entries.contains(name.toLowerCase());
            }
        }

        int getAllocationSize() {
            return (data == null) ? 0 : data.length;
        }

        int getAttributes() {
            return attributes;
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
            return size;
        }

        Pointer getSecurityDescriptor() {
            if (securityDescriptor == null) allocateDefaultSecurityDescriptor();
            return securityDescriptor;
        }

        int getSecurityDescriptor(int SecurityInformation, ByteBuffer Buffer, int BufferLength, IntByReference SecurityDescriptorLength)
        {
            boolean Result = true;
            int  ResultCode = ERROR_SUCCESS;

            if (securityDescriptor == null) allocateDefaultSecurityDescriptor();

            Pointer NewSecurityDescriptor = Kernel32.INSTANCE.LocalAlloc(LPTR, new SECURITY_DESCRIPTOR_RELATIVE().size() + 0x100);

            Advapi32Ex.INSTANCE.InitializeSecurityDescriptor(NewSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);

            do
            {
                //
                // Owner
                //
                if ((SecurityInformation & OWNER_SECURITY_INFORMATION) != 0)
                {
                    PSIDByReference Owner = new PSIDByReference();
                    BOOLByReference OwnerDefaulted = new BOOLByReference();

                    Result = Advapi32Ex.INSTANCE.GetSecurityDescriptorOwner(securityDescriptor, Owner, OwnerDefaulted);
                    if (Result == false)
                        break;

                    Result = Advapi32Ex.INSTANCE.SetSecurityDescriptorOwner(NewSecurityDescriptor, Owner.getValue(), OwnerDefaulted.getValue().booleanValue());
                    if (Result == false)
                        break;
                }

                //
                // Group
                //
                if ((SecurityInformation & GROUP_SECURITY_INFORMATION) != 0)
                {
                    PSIDByReference Group = new PSIDByReference();
                    BOOLByReference GroupDefaulted = new BOOLByReference();

                    Result = Advapi32Ex.INSTANCE.GetSecurityDescriptorGroup(securityDescriptor, Group, GroupDefaulted);
                    if (Result == false)
                        break;

                    Result = Advapi32Ex.INSTANCE.SetSecurityDescriptorGroup(NewSecurityDescriptor, Group.getValue(), GroupDefaulted.getValue().booleanValue());
                    if (Result == false)
                        break;
                }

                //
                // Dacl
                //
                if ((SecurityInformation & DACL_SECURITY_INFORMATION) != 0)
                {
                    BOOLByReference DaclPresent = new BOOLByReference();
                    PACLByReference Dacl = new PACLByReference();
                    BOOLByReference DaclDefaulted = new BOOLByReference();

                    Result = Advapi32Ex.INSTANCE.GetSecurityDescriptorDacl(securityDescriptor, DaclPresent, Dacl, DaclDefaulted);
                    if (Result == false)
                        break;

                    Result = Advapi32Ex.INSTANCE.SetSecurityDescriptorDacl(NewSecurityDescriptor, DaclPresent.getValue().booleanValue(), Dacl.getValue(), DaclDefaulted.getValue().booleanValue());
                    if (Result == false)
                        break;
                }

                //
                // Sacl
                //
                if ((SecurityInformation & SACL_SECURITY_INFORMATION) != 0)
                {
                    BOOLByReference SaclPresent = new BOOLByReference();
                    PACLByReference Sacl = new PACLByReference();
                    BOOLByReference SaclDefaulted = new BOOLByReference();

                    Result = Advapi32Ex.INSTANCE.GetSecurityDescriptorSacl(securityDescriptor, SaclPresent, Sacl, SaclDefaulted);
                    if (Result == false)
                        break;

                    Result = Advapi32Ex.INSTANCE.SetSecurityDescriptorSacl(NewSecurityDescriptor, SaclPresent.getValue().booleanValue(), Sacl.getValue(), SaclDefaulted.getValue().booleanValue());
                    if (Result == false)
                        break;
                }

                //
                // Make
                //
                {
                    Pointer SRSD;
                    IntByReference SRSDLength = new IntByReference(new Integer(0));

                    Result = Advapi32Ex.INSTANCE.MakeSelfRelativeSD(NewSecurityDescriptor, null, SRSDLength);
                    if (Result == false && Kernel32.INSTANCE.GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                        break;

                    SecurityDescriptorLength.setValue(SRSDLength.getValue());

                    if (BufferLength < SRSDLength.getValue())
                    {
                        ResultCode = ERROR_MORE_DATA;
                        Result = true;

                        break;
                    }

                    SRSD = Kernel32.INSTANCE.LocalAlloc(LPTR, SRSDLength.getValue());
                    Result = Advapi32Ex.INSTANCE.MakeSelfRelativeSD(NewSecurityDescriptor, SRSD, SRSDLength);
                    if (Result == false)
                        break;

                    Buffer.put(SRSD.getByteBuffer(0, SRSDLength.getValue()));

                    Kernel32.INSTANCE.LocalFree(SRSD);
                }

            }while (false);

            if (Result == false)
                ResultCode = Kernel32.INSTANCE.GetLastError();

            Kernel32.INSTANCE.LocalFree(NewSecurityDescriptor);

            return ResultCode;
        }

        int setSecurityDescriptor(int SecurityInformation, ByteBuffer SecurityDescriptor, int SecurityDescriptorLength)
        {
            boolean Result = true;
            int  ResultCode = ERROR_SUCCESS;

            if (securityDescriptor == null) allocateDefaultSecurityDescriptor();

            byte[] SDDataArray = new byte[SecurityDescriptorLength];
            SecurityDescriptor.get(SDDataArray);
            Pointer InputSecurityDescriptor = new Memory(SecurityDescriptorLength);
            InputSecurityDescriptor.write(0, SDDataArray, 0, SDDataArray.length);
            Pointer TargetSecurityDescriptor;
            Pointer NewSecurityDescriptor = Kernel32.INSTANCE.LocalAlloc(LPTR, new SECURITY_DESCRIPTOR_RELATIVE().size() + 0x100);

            Advapi32Ex.INSTANCE.InitializeSecurityDescriptor(NewSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
            do
            {
                //
                // Owner
                //
                {
                    PSIDByReference Owner = new PSIDByReference();
                    BOOLByReference OwnerDefaulted = new BOOLByReference();

                    if ((SecurityInformation & OWNER_SECURITY_INFORMATION) != 0)
                        TargetSecurityDescriptor = InputSecurityDescriptor;
                    else
                        TargetSecurityDescriptor = securityDescriptor;

                    Result = Advapi32Ex.INSTANCE.GetSecurityDescriptorOwner(TargetSecurityDescriptor, Owner, OwnerDefaulted);
                    if (Result == false)
                        break;

                    Result = Advapi32Ex.INSTANCE.SetSecurityDescriptorOwner(NewSecurityDescriptor, Owner.getValue(), OwnerDefaulted.getValue().booleanValue());
                    if (Result == false)
                        break;
                }


                //
                // Group
                //
                {
                    PSIDByReference Group = new PSIDByReference();
                    BOOLByReference GroupDefaulted = new BOOLByReference();

                    if ((SecurityInformation & GROUP_SECURITY_INFORMATION) != 0)
                        TargetSecurityDescriptor = InputSecurityDescriptor;
                    else
                        TargetSecurityDescriptor = securityDescriptor;

                    Result = Advapi32Ex.INSTANCE.GetSecurityDescriptorGroup(TargetSecurityDescriptor, Group, GroupDefaulted);
                    if (Result == false)
                        break;

                    Result = Advapi32Ex.INSTANCE.SetSecurityDescriptorGroup(NewSecurityDescriptor, Group.getValue(), GroupDefaulted.getValue().booleanValue());
                    if (Result == false)
                        break;
                }

                //
                // Dacl
                //
                {
                    BOOLByReference DaclPresent = new BOOLByReference();
                    PACLByReference Dacl = new PACLByReference();
                    BOOLByReference DaclDefaulted = new BOOLByReference();

                    if ((SecurityInformation & DACL_SECURITY_INFORMATION) != 0)
                        TargetSecurityDescriptor = InputSecurityDescriptor;
                    else
                        TargetSecurityDescriptor = securityDescriptor;

                    Result = Advapi32Ex.INSTANCE.GetSecurityDescriptorDacl(TargetSecurityDescriptor, DaclPresent, Dacl, DaclDefaulted);
                    if (Result == false)
                        break;

                    Result = Advapi32Ex.INSTANCE.SetSecurityDescriptorDacl(NewSecurityDescriptor, DaclPresent.getValue().booleanValue(), Dacl.getValue(), DaclDefaulted.getValue().booleanValue());
                    if (Result == false)
                        break;
                }

                //
                // Sacl
                //
                {
                    BOOLByReference SaclPresent = new BOOLByReference();
                    PACLByReference Sacl = new PACLByReference();
                    BOOLByReference SaclDefaulted = new BOOLByReference();

                    if ((SecurityInformation & SACL_SECURITY_INFORMATION) != 0)
                        TargetSecurityDescriptor = InputSecurityDescriptor;
                    else
                        TargetSecurityDescriptor = securityDescriptor;

                    Result = Advapi32Ex.INSTANCE.GetSecurityDescriptorSacl(TargetSecurityDescriptor, SaclPresent, Sacl, SaclDefaulted);
                    if (Result == false)
                        break;

                    Result = Advapi32Ex.INSTANCE.SetSecurityDescriptorSacl(NewSecurityDescriptor, SaclPresent.getValue().booleanValue(), Sacl.getValue(), SaclDefaulted.getValue().booleanValue());
                    if (Result == false)
                        break;
                }

                //
                // Make
                //
                {
                    Pointer SRSD;
                    IntByReference SRSDLength = new IntByReference(new Integer(0));

                    Result = Advapi32Ex.INSTANCE.MakeSelfRelativeSD(NewSecurityDescriptor, null, SRSDLength);
                    if (Result == false && Kernel32.INSTANCE.GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                        break;

                    SRSD = Kernel32.INSTANCE.LocalAlloc(LPTR, SRSDLength.getValue());
                    Result = Advapi32Ex.INSTANCE.MakeSelfRelativeSD(NewSecurityDescriptor, SRSD, SRSDLength);
                    if (Result == false)
                        break;

                    Kernel32.INSTANCE.LocalFree(securityDescriptor);
                    securityDescriptor = SRSD;
                    securityDescriptorLength = SRSDLength.getValue();
                }

            }while (false);

            if (Result == false)
                ResultCode = Kernel32.INSTANCE.GetLastError();

            Kernel32.INSTANCE.LocalFree(NewSecurityDescriptor);

            return ResultCode;
        }

        int refOpen()
        {
            ++openCount;
            return openCount;
        }

        int derOpen()
        {
            --openCount;
            return openCount;
        }

        private String getCurrentUserSid() {
            HANDLE AccessToken = new HANDLE();
            HANDLEByReference AccessTokenRef = new HANDLEByReference(AccessToken);
            TOKEN_USER UserToken = new TOKEN_USER(1024);
            PointerByReference StringSid = new PointerByReference();
            IntByReference InfoSize = new IntByReference(0);

            if (!Advapi32.INSTANCE.OpenThreadToken(Kernel32.INSTANCE.GetCurrentThread(), TOKEN_QUERY, true, AccessTokenRef)) {
                if (Kernel32.INSTANCE.GetLastError() == ERROR_NO_TOKEN) {
                    Advapi32.INSTANCE.OpenProcessToken(Kernel32.INSTANCE.GetCurrentProcess(), TOKEN_QUERY, AccessTokenRef);
                }
            }

            Advapi32.INSTANCE.GetTokenInformation(AccessTokenRef.getValue(), TokenUser, UserToken, 1024, InfoSize);

            Advapi32.INSTANCE.ConvertSidToStringSid(UserToken.User.Sid, StringSid);

            Kernel32.INSTANCE.CloseHandle(AccessToken);

            Pointer ptr = StringSid.getValue();
            try {
                return ptr.getWideString(0);
            } finally {
                Kernel32Util.freeLocalMemory(ptr);
            }
        }

        interface Advapi32Ex extends StdCallLibrary {
            Advapi32Ex INSTANCE = Native.load("Advapi32", Advapi32Ex.class, W32APIOptions.DEFAULT_OPTIONS);
            boolean ConvertStringSecurityDescriptorToSecurityDescriptor(String var1, int var2, PointerByReference var3, ULONGByReference var4);
            boolean GetSecurityDescriptorOwner(Pointer var1, PSIDByReference var2, BOOLByReference var3);
            boolean SetSecurityDescriptorOwner(Pointer var1, PSID var2, boolean var3);
            boolean GetSecurityDescriptorGroup(Pointer var1, PSIDByReference var2, BOOLByReference var3);
            boolean SetSecurityDescriptorGroup(Pointer var1, PSID var2, boolean var3);
            boolean GetSecurityDescriptorDacl(Pointer var1, BOOLByReference var2, PACLByReference var3, BOOLByReference var4);
            boolean SetSecurityDescriptorDacl(Pointer var1, boolean var2, ACL var3, boolean var4);
            boolean SetSecurityDescriptorSacl(Pointer var1, boolean var2, ACL var3, boolean var4);
            boolean GetSecurityDescriptorSacl(Pointer var1, BOOLByReference var2, PACLByReference var3, BOOLByReference var4);
            boolean MakeSelfRelativeSD(Pointer var1, Pointer var2, IntByReference var3);
            boolean InitializeSecurityDescriptor(Pointer var1, int var2);
        }

        void allocateDefaultSecurityDescriptor()
        {
            if (securityDescriptor != null) {
                Kernel32.INSTANCE.LocalFree(securityDescriptor);
                securityDescriptor = null;
                securityDescriptorLength = 0;
            }

            //
            // Format
            // O : owner_sid, SID string: https://docs.microsoft.com/en-us/windows/win32/secauthz/sid-strings
            // G : group_sid, SID string: https://docs.microsoft.com/en-us/windows/win32/secauthz/sid-strings
            // D : dacl_flags(string_ace1)(string_ace2)... (string_acen), ACE string: https://docs.microsoft.com/en-us/windows/win32/secauthz/ace-strings
            // S : sacl_flags(string_ace1)(string_ace2)... (string_acen), ACE string: https://docs.microsoft.com/en-us/windows/win32/secauthz/ace-strings
            //
            // ACE string syntax: ace_type;ace_flags;rights;object_guid;inherit_object_guid;account_sid;(resource_attribute)
            //                    https://docs.microsoft.com/en-us/windows/win32/secauthz/ace-strings
            //

            String CurentUserSid = getCurrentUserSid();
            String StringSecurityDescriptor;
            int  CUAccessRights, AUAccessRights, BAAccessRights;

            //
            // Current User
            //
            CUAccessRights = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;

            //
            // Authenticated User
            //
            AUAccessRights = FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE | DELETE;

            //
            // Administrator User
            //
            BAAccessRights = FILE_ALL_ACCESS;


            //
            // ------------------- File ----------------
            // Owner: Current User
            // Group: Current User
            // -----------------------------------------
            //
            if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
                StringSecurityDescriptor = String.format(
                        "O:%sG:%sD:(A;;0x%X;;;%s)(A;;0x%X;;;AU)(A;;0x%X;;;BA)",
                        CurentUserSid, CurentUserSid, CUAccessRights, CurentUserSid, AUAccessRights, BAAccessRights);
                //
                // ------------------ Directory -------------
                // Owner: Current User
                // Group: Current User
                // ------------------------------------------
                //
            else
                StringSecurityDescriptor = String.format(
                        "O:%sG:%sD:(A;OICI;0x%X;;;%s)(A;OICI;0x%X;;;AU)(A;OICI;0x%X;;;BA)",
                        CurentUserSid, CurentUserSid, CUAccessRights, CurentUserSid, AUAccessRights, BAAccessRights);

            ULONGByReference SecurityDescriptorLength = new ULONGByReference(new ULONG(0));
            PointerByReference SecurityDescriptor = new PointerByReference();
            if (Advapi32Ex.INSTANCE.ConvertStringSecurityDescriptorToSecurityDescriptor(StringSecurityDescriptor,
                    SECURITY_DESCRIPTOR_REVISION,
                    SecurityDescriptor,
                    SecurityDescriptorLength)) {
                securityDescriptor = SecurityDescriptor.getValue();
                securityDescriptorLength = SecurityDescriptorLength.getValue().longValue();

                assert (Advapi32.INSTANCE.IsValidSecurityDescriptor(securityDescriptor));
            }

        }

        boolean isDirectory() {
            return (attributes & FileAttribute.DIRECTORY) != 0;
        }

        boolean isEmpty() {
            if (entries != null)
                synchronized (entries) {
                    return (entries.size() == 0);
                }

            return (size == 0);
        }

        long read(long position, long bytesToRead, ByteBuffer buffer) {
            if (position < 0 || position >= size)
                return -1;

            int count = (int) Math.min(bytesToRead, size - position);
            if (count != 0)
                buffer.put(data, (int) position, count);
            return count;
        }

        void remove(String name) {
            if (entries == null || isNullOrEmpty(name))
                return;
            synchronized (entries) {
                String lowered = name.toLowerCase();
                if (!entries.contains(lowered))
                    return;
                entries.remove(lowered);
            }
        }

        void setAllocationSize(int newSize) {
            if (isDirectory() || newSize == getAllocationSize())
                return;

            if (data == null || data.length == 0)
                data = new byte[(int) newSize];
            else {
                byte[] newData = new byte[newSize];
                System.arraycopy(data, 0, newData, 0, Math.min(data.length, newSize));
                if (size > newSize)
                    size = newSize;
                Arrays.fill(data, (byte) 0);
                data = newData;
            }
        }

        void setAttributes(int newAttributes) {
            this.attributes = newAttributes;
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
            if (isDirectory() || size == newSize)
                return;

            if (newSize < size)
                size = newSize;
            else {
                if (newSize <= getAllocationSize())
                    size = newSize;
                else
                    setAllocationSize(newSize);
            }
        }

        void setName(String newName) {
            this.name = newName;
        }

        private String wildcardToRegex(String wildcard){
            StringBuilder s = new StringBuilder(wildcard.length());
            s.append('^');
            for (int i = 0, len = wildcard.length(); i < len; i++) {
                char c = wildcard.charAt(i);
                switch(c) {
                    case '*':
                        s.append(".*");
                        break;
                    case '?':
                        s.append(".");
                        break;
                    case '^': // escape character in cmd.exe
                        s.append("\\");
                        break;
                    // escape special regexp-characters
                    case '(': case ')': case '[': case ']': case '$':
                    case '.': case '{': case '}': case '|':
                    case '\\':
                        s.append("\\");
                        s.append(c);
                        break;
                    default:
                        s.append(c);
                        break;
                }
            }
            s.append('$');
            return s.toString();
        }

        long write(ByteBuffer buffer, long position, long bytesToWrite) {
            if (position < 0 || position >= size)
                return -1;

            int count = (int) Math.min(bytesToWrite, size - position);
            buffer.get(data, (int) position, count);
            return count;
        }
    }

    private static class EnumContext {
        private List<String> entries;
        private int index;
        private String parent;

        EnumContext(String directory, List<String> entries) {
            this.parent = directory.toLowerCase();
            index = 0;
            this.entries = entries;
        }

        String getNext() {
            if (entries == null || entries.size() == 0)
                return null;

            for (int i = index; i < entries.size(); i++) {
                String name = combineNames(parent, entries.get(i));
                VirtualFile file = Files.get(name);
                if (file == null)
                    continue;
                index = i + 1;
                return name;
            }

            return null;
        }
    }

    private static class Files {
        private static final Map<String, VirtualFile> entries;

        static {
            entries = new HashMap<String, VirtualFile>();
            entries.put("\\", new VirtualFile("", FileAttribute.DIRECTORY));
        }

        static void add(String name, VirtualFile file) {
            if (isNullOrEmpty(name) || file == null)
                return;
            synchronized (entries) {
                entries.put(name.toLowerCase(), file);
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

                VirtualFile parent = entries.get(names[0].toLowerCase());
                if (parent != null)
                    parent.remove(names[1]);

                VirtualFile file = entries.remove(name.toLowerCase());
                if (file != null)
                    file.clear();
            }
        }

        static VirtualFile get(String name) {
            if (isNullOrEmpty(name))
                return null;
            synchronized (entries) {
                return entries.get(name.toLowerCase());
            }
        }

        static void rename(String name, String newName) {
            if (entries == null)
                return;

            VirtualFile file = entries.remove(name.toLowerCase());
            if (file == null)
                return;
            entries.put(newName.toLowerCase(), file);
        }
    }

    /**
     * The storage for global objects (such as contexts) which are returned to a driver
     * and must not be processed by GC until explicitly released.
     */
    private static class globals {
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
}




